#include "auto_update.h"

#ifdef USE_ESP32

#include "esphome/components/network/util.h"
#include "esphome/components/storage/store.h"
#include "esphome/components/storage/util.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_UDPSERVER
#include "esphome/components/udp_server/udp_server.h"
#endif

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_system.h>
#include <mbedtls/md.h>

#include <cstdio>
#include <cstring>

namespace esphome {
namespace auto_update {

static const char *const TAG = "auto_update";

AutoUpdateComponent *global_auto_update = nullptr;

/// Bumped whenever AutoUpdateState changes shape, so an old record is discarded
/// rather than reinterpreted - a garbage last_attempt_day would either stall the
/// fleet for a day or let it retry in a loop.
static constexpr uint32_t AUTO_UPDATE_PREF_HASH = 99991151UL;

/// Only checked this often; the window is two hours wide and the slot has second
/// resolution, so there is nothing to gain from looking more often than this.
static constexpr uint32_t TICK_INTERVAL_MS = 20000;
/// How long the boot verdict waits between attempts to reach the cloud.
static constexpr uint32_t VERDICT_RETRY_MS = 30000;
/// After this long the verdict is given up on. The board still has the
/// firmware.autoupdate.offered row from the check, so the night is not lost -
/// but holding the report forever would mean reporting it days later, attached
/// to nothing.
static constexpr uint32_t VERDICT_GIVE_UP_MS = 30 * 60 * 1000UL;

/// Copies into a fixed char field, always null terminated. A version that no
/// longer fits would compare unequal on the next boot and turn a successful
/// update into a reported failure, so the fields are sized with room to spare.
static void copy_field(char *field, size_t size, const std::string &value) {
  const size_t length = value.size() < size - 1 ? value.size() : size - 1;
  memcpy(field, value.c_str(), length);
  field[length] = 0;
}

void AutoUpdateComponent::setup() {
  global_auto_update = this;

  this->pref_ = global_preferences->make_preference<AutoUpdateState>(AUTO_UPDATE_PREF_HASH, true);
  if (!this->pref_.load(&this->state_)) {
    this->state_ = AutoUpdateState{};
  }

  if (this->state_.phase == AUTO_UPDATE_PHASE_SELF_FLASHING) {
    // The previous boot was in the middle of installing an image. Whether that
    // worked is decided by what this boot is actually running, not by anything
    // the download reported before it rebooted.
    const std::string expected = this->state_.pending_version;
    const std::string running = this->core_ != nullptr ? this->core_->firmware_version() : std::string();
    this->boot_verdict_success_ = !expected.empty() && expected == running;
    this->boot_verdict_version_ = expected;
    this->boot_verdict_file_id_ = this->state_.pending_file_id;
    this->boot_verdict_release_id_ = this->state_.pending_release_id;
    this->boot_verdict_file_size_ = this->state_.pending_file_size;
    this->boot_verdict_pending_ = true;
    this->boot_verdict_next_try_ms_ = millis() + 5000;

    if (this->boot_verdict_success_) {
      ESP_LOGI(TAG, "Self update to %s completed", expected.c_str());
      this->last_result_ = "installed " + expected;
    } else {
      ESP_LOGE(TAG, "Self update to %s did not take; this build reports %s", expected.c_str(), running.c_str());
      this->last_result_ = "failed installing " + expected;
    }

    // The day guard stays as it was written before the download. Clearing the
    // phase only stops the next boot from reporting the same verdict again.
    this->state_.phase = AUTO_UPDATE_PHASE_IDLE;
    this->save_state_();
  }

  if (this->board_url_.empty() || this->promoss_secret_.empty()) {
    // Not a silent no-op: a build that cannot reach the board will never update
    // itself again, and that has to be visible in the boot log rather than being
    // discovered months later on a fleet nobody noticed had gone stale.
    ESP_LOGE(TAG, "Auto update is compiled in but not configured (board_url or promoss_secret is empty); this device will never self-update");
    this->last_result_ = "not configured";
    this->enabled_ = false;
    this->status_set_error();
  }
}

void AutoUpdateComponent::loop() {
  const uint32_t now = millis();

  if (this->boot_verdict_pending_ && now >= this->boot_verdict_next_try_ms_) {
    this->report_boot_verdict_();
  }

  if (!this->enabled_ || this->core_ == nullptr || this->time_ == nullptr)
    return;
  if (now - this->last_tick_ms_ < TICK_INTERVAL_MS)
    return;
  this->last_tick_ms_ = now;

  if (!network::is_connected())
    return;

  const ESPTime local = this->time_->now();
  if (!local.is_valid())
    return;

  const uint32_t day = day_number_(local);
  if (this->state_.last_attempt_day == day)
    return;

  const uint32_t second_of_day = local.hour * 3600UL + local.minute * 60UL + local.second;
  const uint32_t window_end = static_cast<uint32_t>(this->window_end_hour_) * 3600UL;
  // A device that was switched off at its own slot still updates if it is back
  // before the window closes; only the day guard keeps it to one attempt.
  if (second_of_day < this->slot_seconds() || second_of_day >= window_end)
    return;

  this->run_check_(true);
}

void AutoUpdateComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Auto update:");
  ESP_LOGCONFIG(TAG, "  Enabled: %s", YESNO(this->enabled_));
  ESP_LOGCONFIG(TAG, "  Board URL: %s", this->board_url_.c_str());
  ESP_LOGCONFIG(TAG, "  Channel: %s", this->channel_.c_str());
  ESP_LOGCONFIG(TAG, "  Window: %02u:00-%02u:00 local", this->window_start_hour_, this->window_end_hour_);
  const uint32_t slot = this->slot_seconds();
  ESP_LOGCONFIG(TAG, "  This device checks at %02u:%02u:%02u", slot / 3600, (slot % 3600) / 60, slot % 60);
  ESP_LOGCONFIG(TAG, "  Last attempt day: %u", this->state_.last_attempt_day);
  ESP_LOGCONFIG(TAG, "  Last result: %s", this->last_result_.c_str());
}

uint32_t AutoUpdateComponent::slot_seconds() const {
  uint8_t mac[6];
  get_mac_address_raw(mac);

  // FNV-1a over the MAC. Deterministic on purpose: a slot re-rolled at every
  // boot would let a device that reboots inside the window get a second attempt,
  // which is exactly what the day guard exists to prevent.
  uint32_t hash = 2166136261UL;
  for (uint8_t byte : mac) {
    hash ^= byte;
    hash *= 16777619UL;
  }

  const uint32_t start = static_cast<uint32_t>(this->window_start_hour_) * 3600UL;
  const uint32_t end = static_cast<uint32_t>(this->window_end_hour_) * 3600UL;
  const uint32_t span = end > start ? end - start : 3600UL;
  return start + (hash % span);
}

bool AutoUpdateComponent::run_now() { return this->run_check_(false); }

bool AutoUpdateComponent::run_check_(bool respect_day_guard) {
  if (this->core_ == nullptr) {
    ESP_LOGE(TAG, "No api_core to run an update through");
    return false;
  }
  if (this->board_url_.empty()) {
    ESP_LOGE(TAG, "No board URL configured");
    return false;
  }

  if (respect_day_guard) {
    const ESPTime local = this->time_ != nullptr ? this->time_->now() : ESPTime{};
    if (!local.is_valid())
      return false;
    // Written before a single byte is downloaded. Everything after this point
    // can crash, brown out or reboot the device, and tomorrow is still the
    // earliest it will try again.
    this->mark_attempt_(day_number_(local));
  }

  const std::string nonce = this->random_nonce_();
  const std::string request_body = this->build_request_body_(nonce);
  std::string response_body;
  if (!this->fetch_plan_(request_body, response_body)) {
    return false;
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, response_body);
  if (error) {
    ESP_LOGE(TAG, "Update plan is not valid JSON: %s", error.c_str());
    this->last_result_ = "bad plan json";
    return false;
  }

  JsonObject root = doc.as<JsonObject>();
  if (root["nonce"].as<std::string>() != nonce) {
    // The signature already covers the body, so this only catches a stale answer
    // rather than a forged one - but a stale answer names a firmware too.
    ESP_LOGE(TAG, "Update plan answers a different request");
    this->last_result_ = "nonce mismatch";
    return false;
  }

  this->apply_plan_(root);
  return true;
}

bool AutoUpdateComponent::fetch_plan_(const std::string &request_body, std::string &response_body) {
  const bool secure = this->board_url_.compare(0, 6, "https:") == 0;
  WiFiClientSecure secure_client;
  WiFiClient plain_client;
  if (secure) {
    // Same trade as the OTA download in http_update: pinning a CA into the fleet
    // means a certificate rotation takes the update path down with no way to
    // ship the fix. What makes that safe here is the signature on the answer -
    // it is checked below before anything in it is acted on.
    secure_client.setInsecure();
    secure_client.setTimeout(this->timeout_ms_ / 1000);
  }

  HTTPClient http;
  const std::string url = this->board_url_ + "/api/device/v1/firmware/check";
  const bool begun = secure ? http.begin(secure_client, url.c_str()) : http.begin(plain_client, url.c_str());
  if (!begun) {
    ESP_LOGE(TAG, "Cannot open %s", url.c_str());
    this->last_result_ = "connect failed";
    return false;
  }

  http.setTimeout(this->timeout_ms_);
  http.addHeader("content-type", "application/json");
  http.addHeader("x-gsmart-key-id", this->device_mac_().c_str());
  http.addHeader("x-gsmart-signature", this->hmac_sha256_hex_(this->derived_device_secret_(), request_body).c_str());
  const char *collect[] = {"x-gsmart-signature"};
  http.collectHeaders(collect, 1);

  const int status = http.POST(reinterpret_cast<uint8_t *>(const_cast<char *>(request_body.c_str())),
                               request_body.size());
  if (status != 200) {
    ESP_LOGE(TAG, "Update check answered %d", status);
    this->last_result_ = str_sprintf("http %d", status);
    http.end();
    return false;
  }

  response_body = http.getString().c_str();
  const std::string signature = http.header("x-gsmart-signature").c_str();
  http.end();

  const std::string expected = this->hmac_sha256_hex_(this->derived_device_secret_(), response_body);
  if (signature.empty() || signature != expected) {
    // The one check that makes an answer received over setInsecure() safe to
    // act on. Without it, anything sitting in front of this connection could
    // name a firmware image of its own choosing and the md5 further down would
    // confirm it faithfully.
    ESP_LOGE(TAG, "Update plan signature does not verify; ignoring it");
    this->last_result_ = "bad plan signature";
    return false;
  }
  return true;
}

void AutoUpdateComponent::apply_plan_(JsonObject root) {
  JsonArray updates = root["updates"].as<JsonArray>();
  if (updates.isNull() || updates.size() == 0) {
    ESP_LOGI(TAG, "Nothing to install");
    this->last_result_ = "up to date";
    return;
  }

  // The board puts region members before this device's own update, because a
  // self update reboots and everything after it in this loop would be dropped.
  // The order is the board's to decide; this only has to not reorder it.
  for (JsonObject update : updates) {
    JsonObject command = update["command"].as<JsonObject>();
    if (command.isNull())
      continue;

    api_core_v1::FirmwareUpdatePlan plan = api_core_v1::parse_firmware_update_plan(command);
    plan.trigger = "auto";
    const std::string role = update["role"].as<std::string>();

    if (role == "self") {
      if (plan.url.empty()) {
        ESP_LOGE(TAG, "Self update has no url");
        continue;
      }
      ESP_LOGI(TAG, "Installing %s on this device", plan.version.c_str());

      // Persisted before the download, so that a device which never comes back
      // still leaves behind what it was installing.
      this->state_.phase = AUTO_UPDATE_PHASE_SELF_FLASHING;
      copy_field(this->state_.pending_version, sizeof(this->state_.pending_version), plan.version);
      copy_field(this->state_.pending_file_id, sizeof(this->state_.pending_file_id), plan.file_id);
      copy_field(this->state_.pending_release_id, sizeof(this->state_.pending_release_id), plan.release_id);
      this->state_.pending_file_size = plan.file_size;
      this->save_state_();

      this->last_result_ = "installing " + plan.version;
      this->core_->start_self_firmware_update(plan, 0);
      // Whatever happens next, nothing else in this plan can be trusted to run:
      // a working install never returns from the line above.
      return;
    }

    if (role == "master") {
      ESP_LOGI(TAG, "Relaying %s to %s at %s", plan.version.c_str(), plan.target_serial.c_str(),
               plan.target_ip.c_str());
      const bool ok = this->core_->push_firmware_to_member(plan);
      ESP_LOGI(TAG, "Relay to %s %s", plan.target_serial.c_str(), ok ? "completed" : "failed");
      this->last_result_ = (ok ? "relayed to " : "relay failed for ") + plan.target_serial;
      continue;
    }

    ESP_LOGW(TAG, "Unknown update role %s", role.c_str());
  }
}

void AutoUpdateComponent::report_boot_verdict_() {
  if (!this->boot_verdict_pending_ || this->core_ == nullptr)
    return;

  if (millis() > VERDICT_GIVE_UP_MS) {
    ESP_LOGW(TAG, "Giving up on reporting the last update; the cloud link never came up");
    this->boot_verdict_pending_ = false;
    return;
  }

  api_core_v1::FirmwareUpdatePlan plan;
  plan.version = this->boot_verdict_version_;
  plan.file_id = this->boot_verdict_file_id_;
  plan.release_id = this->boot_verdict_release_id_;
  plan.file_size = this->boot_verdict_file_size_;
  plan.target_serial = this->device_serial_();
  plan.trigger = "auto";

  const bool sent = this->boot_verdict_success_
                        ? this->core_->report_firmware_event("completed", "self", plan, plan.file_size)
                        : this->core_->report_firmware_event("failed", "self", plan, 0, "restarted_during_update");
  if (sent) {
    ESP_LOGI(TAG, "Reported the %s update to %s", this->boot_verdict_success_ ? "completed" : "failed",
             this->boot_verdict_version_.c_str());
    this->boot_verdict_pending_ = false;
    return;
  }
  this->boot_verdict_next_try_ms_ = millis() + VERDICT_RETRY_MS;
}

void AutoUpdateComponent::mark_attempt_(uint32_t day) {
  this->state_.last_attempt_day = day;
  this->state_.phase = AUTO_UPDATE_PHASE_IDLE;
  this->save_state_();
}

void AutoUpdateComponent::save_state_() {
  this->pref_.save(&this->state_);
  // sync(), not just save(): save() only queues the write, and the whole point
  // of these two fields is to survive a reboot that arrives without warning.
  if (global_preferences != nullptr)
    global_preferences->sync();
}

std::string AutoUpdateComponent::build_request_body_(const std::string &nonce) const {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["serial"] = this->device_serial_();
  root["mac"] = this->device_mac_();
  root["model"] = this->device_model_();
  // Exactly what the heartbeat reports, so the board compares this against the
  // lcFwVer it already stores rather than against a second, subtly different
  // spelling of the same build.
  root["version"] = this->core_ != nullptr ? this->core_->firmware_version() : std::string();
  root["channel"] = this->channel_;
  root["nonce"] = nonce;
  // Epoch from the RTC, not from ::time - `time` here is the esphome::time
  // namespace this component already pulls in. The board refuses a request whose
  // ts drifts more than ten minutes, which is what stops a captured one from
  // being replayed the next night.
  root["ts"] = this->time_ != nullptr ? static_cast<uint32_t>(this->time_->now().timestamp) : 0;
  JsonArray members = root["members"].to<JsonArray>();
  this->collect_members_(members);

  std::string out;
  serializeJson(doc, out);
  return out;
}

void AutoUpdateComponent::collect_members_(JsonArray members) const {
// The same three conditions ApiCoreV1::push_firmware_to_member is compiled
// under. Reporting members on a build that cannot relay - the panel keeps no
// GlobalDevices map at all, only GSMART_EMITTER builds do - would have the board
// plan pushes this device could never carry out.
#if defined(USE_GSMART_OTA_PUSH) && defined(GSMART_EMITTER) && defined(USE_UDPSERVER) && defined(GSMART_FEATURE_REGION)
  if (udp_server::udpServer == nullptr || storage::store == nullptr || storage::store->region == nullptr)
    return;
  // Only a master relays. A member reporting its neighbours would be asking the
  // board to let it flash devices it has no business touching - the board
  // refuses that too, but there is no reason to ask.
  if (!storage::store->region->isRegionActive() || !storage::store->region->isMaster())
    return;

  const uint64_t region_serial = storage::store->region->layout.serial;
  for (size_t i = 0; i < udp_server::udpServer->GlobalDevices.ItemsCount; i++) {
    auto *item = udp_server::udpServer->GlobalDevices.Items[i];
    if (item == nullptr || item->region_id != region_serial)
      continue;

    JsonObject member = members.add<JsonObject>();
    member["mac"] = storage::convertMacToStr(item->mac);
    member["ip"] = str_sprintf("%u.%u.%u.%u", item->ip[0], item->ip[1], item->ip[2], item->ip[3]);
    member["model"] = storage::convertModelToStr(item->model);
    member["modelNum"] = item->model;
    // The live build of that node, straight off the LAN. The board uses it to
    // avoid re-pushing an image a member already runs but has not managed to
    // report to the cloud.
    member["build"] = str_sprintf("%u.%u", item->build[0], item->build[1]);
  }
#else
  (void) members;
#endif
}

std::string AutoUpdateComponent::device_mac_() const {
  uint8_t mac[6];
  get_mac_address_raw(mac);
  std::string value = storage::convertMacToStr(mac);
  for (char &ch : value)
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return value;
}

std::string AutoUpdateComponent::device_serial_() const {
  if (storage::store != nullptr)
    return storage::store->get_serial();
  return get_mac_address().substr(6);
}

std::string AutoUpdateComponent::device_model_() const {
  if (storage::store != nullptr)
    return storage::store->get_model();
  return App.get_name();
}

std::string AutoUpdateComponent::derived_device_secret_() const {
  return this->hmac_sha256_hex_(this->promoss_secret_, this->device_mac_());
}

std::string AutoUpdateComponent::hmac_sha256_hex_(const std::string &key, const std::string &message) const {
  uint8_t digest[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, reinterpret_cast<const unsigned char *>(key.c_str()), key.size());
  mbedtls_md_hmac_update(&ctx, reinterpret_cast<const unsigned char *>(message.c_str()), message.size());
  mbedtls_md_hmac_finish(&ctx, digest);
  mbedtls_md_free(&ctx);

  char hex[65];
  for (size_t i = 0; i < sizeof(digest); i++)
    std::snprintf(hex + (i * 2), 3, "%02x", digest[i]);
  hex[64] = 0;
  return hex;
}

std::string AutoUpdateComponent::random_nonce_() const {
  static const char *hex = "0123456789abcdef";
  std::string out;
  out.reserve(32);
  for (size_t i = 0; i < 16; i++) {
    const uint8_t value = static_cast<uint8_t>(esp_random() & 0xff);
    out.push_back(hex[value >> 4]);
    out.push_back(hex[value & 0x0f]);
  }
  return out;
}

uint32_t AutoUpdateComponent::day_number_(const ESPTime &now) {
  return static_cast<uint32_t>(now.year) * 10000UL + static_cast<uint32_t>(now.month) * 100UL +
         static_cast<uint32_t>(now.day_of_month);
}

}  // namespace auto_update
}  // namespace esphome

#endif  // USE_ESP32
