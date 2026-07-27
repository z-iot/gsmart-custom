#include "gsmart_wifi_manager.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/components/network/ip_address.h"
#include "esphome/components/storage/store.h"

#include <algorithm>
#include <cstring>

#ifdef USE_ESP32
#include <esp_err.h>
#include <esp_wifi.h>
#endif

#ifdef USE_ESP8266
#include <ESP8266WiFi.h>
#endif

namespace esphome {
namespace gsmart_wifi_manager {

static const char *const TAG = "gsmart_wifi_manager";
static constexpr uint32_t WIFI_SETTINGS_CLIENT_PREF_ID = 99991201UL;
static constexpr uint32_t WIFI_SETTINGS_AP_PREF_ID     = 99991202UL;
static constexpr uint32_t WIFI_SETTINGS_CLOUD_PREF_ID  = 99991203UL;
static constexpr uint32_t WIFI_SETTINGS_SERVICE_AP_PREF_ID = 99991204UL;
static constexpr uint32_t WIFI_SETTINGS_MAGIC           = 0x47535746UL;  // GSWF
static constexpr uint8_t  WIFI_SETTINGS_CLIENT_VERSION  = 2;
static constexpr uint8_t  WIFI_SETTINGS_AP_VERSION      = 3;
static constexpr uint8_t  WIFI_SETTINGS_CLOUD_VERSION   = 1;
static constexpr uint8_t  WIFI_SETTINGS_SERVICE_AP_VERSION = 2;
static constexpr uint32_t SCAN_INTERVAL_MS = 60000UL;
static constexpr uint32_t SCAN_POLL_TIMEOUT_MS = 12000UL;
static constexpr uint32_t SOFT_AP_REAPPLY_INTERVAL_MS = 5000UL;
static constexpr int32_t SERVICE_AP_OLD_DEFAULT_TIMEOUT_MIN = 60;
static constexpr int32_t SERVICE_AP_DEFAULT_TIMEOUT_MIN = 15;
static constexpr uint32_t SERVICE_AP_MAX_AUTO_OFF_SEC = 2147483UL;
static constexpr const char *SERVICE_AP_AUTO_OFF_TIMEOUT = "service_ap_auto_off";

static constexpr const char *SERVICE_DEFAULT_SSID = "GSmartService-HS";
static constexpr const char *SERVICE_DEFAULT_PWD  = "smart8888";

GsmartWifiManager *global_gsmart_wifi_manager = nullptr;

GsmartWifiManager::GsmartWifiManager() { global_gsmart_wifi_manager = this; }

void GsmartWifiManager::setup() {
  ESP_LOGCONFIG(TAG, "Setting up GSmart Wi-Fi Manager...");
  this->load_settings();
  this->apply_service_ap_startup_policy_();
#ifdef USE_WIFI_SCAN_RESULTS_LISTENERS
  if (wifi::global_wifi_component != nullptr)
    wifi::global_wifi_component->add_scan_results_listener(this);
#endif
  this->update_sta_priority();
  this->apply_wifi_state();
  if (this->service_ap_runtime_active_)
    this->schedule_service_ap_auto_off_(this->timeout_min_to_seconds_(this->service_ap_settings_.startup_timeout_min));
  this->reconnect_sta_();
}

void GsmartWifiManager::loop() {
  const uint32_t now = millis();

  if (this->should_periodic_scan_() && !this->scan_pending_ && now - this->last_scan_time_ > SCAN_INTERVAL_MS)
    this->start_scan(false);

  if (this->scan_pending_)
    this->check_scan_results();

  this->ensure_soft_ap_state_();
}

void GsmartWifiManager::dump_config() {
  ESP_LOGCONFIG(TAG, "GSmart Wi-Fi Manager:");
  ESP_LOGCONFIG(TAG, "  Manufacture networks: %u", static_cast<unsigned>(this->manufacture_networks_.size()));
  ESP_LOGCONFIG(TAG, "  Service SSID: %s", this->client_settings_.service_ssid);
  ESP_LOGCONFIG(TAG, "  Customer Primary SSID: %s", this->client_settings_.customer_primary_ssid);
  ESP_LOGCONFIG(TAG, "  Customer Secondary SSID: %s", this->client_settings_.customer_secondary_ssid);
  ESP_LOGCONFIG(TAG, "  Service AP: Gsmart-<serial> (%s)", this->ap_settings_.service_ap_mode ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  Service AP startup timeout: %d min", this->service_ap_settings_.startup_timeout_min);
  ESP_LOGCONFIG(TAG, "  Service AP manual timeout: %d min", this->service_ap_settings_.manual_timeout_min);
  ESP_LOGCONFIG(TAG, "  Region AP: %s (%s, channel: %u)", this->ap_settings_.region_ap_ssid,
                this->ap_settings_.region_ap_mode ? "enabled" : "disabled",
                this->ap_settings_.region_ap_channel);
  ESP_LOGCONFIG(TAG, "  STA mode: %u  Service mode: %s", this->client_settings_.sta_mode,
                this->client_settings_.service_mode ? "on" : "off");
  ESP_LOGCONFIG(TAG, "  Cloud mode: %u", this->cloud_settings_.cloud_mode);
}

void GsmartWifiManager::add_manufacture_network(const std::string &ssid, const std::string &password) {
  if (!ssid.empty())
    this->manufacture_networks_.push_back({ssid, password});
}

void GsmartWifiManager::copy_string_(char *dest, size_t size, const std::string &value, bool keep_if_empty) {
  if (size == 0 || (keep_if_empty && value.empty()))
    return;
  std::strncpy(dest, value.c_str(), size - 1);
  dest[size - 1] = '\0';
}

void GsmartWifiManager::log_client_settings_(const char *prefix) const {
  ESP_LOGI(TAG,
           "%s client_loaded=%s primary_ssid='%s' primary_password_set=%s secondary_ssid='%s' "
           "secondary_password_set=%s sta_mode=%u service_ssid='%s' service_mode=%u",
           prefix, this->persistence_status_.client_loaded ? "true" : "false",
           this->client_settings_.customer_primary_ssid,
           this->client_settings_.customer_primary_password[0] != '\0' ? "true" : "false",
           this->client_settings_.customer_secondary_ssid,
           this->client_settings_.customer_secondary_password[0] != '\0' ? "true" : "false",
           this->client_settings_.sta_mode, this->client_settings_.service_ssid,
           this->client_settings_.service_mode);
}

void GsmartWifiManager::set_sta_service(const std::string &ssid, const std::string &password, uint8_t mode) {
  this->client_settings_.service_mode = mode;
  this->copy_string_(this->client_settings_.service_ssid, sizeof(this->client_settings_.service_ssid), ssid,
                     ssid.empty());
  this->copy_string_(this->client_settings_.service_password, sizeof(this->client_settings_.service_password), password,
                     ssid.empty());
  this->save_client_settings();
  this->update_sta_priority();
  this->reconnect_sta_();
}

void GsmartWifiManager::set_sta_mode(uint8_t mode) {
  if (mode > 3) mode = 3;
  this->client_settings_.sta_mode = mode;
  this->save_client_settings();
  this->update_sta_priority();
  this->apply_wifi_state();
  this->reconnect_sta_();
}

void GsmartWifiManager::set_sta_customer_primary(const std::string &ssid, const std::string &password) {
  this->copy_string_(this->client_settings_.customer_primary_ssid, sizeof(this->client_settings_.customer_primary_ssid), ssid);
  this->copy_string_(this->client_settings_.customer_primary_password, sizeof(this->client_settings_.customer_primary_password),
                     password, !ssid.empty());
  this->save_client_settings();
  this->update_sta_priority();
  this->reconnect_sta_();
}

void GsmartWifiManager::set_sta_customer_secondary(const std::string &ssid, const std::string &password) {
  this->copy_string_(this->client_settings_.customer_secondary_ssid, sizeof(this->client_settings_.customer_secondary_ssid),
                     ssid);
  this->copy_string_(this->client_settings_.customer_secondary_password,
                     sizeof(this->client_settings_.customer_secondary_password), password, !ssid.empty());
  this->save_client_settings();
  this->update_sta_priority();
  this->reconnect_sta_();
}

void GsmartWifiManager::set_service_ap(const std::string &password, uint8_t mode) {
  this->copy_string_(this->ap_settings_.service_ap_password, sizeof(this->ap_settings_.service_ap_password), password, true);
  this->ap_settings_.service_ap_mode = mode ? 1 : 0;
  this->save_ap_settings();
  this->update_sta_priority();
  if (this->ap_settings_.service_ap_mode) {
    this->start_service_ap("");
  } else {
    this->stop_service_ap();
  }
}

void GsmartWifiManager::set_service_ap_timeouts(int32_t startup_timeout_min, int32_t manual_timeout_min) {
  bool changed = false;
  if (startup_timeout_min >= -1 && this->service_ap_settings_.startup_timeout_min != startup_timeout_min) {
    this->service_ap_settings_.startup_timeout_min = startup_timeout_min;
    changed = true;
  }
  if (manual_timeout_min >= 0 && this->service_ap_settings_.manual_timeout_min != manual_timeout_min) {
    this->service_ap_settings_.manual_timeout_min = manual_timeout_min;
    changed = true;
  }
  if (changed)
    this->save_service_ap_settings();
}

void GsmartWifiManager::set_region_ap(const std::string &ssid, const std::string &password, uint8_t mode,
                                      uint8_t channel) {
  this->copy_string_(this->ap_settings_.region_ap_ssid, sizeof(this->ap_settings_.region_ap_ssid), ssid, true);
  this->copy_string_(this->ap_settings_.region_ap_password, sizeof(this->ap_settings_.region_ap_password), password, true);
  this->ap_settings_.region_ap_mode = mode;
  this->ap_settings_.region_ap_channel = (channel <= 14) ? channel : 0;
  this->save_ap_settings();
  this->update_sta_priority();
  this->apply_wifi_state();
  this->reconnect_sta_();
}

void GsmartWifiManager::set_cloud_mode(uint8_t mode) {
  if (mode > 4) mode = 2;  // clamp; default to full
  this->cloud_settings_.cloud_mode = mode;
  this->save_cloud_settings();
}

void GsmartWifiManager::save_cloud_settings() {
  this->cloud_pref_.save(&this->cloud_settings_);
  global_preferences->sync();
}

bool GsmartWifiManager::is_connected() const {
  return wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_connected();
}

std::string GsmartWifiManager::get_active_ssid() const {
  if (wifi::global_wifi_component == nullptr)
    return "";
  char ssid_buf[wifi::SSID_BUFFER_SIZE];
  return wifi::global_wifi_component->wifi_ssid_to(ssid_buf);
}

std::string GsmartWifiManager::get_ip_address() const {
  if (wifi::global_wifi_component == nullptr)
    return "0.0.0.0";
  auto ips = wifi::global_wifi_component->get_ip_addresses();
  char ip_buf[network::IP_ADDRESS_BUFFER_SIZE];
  ips[0].str_to(ip_buf);
  return std::string(ip_buf);
}

std::string GsmartWifiManager::get_active_ap_profile() const {
  if (!this->current_ap_enabled_ || !this->soft_ap_matches_expected_())
    return "none";
  if (this->service_ap_runtime_active_ && this->current_ap_ssid_ == this->get_service_ap_ssid())
    return "service_ap";
  if (this->ap_settings_.region_ap_mode)
    return "region_ap";
  return "none";
}

bool GsmartWifiManager::is_ap_active() const { return this->current_ap_enabled_ && this->is_soft_ap_running_(); }

bool GsmartWifiManager::is_service_ap_active() const {
  return this->service_ap_runtime_active_ && this->is_ap_active() &&
         this->get_soft_ap_runtime_ssid_() == this->get_service_ap_ssid();
}

uint32_t GsmartWifiManager::get_service_ap_auto_off_remaining_sec() const {
  if (!this->service_ap_auto_off_scheduled_)
    return 0;
  const uint32_t now = millis();
  const int32_t remaining_ms = static_cast<int32_t>(this->service_ap_auto_off_at_ - now);
  if (remaining_ms <= 0)
    return 0;
  return (static_cast<uint32_t>(remaining_ms) + 999UL) / 1000UL;
}

std::string GsmartWifiManager::get_service_ap_ssid() const {
  if (storage::store != nullptr)
    return "Gsmart-" + str_lower_case(storage::store->get_serial());
  return "Gsmart-" + str_lower_case(get_mac_address().substr(6));
}

void GsmartWifiManager::start_service_ap(const std::string &password, int32_t timeout_min) {
  if (!password.empty()) {
    this->copy_string_(this->ap_settings_.service_ap_password, sizeof(this->ap_settings_.service_ap_password), password,
                       true);
    this->save_ap_settings();
  }

  this->service_ap_runtime_active_ = true;
  const int32_t effective_timeout = timeout_min == SERVICE_AP_TIMEOUT_USE_DEFAULT
                                        ? this->service_ap_settings_.manual_timeout_min
                                        : timeout_min;
  const int32_t safe_timeout = effective_timeout < 0 ? this->service_ap_settings_.manual_timeout_min : effective_timeout;
  this->apply_wifi_state();
  this->schedule_service_ap_auto_off_(this->timeout_min_to_seconds_(safe_timeout));
}

void GsmartWifiManager::start_service_ap_for_duration(const std::string &password, uint32_t duration_sec) {
  if (!password.empty()) {
    this->copy_string_(this->ap_settings_.service_ap_password, sizeof(this->ap_settings_.service_ap_password), password,
                       true);
    this->save_ap_settings();
  }

  this->service_ap_runtime_active_ = true;
  this->apply_wifi_state();
  this->schedule_service_ap_auto_off_(duration_sec);
}

void GsmartWifiManager::stop_service_ap() {
  this->service_ap_runtime_active_ = false;
  this->cancel_service_ap_auto_off_();
  this->apply_wifi_state();
}

void GsmartWifiManager::set_service_ap_runtime(bool active) {
  if (active)
    this->start_service_ap("");
  else
    this->stop_service_ap();
}

bool GsmartWifiManager::toggle_service_ap() {
  const bool next = !this->is_service_ap_active();
  this->set_service_ap_runtime(next);
  return next;
}

void GsmartWifiManager::start_scan(bool manual) {
  if (this->scan_pending_)
    return;

  this->manual_scan_ = manual;
  this->scan_cache_valid_ = false;
  this->scan_pending_ = true;
  this->last_scan_time_ = millis();

#ifdef USE_ESP32
  wifi_scan_config_t scan_config = {};
  scan_config.show_hidden = false;
  esp_err_t err = esp_wifi_scan_start(&scan_config, false);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_wifi_scan_start failed: %s", esp_err_to_name(err));
    this->scan_pending_ = false;
  } else {
    ESP_LOGD(TAG, "Started ESP32 Wi-Fi scan");
  }
#elif defined(USE_ESP8266)
  int complete = WiFi.scanComplete();
  if (complete == WIFI_SCAN_RUNNING) {
    return;
  }
  if (complete >= 0)
    WiFi.scanDelete();
  WiFi.scanNetworksAsync(nullptr, true);
  ESP_LOGD(TAG, "Started ESP8266 async Wi-Fi scan");
#else
  this->scan_pending_ = false;
#endif
}

void GsmartWifiManager::load_settings() {
  this->client_pref_ = global_preferences->make_preference<WifiSettingsClient>(WIFI_SETTINGS_CLIENT_PREF_ID);
  WifiSettingsClient loaded_client{};
  const bool client_loaded = this->client_pref_.load(&loaded_client);
  const bool client_valid = client_loaded && loaded_client.magic == WIFI_SETTINGS_MAGIC &&
                            loaded_client.version == WIFI_SETTINGS_CLIENT_VERSION;
  this->persistence_status_.client_loaded = client_valid;
  this->persistence_status_.client_save_ok = true;
  this->persistence_status_.client_sync_ok = true;
  this->persistence_status_.client_verify_ok = true;
  if (client_valid) {
    this->client_settings_ = loaded_client;
    this->log_client_settings_("Loaded Client Wi-Fi settings");
  } else {
    ESP_LOGI(TAG, "Initializing default Client Wi-Fi settings (v%u)", WIFI_SETTINGS_CLIENT_VERSION);
    std::memset(&this->client_settings_, 0, sizeof(WifiSettingsClient));
    this->client_settings_.magic = WIFI_SETTINGS_MAGIC;
    this->client_settings_.version = WIFI_SETTINGS_CLIENT_VERSION;
    this->client_settings_.sta_mode = 1;             // on
    this->copy_string_(this->client_settings_.service_ssid, sizeof(this->client_settings_.service_ssid),
                       SERVICE_DEFAULT_SSID);
    this->copy_string_(this->client_settings_.service_password, sizeof(this->client_settings_.service_password),
                       SERVICE_DEFAULT_PWD);
    this->client_settings_.service_mode = 1;
    this->save_client_settings();
    this->log_client_settings_("Default Client Wi-Fi settings stored");
  }

  this->ap_pref_ = global_preferences->make_preference<WifiSettingsAp>(WIFI_SETTINGS_AP_PREF_ID);
  if (!this->ap_pref_.load(&this->ap_settings_) || this->ap_settings_.magic != WIFI_SETTINGS_MAGIC ||
      (this->ap_settings_.version != WIFI_SETTINGS_AP_VERSION && this->ap_settings_.version != 2)) {
    ESP_LOGI(TAG, "Initializing default AP Wi-Fi settings (v%u)", WIFI_SETTINGS_AP_VERSION);
    std::memset(&this->ap_settings_, 0, sizeof(WifiSettingsAp));
    this->ap_settings_.magic = WIFI_SETTINGS_MAGIC;
    this->ap_settings_.version = WIFI_SETTINGS_AP_VERSION;

    this->copy_string_(this->ap_settings_.service_ap_password, sizeof(this->ap_settings_.service_ap_password),
                       "12345678");
    this->ap_settings_.service_ap_mode = 1;
    this->ap_settings_.region_ap_mode = 0;
    this->ap_settings_.region_ap_channel = 0;        // auto

    this->save_ap_settings();
  } else if (this->ap_settings_.version == 2) {
    ESP_LOGI(TAG, "Migrating AP Wi-Fi settings from v2 to v%u", WIFI_SETTINGS_AP_VERSION);
    this->ap_settings_.version = WIFI_SETTINGS_AP_VERSION;
    this->ap_settings_.service_ap_mode = this->ap_settings_.service_ap_mode ? 1 : 0;
    this->ap_settings_.region_ap_mode = this->ap_settings_.region_ap_mode ? 1 : 0;
    this->save_ap_settings();
  }

  this->cloud_pref_ = global_preferences->make_preference<CloudSettings>(WIFI_SETTINGS_CLOUD_PREF_ID);
  if (!this->cloud_pref_.load(&this->cloud_settings_) || this->cloud_settings_.magic != WIFI_SETTINGS_MAGIC ||
      this->cloud_settings_.version != WIFI_SETTINGS_CLOUD_VERSION) {
    ESP_LOGI(TAG, "Initializing default Cloud settings (v%u)", WIFI_SETTINGS_CLOUD_VERSION);
    std::memset(&this->cloud_settings_, 0, sizeof(CloudSettings));
    this->cloud_settings_.magic = WIFI_SETTINGS_MAGIC;
    this->cloud_settings_.version = WIFI_SETTINGS_CLOUD_VERSION;
    this->cloud_settings_.cloud_mode = 2;            // full (backward-compatible default)
    this->save_cloud_settings();
  }

  this->service_ap_pref_ =
      global_preferences->make_preference<WifiSettingsServiceAp>(WIFI_SETTINGS_SERVICE_AP_PREF_ID);
  if (!this->service_ap_pref_.load(&this->service_ap_settings_) ||
      this->service_ap_settings_.magic != WIFI_SETTINGS_MAGIC ||
      (this->service_ap_settings_.version != WIFI_SETTINGS_SERVICE_AP_VERSION &&
       this->service_ap_settings_.version != 1)) {
    ESP_LOGI(TAG, "Initializing default Service AP lifetime settings (v%u)", WIFI_SETTINGS_SERVICE_AP_VERSION);
    std::memset(&this->service_ap_settings_, 0, sizeof(WifiSettingsServiceAp));
    this->service_ap_settings_.magic = WIFI_SETTINGS_MAGIC;
    this->service_ap_settings_.version = WIFI_SETTINGS_SERVICE_AP_VERSION;
    this->service_ap_settings_.startup_timeout_min = SERVICE_AP_DEFAULT_TIMEOUT_MIN;
    this->service_ap_settings_.manual_timeout_min = SERVICE_AP_DEFAULT_TIMEOUT_MIN;
    this->save_service_ap_settings();
  } else {
    bool changed = false;
    if (this->service_ap_settings_.version == 1) {
      this->service_ap_settings_.version = WIFI_SETTINGS_SERVICE_AP_VERSION;
      if (this->service_ap_settings_.startup_timeout_min == SERVICE_AP_OLD_DEFAULT_TIMEOUT_MIN)
        this->service_ap_settings_.startup_timeout_min = SERVICE_AP_DEFAULT_TIMEOUT_MIN;
      if (this->service_ap_settings_.manual_timeout_min == SERVICE_AP_OLD_DEFAULT_TIMEOUT_MIN)
        this->service_ap_settings_.manual_timeout_min = SERVICE_AP_DEFAULT_TIMEOUT_MIN;
      changed = true;
    }
    if (this->service_ap_settings_.startup_timeout_min < -1) {
      this->service_ap_settings_.startup_timeout_min = -1;
      changed = true;
    }
    if (this->service_ap_settings_.manual_timeout_min < 0) {
      this->service_ap_settings_.manual_timeout_min = SERVICE_AP_DEFAULT_TIMEOUT_MIN;
      changed = true;
    }
    if (changed)
      this->save_service_ap_settings();
  }
}

bool GsmartWifiManager::save_client_settings() {
  const bool save_ok = this->client_pref_.save(&this->client_settings_);
  const bool sync_ok = global_preferences != nullptr && global_preferences->sync();

  WifiSettingsClient verify{};
  const bool verify_load_ok = this->client_pref_.load(&verify);
  const bool verify_ok = verify_load_ok &&
                         std::memcmp(&verify, &this->client_settings_, sizeof(WifiSettingsClient)) == 0;

  this->persistence_status_.client_save_ok = save_ok;
  this->persistence_status_.client_sync_ok = sync_ok;
  this->persistence_status_.client_verify_ok = verify_ok;

  if (!save_ok || !sync_ok || !verify_ok) {
    // save=false almost always means the platform could not hand out a preference slot (on
    // ESP8266 the flash preference pool is shared and fixed size), so the credentials would be
    // lost on the next boot. Log it as an error - this must not look like a routine warning.
    ESP_LOGE(TAG, "Client Wi-Fi settings persistence FAILED: save=%s sync=%s verify=%s", save_ok ? "true" : "false",
             sync_ok ? "true" : "false", verify_ok ? "true" : "false");
  } else {
    ESP_LOGI(TAG, "Client Wi-Fi settings persisted and verified: primary_ssid='%s' password_set=%s sta_mode=%u",
             this->client_settings_.customer_primary_ssid,
             this->client_settings_.customer_primary_password[0] != '\0' ? "true" : "false",
             this->client_settings_.sta_mode);
  }

  return save_ok && sync_ok && verify_ok;
}

void GsmartWifiManager::save_ap_settings() {
  this->ap_pref_.save(&this->ap_settings_);
  global_preferences->sync();
}

void GsmartWifiManager::save_service_ap_settings() {
  this->service_ap_pref_.save(&this->service_ap_settings_);
  global_preferences->sync();
}

void GsmartWifiManager::update_sta_priority() {
  auto *wifi = wifi::global_wifi_component;
  if (wifi == nullptr)
    return;

  wifi->clear_sta();

  if (this->client_settings_.sta_mode == 0) {
    ESP_LOGI(TAG, "STA mode OFF; STA list disabled");
    wifi->init_sta(0);
    return;
  }

  const bool svc_active = this->client_settings_.service_mode &&
                          this->client_settings_.service_ssid[0] != '\0';

  size_t count = this->manufacture_networks_.size();
  if (svc_active)
    count++;
  if (this->client_settings_.customer_primary_ssid[0] != '\0')
    count++;
  if (this->client_settings_.customer_secondary_ssid[0] != '\0')
    count++;

  wifi->init_sta(count);

  for (const auto &net : this->manufacture_networks_) {
    wifi::WiFiAP ap;
    ap.set_ssid(net.ssid);
    ap.set_password(net.password);
    ap.set_priority(100);
    wifi->add_sta(ap);
  }

  if (svc_active) {
    wifi::WiFiAP ap;
    ap.set_ssid(this->client_settings_.service_ssid);
    ap.set_password(this->client_settings_.service_password);
    ap.set_priority(50);
    wifi->add_sta(ap);
  }

  if (this->client_settings_.customer_primary_ssid[0] != '\0') {
    wifi::WiFiAP ap;
    ap.set_ssid(this->client_settings_.customer_primary_ssid);
    ap.set_password(this->client_settings_.customer_primary_password);
    ap.set_priority(10);
    wifi->add_sta(ap);
  }

  if (this->client_settings_.customer_secondary_ssid[0] != '\0') {
    wifi::WiFiAP ap;
    ap.set_ssid(this->client_settings_.customer_secondary_ssid);
    ap.set_password(this->client_settings_.customer_secondary_password);
    ap.set_priority(5);
    wifi->add_sta(ap);
  }
}

void GsmartWifiManager::apply_service_ap_startup_policy_() {
  this->cancel_service_ap_auto_off_();
  const bool region_ap_enabled = this->ap_settings_.region_ap_mode != 0;
  this->service_ap_runtime_active_ = this->ap_settings_.service_ap_mode != 0 &&
                                     this->service_ap_settings_.startup_timeout_min >= 0 &&
                                     !region_ap_enabled;
}

void GsmartWifiManager::apply_wifi_state() {
  std::string ssid;
  std::string password;
  bool active = false;
  bool ap_only = false;
  uint8_t channel = 0;

  if (this->service_ap_runtime_active_) {
    ssid = this->get_service_ap_ssid();
    password = this->ap_settings_.service_ap_password;
    active = true;
  } else if (this->ap_settings_.region_ap_mode) {
    ssid = this->ap_settings_.region_ap_ssid;
    password = this->ap_settings_.region_ap_password;
    active = true;
    ap_only = this->client_settings_.sta_mode == 0;
    channel = this->ap_settings_.region_ap_channel;
  }

  this->apply_soft_ap_(active, ssid, password, ap_only, channel);
  if (active)
    this->start_scan(true);
}

void GsmartWifiManager::apply_soft_ap_(bool active, const std::string &ssid, const std::string &password, bool ap_only,
                                       uint8_t channel) {
  if (wifi::global_wifi_component != nullptr) {
    wifi::WiFiAP ap;
    ap.set_ssid(ssid);
    ap.set_password(password);
    wifi::global_wifi_component->set_ap(ap);
  }

#ifdef USE_ESP32
  wifi_mode_t current_mode = WIFI_MODE_NULL;
  esp_err_t err = esp_wifi_get_mode(&current_mode);
  if (err != ESP_OK)
    current_mode = WIFI_MODE_NULL;

  bool sta_enabled = current_mode == WIFI_MODE_STA || current_mode == WIFI_MODE_APSTA;
  bool ap_enabled = current_mode == WIFI_MODE_AP || current_mode == WIFI_MODE_APSTA;

  if (active) {
    if (ssid.empty()) {
      ESP_LOGW(TAG, "SoftAP requested without SSID");
      return;
    }

    wifi_config_t conf = {};
    std::memcpy(reinterpret_cast<char *>(conf.ap.ssid), ssid.c_str(), std::min<size_t>(ssid.size(), 32));
    conf.ap.ssid_len = std::min<size_t>(ssid.size(), 32);
    conf.ap.channel = (channel >= 1 && channel <= 14) ? channel : 1;
    conf.ap.ssid_hidden = 0;
    conf.ap.max_connection = 5;
    conf.ap.beacon_interval = 100;
    conf.ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP;
    if (password.empty()) {
      conf.ap.authmode = WIFI_AUTH_OPEN;
    } else {
      conf.ap.authmode = WIFI_AUTH_WPA2_PSK;
      std::memcpy(reinterpret_cast<char *>(conf.ap.password), password.c_str(), std::min<size_t>(password.size(), 64));
    }

    wifi_mode_t new_mode = ap_only ? WIFI_MODE_AP : WIFI_MODE_APSTA;
    err = esp_wifi_set_mode(new_mode);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "esp_wifi_set_mode(%d) failed: %s", new_mode, esp_err_to_name(err));
      return;
    }
    err = esp_wifi_set_config(WIFI_IF_AP, &conf);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "esp_wifi_set_config(WIFI_IF_AP) failed: %s", esp_err_to_name(err));
      return;
    }
  } else if (ap_enabled) {
    wifi_mode_t new_mode = sta_enabled ? WIFI_MODE_STA : WIFI_MODE_NULL;
    err = esp_wifi_set_mode(new_mode);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "esp_wifi_set_mode(%d) failed: %s", new_mode, esp_err_to_name(err));
      return;
    }
  }
#elif defined(USE_ESP8266)
  if (active) {
    if (ssid.empty()) {
      ESP_LOGW(TAG, "SoftAP requested without SSID");
      return;
    }
    WiFi.softAP(ssid.c_str(), password.empty() ? nullptr : password.c_str());
    WiFi.mode(ap_only ? WIFI_AP : WIFI_AP_STA);
  } else {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
  }
#endif

  this->current_ap_enabled_ = active;
  this->current_ap_ssid_ = active ? ssid : "";
  this->current_ap_password_ = active ? password : "";
  ESP_LOGI(TAG, "SoftAP %s%s%s", active ? "active: " : "disabled", active ? ssid.c_str() : "", ap_only ? " (AP only)" : "");
}

void GsmartWifiManager::reconnect_sta_() {
  auto *wifi = wifi::global_wifi_component;
  if (wifi == nullptr || !wifi->has_sta())
    return;
  this->defer([wifi]() {
    if (wifi->has_sta())
      wifi->start_scanning();
  });
}

void GsmartWifiManager::trigger_reconnect_() {
#ifdef USE_ESP32
  esp_wifi_disconnect();
#elif defined(USE_ESP8266)
  WiFi.disconnect();
#endif
  this->reconnect_sta_();
}

uint8_t GsmartWifiManager::priority_for_ssid_(const std::string &ssid) const {
  if (ssid.empty())
    return 0;
  for (const auto &net : this->manufacture_networks_) {
    if (net.ssid == ssid)
      return 100;
  }
  if (this->client_settings_.service_mode &&
      this->client_settings_.service_ssid[0] != '\0' &&
      ssid == this->client_settings_.service_ssid)
    return 50;
  if (ssid == this->client_settings_.customer_primary_ssid)
    return 10;
  if (ssid == this->client_settings_.customer_secondary_ssid)
    return 5;
  return 0;
}

uint8_t GsmartWifiManager::current_sta_priority_() const { return this->priority_for_ssid_(this->get_active_ssid()); }

uint8_t GsmartWifiManager::highest_configured_sta_priority_() const {
  if (!this->manufacture_networks_.empty())
    return 100;
  if (this->client_settings_.service_mode && this->client_settings_.service_ssid[0] != '\0')
    return 50;
  if (this->client_settings_.customer_primary_ssid[0] != '\0')
    return 10;
  if (this->client_settings_.customer_secondary_ssid[0] != '\0')
    return 5;
  return 0;
}

bool GsmartWifiManager::should_periodic_scan_() const {
  if (this->is_ap_active())
    return true;

  auto *wifi = wifi::global_wifi_component;
  if (wifi == nullptr || !wifi->has_sta() || !this->is_connected())
    return false;

  return this->current_sta_priority_() < this->highest_configured_sta_priority_();
}

bool GsmartWifiManager::is_soft_ap_running_() const {
#ifdef USE_ESP32
  wifi_mode_t mode = WIFI_MODE_NULL;
  if (esp_wifi_get_mode(&mode) != ESP_OK)
    return false;
  return mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA;
#elif defined(USE_ESP8266)
  WiFiMode_t mode = WiFi.getMode();
  return mode == WIFI_AP || mode == WIFI_AP_STA;
#else
  return this->current_ap_enabled_;
#endif
}

std::string GsmartWifiManager::get_soft_ap_runtime_ssid_() const {
#ifdef USE_ESP32
  if (!this->is_soft_ap_running_())
    return "";
  wifi_config_t conf = {};
  if (esp_wifi_get_config(WIFI_IF_AP, &conf) != ESP_OK)
    return "";
  size_t len = conf.ap.ssid_len;
  if (len == 0)
    len = strnlen(reinterpret_cast<const char *>(conf.ap.ssid), sizeof(conf.ap.ssid));
  return std::string(reinterpret_cast<const char *>(conf.ap.ssid), std::min<size_t>(len, sizeof(conf.ap.ssid)));
#elif defined(USE_ESP8266)
  if (!this->is_soft_ap_running_())
    return "";
  return WiFi.softAPSSID().c_str();
#else
  return this->current_ap_ssid_;
#endif
}

bool GsmartWifiManager::soft_ap_matches_expected_() const {
  if (!this->current_ap_enabled_)
    return true;
  if (!this->is_soft_ap_running_())
    return false;
  return this->get_soft_ap_runtime_ssid_() == this->current_ap_ssid_;
}

void GsmartWifiManager::ensure_soft_ap_state_() {
  if (!this->current_ap_enabled_ || this->soft_ap_matches_expected_())
    return;

  const uint32_t now = millis();
  if (now - this->last_soft_ap_reapply_ms_ < SOFT_AP_REAPPLY_INTERVAL_MS)
    return;
  this->last_soft_ap_reapply_ms_ = now;

  ESP_LOGW(TAG, "SoftAP runtime state does not match requested state; reapplying AP configuration");
  this->apply_wifi_state();
}

uint32_t GsmartWifiManager::timeout_min_to_seconds_(int32_t timeout_min) const {
  if (timeout_min <= 0)
    return 0;
  const uint32_t minutes = static_cast<uint32_t>(timeout_min);
  return minutes > (SERVICE_AP_MAX_AUTO_OFF_SEC / 60UL) ? SERVICE_AP_MAX_AUTO_OFF_SEC : minutes * 60UL;
}

void GsmartWifiManager::schedule_service_ap_auto_off_(uint32_t duration_sec) {
  this->cancel_service_ap_auto_off_();
  if (!this->service_ap_runtime_active_ || duration_sec == 0)
    return;

  const uint32_t delay_ms =
      (duration_sec > SERVICE_AP_MAX_AUTO_OFF_SEC ? SERVICE_AP_MAX_AUTO_OFF_SEC : duration_sec) * 1000UL;
  this->service_ap_auto_off_scheduled_ = true;
  this->service_ap_auto_off_at_ = millis() + delay_ms;
  this->set_timeout(SERVICE_AP_AUTO_OFF_TIMEOUT, delay_ms, [this]() {
    this->service_ap_runtime_active_ = false;
    this->service_ap_auto_off_scheduled_ = false;
    this->service_ap_auto_off_at_ = 0;
    this->apply_wifi_state();
  });
}

void GsmartWifiManager::cancel_service_ap_auto_off_() {
  this->cancel_timeout(SERVICE_AP_AUTO_OFF_TIMEOUT);
  this->service_ap_auto_off_scheduled_ = false;
  this->service_ap_auto_off_at_ = 0;
}

void GsmartWifiManager::cache_scan_result_(const std::string &ssid, int8_t rssi, uint8_t channel, bool secure) {
  if (ssid.empty())
    return;
  auto it = std::find_if(this->scan_cache_.begin(), this->scan_cache_.end(),
                         [&ssid](const WifiScanCacheItem &item) { return item.ssid == ssid; });
  uint8_t priority = this->priority_for_ssid_(ssid);
  if (it == this->scan_cache_.end()) {
    this->scan_cache_.push_back({ssid, rssi, channel, secure, priority, priority > 0});
  } else if (rssi > it->rssi) {
    it->rssi = rssi;
    it->channel = channel;
    it->secure = secure;
    it->priority = priority;
    it->known = priority > 0;
  }
}

void GsmartWifiManager::on_wifi_scan_results(const wifi::wifi_scan_vector_t<wifi::WiFiScanResult> &results) {
  this->scan_cache_.clear();
  for (const auto &result : results)
    this->cache_scan_result_(result.get_ssid().str(), result.get_rssi(), result.get_channel(), result.get_with_auth());

  this->process_scan_results_();
  this->scan_pending_ = false;
  this->scan_cache_valid_ = true;
}

void GsmartWifiManager::check_scan_results() {
#ifdef USE_ESP8266
  int complete = WiFi.scanComplete();
  if (complete == WIFI_SCAN_RUNNING) {
    if (millis() - this->last_scan_time_ > SCAN_POLL_TIMEOUT_MS) {
      ESP_LOGW(TAG, "ESP8266 Wi-Fi scan timed out");
      WiFi.scanDelete();
      this->scan_pending_ = false;
    }
    return;
  }
  if (complete < 0)
    return;

  this->scan_cache_.clear();
  for (int i = 0; i < complete; i++) {
    this->cache_scan_result_(WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i), WiFi.encryptionType(i) != ENC_TYPE_NONE);
  }
  WiFi.scanDelete();
  this->process_scan_results_();
  this->scan_pending_ = false;
  this->scan_cache_valid_ = true;
#else
  if (millis() - this->last_scan_time_ > SCAN_POLL_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Wi-Fi scan timed out");
    this->scan_pending_ = false;
  }
#endif
}

void GsmartWifiManager::process_scan_results_() {
  if (this->scan_cache_.empty())
    return;

  std::sort(this->scan_cache_.begin(), this->scan_cache_.end(), [](const WifiScanCacheItem &a, const WifiScanCacheItem &b) {
    if (a.priority != b.priority)
      return a.priority > b.priority;
    return a.rssi > b.rssi;
  });

  const auto &best = this->scan_cache_.front();
  uint8_t current_priority = this->current_sta_priority_();
  if (best.known && best.priority > current_priority && this->is_connected()) {
    ESP_LOGI(TAG, "Higher priority Wi-Fi visible (%s, priority %u > %u), reconnecting", best.ssid.c_str(),
             best.priority, current_priority);
    this->trigger_reconnect_();
  }
}

}  // namespace gsmart_wifi_manager
}  // namespace esphome
