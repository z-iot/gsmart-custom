#include "api_core_v1.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/preferences.h"

#include "payloads.h"
#include "esphome/components/storage/store.h"
#include "esphome/components/storage/util.h"
#include "esphome/components/wifi/wifi_component.h"
#ifdef ESP32
#include <esp_wifi.h>
#endif

#include "esphome/components/gsmart_wifi_manager/gsmart_wifi_manager.h"
#include <algorithm>
#include <cctype>
#include <ctime>

#ifdef USE_MQTT
#include "esphome/components/mqtt/mqtt_client.h"
#endif

#ifdef USE_UDPSERVER
#include "esphome/components/udp_server/udp_server.h"
#endif

namespace esphome {
namespace api_core_v1 {

static const char *const TAG = "api_core_v1";

namespace {

std::string normalize_token(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return std::tolower(ch); });
  return value;
}

std::string normalize_mac_token(std::string value) {
  std::string normalized;
  normalized.reserve(value.size());
  for (char ch : value) {
    if (ch == ':' || ch == '-' || ch == ' ')
      continue;
    normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }
  return normalized;
}

std::string local_mac_string() {
  uint8_t mac[6];
  get_mac_address_raw(mac);
  return storage::convertMacToStr(mac);
}

std::string radiation_mode_to_api(storage::RadiationMode mode) {
  switch (mode) {
    case storage::RadiationMode::MIN:
      return "min";
    case storage::RadiationMode::STD:
      return "std";
    case storage::RadiationMode::MAX:
      return "max";
    case storage::RadiationMode::ON:
      return "on";
    default:
      return "off";
  }
}

storage::RadiationMode radiation_mode_from_api(JsonVariant value) {
  if (value.is<int>()) {
    switch (value.as<int>()) {
      case 1:
        return storage::RadiationMode::MIN;
      case 2:
        return storage::RadiationMode::STD;
      case 3:
        return storage::RadiationMode::MAX;
      case 4:
        return storage::RadiationMode::ON;
      default:
        return storage::RadiationMode::OFF;
    }
  }

  std::string mode = normalize_token(value.as<std::string>());
  if (mode == "min" || mode == "minimum")
    return storage::RadiationMode::MIN;
  if (mode == "std" || mode == "standard" || mode == "normal")
    return storage::RadiationMode::STD;
  if (mode == "max" || mode == "maximum")
    return storage::RadiationMode::MAX;
  if (mode == "on" || mode == "nonstop" || mode == "non" || mode == "inf" || mode == "always")
    return storage::RadiationMode::ON;
  return storage::RadiationMode::OFF;
}

std::string radiation_source_to_api(storage::RadiationSource source) {
  switch (source) {
    case storage::RadiationSource::EXT:
      return "external";
    case storage::RadiationSource::SCH:
      return "scheduler";
    case storage::RadiationSource::REGION:
      return "region";
    case storage::RadiationSource::INT:
    default:
      return "internal";
  }
}

bool json_bool(JsonVariant value, bool fallback) {
  if (value.isNull())
    return fallback;
  if (value.is<bool>())
    return value.as<bool>();
  if (value.is<int>())
    return value.as<int>() != 0;

  std::string token = normalize_token(value.as<std::string>());
  if (token == "true" || token == "1" || token == "on" || token == "enabled" || token == "enable")
    return true;
  if (token == "false" || token == "0" || token == "off" || token == "disabled" || token == "disable")
    return false;
  return fallback;
}

std::string json_string(JsonVariant value, const std::string &fallback = "") {
  if (value.isNull())
    return fallback;
  return value.as<std::string>();
}

bool require_confirmation(JsonObject root, JsonObject response, const char *expected) {
  if (json_string(root["confirm"]) == expected)
    return true;

  response["ok"] = false;
  response["error"] = "confirmation_required";
  response["message"] = std::string("Missing required confirmation token: ") + expected;
  response["requiredConfirm"] = expected;
  return false;
}

storage::RadiationCauseKind radiation_cause_from_request(JsonObject root) {
  std::string cause = normalize_token(json_string(root["causeKind"], json_string(root["transport"], "mobile_api")));
  if (cause == "mqtt")
    return storage::RadiationCauseKind::MQTT;
  return storage::RadiationCauseKind::MOBILE_API;
}

void set_radiation_request_cause(JsonObject root) {
  const auto kind = radiation_cause_from_request(root);
  std::string detail = json_string(root["causeDetail"]);
  if (detail.empty())
    detail = storage::radiationCauseKindToApi(kind);
  storage::store->setRadiationCause(kind, detail);
}

void add_wifi_runtime(JsonObject root) {
  if (wifi::global_wifi_component == nullptr)
    return;

  root["connected"] = wifi::global_wifi_component->is_connected();
  root["apActive"] = gsmart_wifi_manager::global_gsmart_wifi_manager != nullptr
                         ? gsmart_wifi_manager::global_gsmart_wifi_manager->is_ap_active()
                         : esphome::wifi::global_wifi_component->is_ap_active();
  char ssid_buf[wifi::SSID_BUFFER_SIZE];
  root["ssid"] = wifi::global_wifi_component->wifi_ssid_to(ssid_buf);
  root["signal"] = wifi::global_wifi_component->wifi_rssi();
  root["channel"] = wifi::global_wifi_component->get_wifi_channel();

  char ip_buf[network::IP_ADDRESS_BUFFER_SIZE];
  for (const auto &ip : wifi::global_wifi_component->wifi_sta_ip_addresses()) {
    if (ip.is_set()) {
      root["ip"] = ip.str_to(ip_buf);
      break;
    }
  }
}

void add_device_time(JsonObject root) {
  const time_t now = time(nullptr);
  JsonObject time_obj = root["time"].to<JsonObject>();

  char local_buf[24] = "";
  tm *local = localtime(&now);
  if (local != nullptr)
    std::strftime(local_buf, sizeof(local_buf), "%Y-%m-%d %H:%M:%S", local);

  time_obj["epoch"] = static_cast<uint32_t>(now);
  time_obj["local"] = local_buf;
  time_obj["valid"] = now >= 1704067200;  // 2024-01-01; guards unsynced 1970 clocks.
  time_obj["uptimeSec"] = millis() / 1000;
}

void add_situation(JsonObject root) {
  auto &situation = storage::store->global->situation;
  root["schedulerActive"] = situation.SchedulerActive;
  root["schedulerItemsCount"] = situation.SchedulerItemsCount;
  root["source"] = radiation_source_to_api(situation.source);
  root["currentMode"] = radiation_mode_to_api(situation.CurrentMode);
  root["currentIsActive"] = situation.CurrentIsActive;
  root["currentIsSchedule"] = situation.CurrentIsSchedule;
  root["currentIsExternal"] = situation.CurrentIsExternal;
  root["currentBeginTime"] = situation.CurrentBeginTime;
  root["currentEndTime"] = situation.CurrentEndTime;
  root["currentBeamedSec"] = situation.CurrentBeamedSec;
  root["currentTotalSec"] = situation.CurrentTotalSec;
  root["prevMode"] = radiation_mode_to_api(situation.PrevMode);
  root["prevBeginTime"] = situation.PrevBeginTime;
  root["prevEndTime"] = situation.PrevEndTime;
  root["prevBeamedSec"] = situation.PrevBeamedSec;
  root["prevTotalSec"] = situation.PrevTotalSec;
  root["scheduleMode"] = radiation_mode_to_api(situation.SchMode);
  root["scheduleBeginTime"] = situation.SchBeginTime;
  root["scheduleEndTime"] = situation.SchEndTime;
  root["scheduleTotalSec"] = situation.SchTotalSec;
  root["scheduleIsAborted"] = situation.SchIsAborted;
  root["nextMode"] = radiation_mode_to_api(situation.NextMode);
  root["nextBeginTime"] = situation.NextBeginTime;
  root["nextEndTime"] = situation.NextEndTime;
  root["nextTotalSec"] = situation.NextTotalSec;
}

void add_region_runtime(JsonObject root) {
  const auto active_mode = storage::store->global->radiation.activeMode;
  auto &situation = storage::store->global->situation;

  root["mode"] = radiation_mode_to_api(active_mode);
  root["radiate"] = active_mode != storage::RadiationMode::OFF;
  root["source"] = radiation_source_to_api(storage::store->global->radiation.lastSource);
  root["remainingSec"] = storage::store->getTimerDurationSec(time(nullptr));
  root["currentMode"] = radiation_mode_to_api(situation.CurrentMode);
  root["currentIsActive"] = situation.CurrentIsActive;
}

void add_lamps_status(JsonObject root) {
  (void) root;
#if defined(GSMART_FEATURE_USAGE) && defined(GSMART_EMITTER)
  JsonArray lamps = root["lamps"].to<JsonArray>();
  int lamp_count = storage::store->usage->beam.pref.lampCount;
  if (lamp_count < 0 || lamp_count > DEVICE_MAX_LAMP)
    lamp_count = DEVICE_MAX_LAMP;

  bool any_lamp_on = false;
  for (int i = 0; i < lamp_count; i++) {
    const bool running = storage::store->usage->lamp[i].lastStart > storage::store->usage->lamp[i].lastStop;
    any_lamp_on = any_lamp_on || running;

    JsonObject lamp = lamps.add<JsonObject>();
    lamp["channel"] = i + 1;
    lamp["running"] = running;
    lamp["onSec"] = storage::store->usage->lamp[i].pref.onSec;
    lamp["startCount"] = storage::store->usage->lamp[i].pref.startCount;
    lamp["stopCount"] = storage::store->usage->lamp[i].pref.stopCount;
    lamp["lastStartSec"] = storage::store->usage->lamp[i].lastStart;
    lamp["lastStopSec"] = storage::store->usage->lamp[i].lastStop;
  }

  root["lampState"] = any_lamp_on;
  if (lamp_count > 0)
    root["lampStateA"] = storage::store->usage->lamp[0].lastStart > storage::store->usage->lamp[0].lastStop;
  if (lamp_count > 1)
    root["lampStateB"] = storage::store->usage->lamp[1].lastStart > storage::store->usage->lamp[1].lastStop;
#endif
}

void add_motion_status(JsonObject root) {
  (void) root;
#if defined(GSMART_FEATURE_USAGE) && defined(GSMART_EMITTER)
  const bool motion = storage::store->usage->motion.lastStart > storage::store->usage->motion.lastStop;
  root["motion"] = motion;

  JsonObject motion_obj = root["motionInfo"].to<JsonObject>();
  motion_obj["active"] = motion;
  motion_obj["onSec"] = storage::store->usage->motion.onSec;
  motion_obj["offSec"] = storage::store->usage->motion.offSec;
  motion_obj["startCount"] = storage::store->usage->motion.startCount;
  motion_obj["stopCount"] = storage::store->usage->motion.stopCount;
  motion_obj["lastStartSec"] = storage::store->usage->motion.lastStart;
  motion_obj["lastStopSec"] = storage::store->usage->motion.lastStop;
#endif
}

void add_error_status(JsonObject root) {
  JsonObject errors = root["errors"].to<JsonObject>();
  errors["count"] = 0;
  errors["lastCode"] = 0;
  errors["lastMessage"] = "";
  errors["hasError"] = false;

#ifdef GSMART_FEATURE_USAGE
  errors["count"] = storage::store->usage->error.totalCount;
  errors["lastCode"] = storage::store->usage->error.lastCode;
  errors["lastMessage"] = storage::store->usage->error.lastDesc;
  errors["hasError"] = storage::store->usage->error.totalCount > 0;
#endif

  JsonArray warnings = root["warnings"].to<JsonArray>();
  if (storage::store->global->isGuardDurationOverflow()) {
    JsonObject warning = warnings.add<JsonObject>();
    warning["code"] = "guard_duration_overflow";
    warning["message"] = "Radiation guard duration has been exceeded.";
  }
#ifdef GSMART_FEATURE_USAGE
  if (storage::store->usage->error.totalCount > 0) {
    JsonObject warning = warnings.add<JsonObject>();
    warning["code"] = "device_error";
    warning["message"] = storage::store->usage->error.lastDesc;
  }
#endif
}

void add_diagnostics_telemetry(JsonObject root) {
  JsonObject telemetry = root["telemetry"].to<JsonObject>();
  telemetry["wifi"] = wifi::global_wifi_component != nullptr ? "live" : "disabled";
  telemetry["memory"] = "live";
#ifdef GSMART_FEATURE_FILESYSTEM
  telemetry["filesystem"] = "live";
#else
  telemetry["filesystem"] = "disabled";
#endif
#if defined(GSMART_FEATURE_USAGE) && defined(GSMART_EMITTER)
  telemetry["lamps"] = "logical";
  telemetry["motion"] = "live";
#else
  telemetry["lamps"] = "unsupported";
  telemetry["motion"] = "unsupported";
#endif
  telemetry["fans"] = "not_instrumented";
  telemetry["relays"] = "not_instrumented";
  telemetry["triacs"] = "not_instrumented";
#ifdef GSMART_FEATURE_USAGE
  telemetry["errors"] = "stored";
#else
  telemetry["errors"] = "disabled";
#endif
}

}  // namespace

std::string ApiCoreV1::get_build_code_() const {
  if (storage::store == nullptr)
    return "";

  uint8_t build_hi = 0;
  uint8_t build_lo = 0;
  storage::store->getBuildNumber(build_hi, build_lo);
  return str_sprintf("%u.%u", build_hi, build_lo);
}

std::string ApiCoreV1::get_firmware_version_() const {
  if (!this->firmware_version_.empty())
    return this->firmware_version_;

  return this->get_build_code_();
}

std::string ApiCoreV1::get_device_name_() const {
  std::string model = storage::store->get_model();
  if (!model.empty())
    model[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(model[0])));

  std::string serial = storage::store->get_serial();
  std::transform(serial.begin(), serial.end(), serial.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });

  if (model.empty())
    return serial;
  if (serial.empty())
    return model;
  return model + "-" + serial;
}

void ApiCoreV1::sync_preferences_now_() const {
  if (global_preferences != nullptr)
    global_preferences->sync();
}

void ApiCoreV1::build_info(JsonObject root) {
  uint8_t mac[6];
  get_mac_address_raw(mac);

  const std::string build = this->get_build_code_();
  const std::string firmware_version = this->get_firmware_version_();

  root["api"] = "g-node.v1";
  root["model"] = storage::store->get_model();
  root["modelNum"] = storage::store->get_model_num();
  root["serial"] = storage::store->get_serial();
  root["mac"] = storage::convertMacToStr(mac);
  root["name"] = this->get_device_name_();

  JsonObject firmware = root["firmware"].to<JsonObject>();
  firmware["build"] = build;
  firmware["version"] = firmware_version;

  add_wifi_runtime(root);
  add_device_time(root);

  JsonObject capabilities = root["capabilities"].to<JsonObject>();
  capabilities["control"] = true;
  capabilities["diagnostics"] = true;
  const uint8_t model_num = storage::store->get_model_num();
  capabilities["emitter"] = storage::isEmitterModel(model_num);
  capabilities["actuator"] = model_num == 51 || model_num == 52;
#ifdef GSMART_FEATURE_SCHEDULE
  capabilities["scheduler"] = true;
#else
  capabilities["scheduler"] = false;
#endif
#ifdef GSMART_FEATURE_REGION
  capabilities["region"] = true;
#else
  capabilities["region"] = false;
#endif
#ifdef GSMART_FEATURE_USAGE
  capabilities["consumption"] = true;
#else
  capabilities["consumption"] = false;
#endif
#ifdef USE_MQTT
  capabilities["mqtt"] = true;
#else
  capabilities["mqtt"] = false;
#endif
}

void ApiCoreV1::build_status(JsonObject root) {
  const auto active_mode = storage::store->global->radiation.activeMode;
  root["model"] = storage::store->get_model();
  root["serial"] = storage::store->get_serial();
  root["mode"] = radiation_mode_to_api(active_mode);
  root["radiate"] = active_mode != storage::RadiationMode::OFF;
  root["source"] = radiation_source_to_api(storage::store->global->radiation.lastSource);
  root["lastStartSec"] = storage::store->global->radiation.lastStart;
  root["lastStopSec"] = storage::store->global->radiation.lastStop;
  root["remainingSec"] = storage::store->getTimerDurationSec(time(nullptr));
  root["uptimeSec"] = millis() / 1000;

  add_wifi_runtime(root);
  add_lamps_status(root);
  add_motion_status(root);
  add_error_status(root);

  JsonObject situation = root["situation"].to<JsonObject>();
  add_situation(situation);

#ifdef GSMART_FEATURE_REGION
  JsonObject region = root["region"].to<JsonObject>();
  region["active"] = storage::store->region->isRegionActive();
  region["isMaster"] = storage::store->region->isMaster();
  region["regionId"] = storage::convertRegionSerialtoStr(storage::store->region->layout.serial);
  region["selfIndex"] = storage::store->region->selfIndex;
  region["masterIndex"] = storage::store->region->layout.masterIndex;
  region["udpChannel"] = storage::store->region->metadata.udpChannel;
  add_region_runtime(region);
#endif
}

void ApiCoreV1::build_diagnostics(JsonObject root) {
  const std::string build = this->get_build_code_();
  const std::string firmware_version = this->get_firmware_version_();

  root["model"] = storage::store->get_model();
  root["serial"] = storage::store->get_serial();
  root["uptimeSec"] = millis() / 1000;
  add_diagnostics_telemetry(root);

  JsonObject memory = root["memory"].to<JsonObject>();
  memory["freeHeap"] = ESP.getFreeHeap();
#ifdef ESP32
  memory["maxAllocHeap"] = ESP.getMaxAllocHeap();
  memory["psramSize"] = ESP.getPsramSize();
  memory["freePsram"] = ESP.getFreePsram();
#elif defined(ESP8266)
  memory["maxAllocHeap"] = ESP.getMaxFreeBlockSize();
  memory["heapFragmentation"] = ESP.getHeapFragmentation();
#endif

  JsonObject diagnostics_firmware = root["firmware"].to<JsonObject>();
  diagnostics_firmware["build"] = build;
  diagnostics_firmware["version"] = firmware_version;
#ifdef ESP32
  diagnostics_firmware["platform"] = "esp32";
#elif defined(ESP8266)
  diagnostics_firmware["platform"] = "esp8266";
#else
  diagnostics_firmware["platform"] = "unknown";
#endif
  diagnostics_firmware["cpuFreqMhz"] = ESP.getCpuFreqMHz();
  diagnostics_firmware["sketchSize"] = ESP.getSketchSize();
  diagnostics_firmware["freeSketchSpace"] = ESP.getFreeSketchSpace();
  diagnostics_firmware["sdkVersion"] = ESP.getSdkVersion();
  diagnostics_firmware["flashChipSize"] = ESP.getFlashChipSize();
  diagnostics_firmware["flashChipSpeed"] = ESP.getFlashChipSpeed();

  JsonObject filesystem = root["filesystem"].to<JsonObject>();
#ifdef GSMART_FEATURE_FILESYSTEM
  filesystem["enabled"] = true;
  filesystem["total"] = storage::store->file_system_->GetTotalBytes();
  filesystem["used"] = storage::store->file_system_->GetUsedBytes();
#else
  filesystem["enabled"] = false;
  filesystem["total"] = 0;
  filesystem["used"] = 0;
#endif

  JsonObject wifi = root["wifi"].to<JsonObject>();
  add_wifi_runtime(wifi);

  add_lamps_status(root);
  add_motion_status(root);
  add_error_status(root);
}

void ApiCoreV1::build_consumption(JsonObject root) {
#ifdef GSMART_FEATURE_USAGE
  storage::store->usage->fillAdvertise(root);
  root["uptimeSec"] = millis() / 1000;

#ifdef GSMART_EMITTER
  JsonObject beam = root["beam"].to<JsonObject>();
  beam["lampCount"] = storage::store->usage->beam.pref.lampCount;
  beam["fanCount"] = storage::store->usage->beam.pref.fanCount;
  beam["onSec"] = storage::store->usage->beam.pref.onSec;
  beam["startCount"] = storage::store->usage->beam.pref.startCount;
  beam["stopCount"] = storage::store->usage->beam.pref.stopCount;
  beam["lastStartSec"] = storage::store->usage->beam.lastStart;
  beam["lastStopSec"] = storage::store->usage->beam.lastStop;

  JsonArray channels = root["channels"].to<JsonArray>();
  int lamp_count = storage::store->usage->beam.pref.lampCount;
  if (lamp_count < 0 || lamp_count > DEVICE_MAX_LAMP)
    lamp_count = DEVICE_MAX_LAMP;
  for (int i = 0; i < lamp_count; i++) {
    JsonObject channel = channels.add<JsonObject>();
    channel["channel"] = i + 1;
    channel["onSec"] = storage::store->usage->lamp[i].pref.onSec;
    channel["startCount"] = storage::store->usage->lamp[i].pref.startCount;
    channel["stopCount"] = storage::store->usage->lamp[i].pref.stopCount;
    channel["lastStartSec"] = storage::store->usage->lamp[i].lastStart;
    channel["lastStopSec"] = storage::store->usage->lamp[i].lastStop;
    channel["running"] = storage::store->usage->lamp[i].lastStart > storage::store->usage->lamp[i].lastStop;
  }
#endif
#else
  root["enabled"] = false;
#endif
}

void ApiCoreV1::build_network(JsonObject root) {
  auto *mgr = gsmart_wifi_manager::global_gsmart_wifi_manager;
  if (mgr == nullptr)
    return;

  root["connected"] = mgr->is_connected();
  root["active_ssid"] = mgr->get_active_ssid();
  root["ip"] = mgr->get_ip_address();
  root["active_profile"] = mgr->get_active_ap_profile();

  const auto &client = mgr->get_client_settings();
  const auto &ap = mgr->get_ap_settings();
  const auto &service_ap_settings = mgr->get_service_ap_settings();
  const auto &cloud = mgr->get_cloud_settings();

  JsonObject sta = root["sta"].to<JsonObject>();
  sta["mode"] = client.sta_mode;

  auto add_net = [&](JsonObject obj, const char *ssid, const char *pswd) {
    obj["ssid"] = ssid;
    obj["password_set"] = (pswd[0] != 0);
  };

  JsonObject service_obj = sta["service"].to<JsonObject>();
  add_net(service_obj, client.service_ssid, client.service_password);
  service_obj["mode"] = client.service_mode;

  add_net(sta["customer_primary"].to<JsonObject>(), client.customer_primary_ssid, client.customer_primary_password);
  add_net(sta["customer_secondary"].to<JsonObject>(), client.customer_secondary_ssid, client.customer_secondary_password);

  JsonObject soft_ap = root["soft_ap"].to<JsonObject>();

  auto add_ap = [&](JsonObject obj, const char *ssid, const char *pswd, uint8_t mode) {
    obj["ssid"] = ssid;
    obj["password_set"] = (pswd[0] != 0);
    obj["mode"] = mode;
  };

  JsonObject svc_ap_obj = soft_ap["service_ap"].to<JsonObject>();
  svc_ap_obj["ssid"] = mgr->get_service_ap_ssid();
  svc_ap_obj["password_set"] = (ap.service_ap_password[0] != 0);
  svc_ap_obj["mode"] = ap.service_ap_mode;
  svc_ap_obj["enabled"] = ap.service_ap_mode != 0;
  svc_ap_obj["active"] = mgr->is_service_ap_active();
  svc_ap_obj["startup_timeout_min"] = service_ap_settings.startup_timeout_min;
  svc_ap_obj["manual_timeout_min"] = service_ap_settings.manual_timeout_min;
  svc_ap_obj["auto_off_scheduled"] = mgr->is_service_ap_auto_off_scheduled();
  svc_ap_obj["auto_off_remaining_sec"] = mgr->get_service_ap_auto_off_remaining_sec();

  JsonObject region_ap_obj = soft_ap["region_ap"].to<JsonObject>();
  add_ap(region_ap_obj, ap.region_ap_ssid, ap.region_ap_password, ap.region_ap_mode);
  region_ap_obj["enabled"] = ap.region_ap_mode != 0;
  region_ap_obj["channel"] = ap.region_ap_channel;

  JsonObject cloud_obj = root["cloud"].to<JsonObject>();
  cloud_obj["mode"] = cloud.cloud_mode;

  const auto &persistence = mgr->get_persistence_status();
  JsonObject persistence_obj = root["persistence"].to<JsonObject>();
  persistence_obj["client_loaded"] = persistence.client_loaded;
  persistence_obj["client_save_ok"] = persistence.client_save_ok;
  persistence_obj["client_sync_ok"] = persistence.client_sync_ok;
  persistence_obj["client_verify_ok"] = persistence.client_verify_ok;
}

void ApiCoreV1::build_network_scan(JsonObject root) {
  auto *mgr = gsmart_wifi_manager::global_gsmart_wifi_manager;
  if (mgr == nullptr) {
    root["scan_started"] = false;
    root["cached"] = false;
    root["networks"].to<JsonArray>();
    return;
  }

  mgr->start_scan(true);
  root["scan_started"] = true;
  root["cached"] = mgr->has_scan_results();
  JsonArray networks = root["networks"].to<JsonArray>();
  for (const auto &item : mgr->get_scan_results()) {
    JsonObject obj = networks.add<JsonObject>();
    obj["ssid"] = item.ssid;
    obj["rssi"] = item.rssi;
    obj["channel"] = item.channel;
    obj["secure"] = item.secure;
    obj["known"] = item.known;
    obj["priority"] = item.priority;
  }
}

bool ApiCoreV1::apply_network(JsonObject root) {
  auto *mgr = gsmart_wifi_manager::global_gsmart_wifi_manager;
  if (mgr == nullptr)
    return false;

  const auto &client_settings = mgr->get_client_settings();
  const auto &ap_settings = mgr->get_ap_settings();

  auto json_or_current = [](JsonVariant value, const char *fallback) -> std::string {
    if (value.isNull())
      return std::string(fallback);
    return value.as<std::string>();
  };
  auto json_password_or_current = [](JsonObject obj, const char *fallback) -> std::string {
    if (!obj["password"].isNull())
      return obj["password"].as<std::string>();
    if (!obj["pswd"].isNull())
      return obj["pswd"].as<std::string>();
    if (!obj["pwd"].isNull())
      return obj["pwd"].as<std::string>();
    if (!obj["wifi_password"].isNull())
      return obj["wifi_password"].as<std::string>();
    if (!obj["wifi_pass"].isNull())
      return obj["wifi_pass"].as<std::string>();
    return std::string(fallback);
  };

  bool changed = false;

  // Legacy mapping
  if (!root["wifi_ssid"].isNull()) {
    mgr->set_sta_customer_primary(root["wifi_ssid"].as<std::string>(),
                                  json_password_or_current(root, client_settings.customer_primary_password));
    changed = true;
  }

  if (root["client"].is<JsonObject>()) {
    JsonObject client_obj = root["client"].as<JsonObject>();
    std::string ssid = json_string(client_obj["ssid"]);
    std::string password = json_password_or_current(client_obj, client_settings.customer_primary_password);
    if (!ssid.empty()) {
      mgr->set_sta_customer_primary(ssid, password);
      changed = true;
    }
  }

  // New API
  if (root["sta"].is<JsonObject>()) {
    JsonObject sta = root["sta"].as<JsonObject>();
    if (!sta["mode"].isNull()) {
      mgr->set_sta_mode(sta["mode"].as<uint8_t>());
      changed = true;
    }
    if (sta["service"].is<JsonObject>()) {
      JsonObject s = sta["service"].as<JsonObject>();
      uint8_t mode = s["mode"].isNull() ? client_settings.service_mode : s["mode"].as<uint8_t>();
      mgr->set_sta_service(json_or_current(s["ssid"], client_settings.service_ssid),
                           json_password_or_current(s, client_settings.service_password), mode);
      changed = true;
    }
    if (sta["customer_primary"].is<JsonObject>()) {
      JsonObject s = sta["customer_primary"].as<JsonObject>();
      mgr->set_sta_customer_primary(json_or_current(s["ssid"], client_settings.customer_primary_ssid),
                                    json_password_or_current(s, client_settings.customer_primary_password));
      changed = true;
    }
    if (sta["customer_secondary"].is<JsonObject>()) {
      JsonObject s = sta["customer_secondary"].as<JsonObject>();
      mgr->set_sta_customer_secondary(json_or_current(s["ssid"], client_settings.customer_secondary_ssid),
                                      json_password_or_current(s, client_settings.customer_secondary_password));
      changed = true;
    }
  }

  if (root["soft_ap"].is<JsonObject>()) {
    JsonObject soft_ap = root["soft_ap"].as<JsonObject>();
    if (soft_ap["service_ap"].is<JsonObject>()) {
      JsonObject s = soft_ap["service_ap"].as<JsonObject>();
      uint8_t mode = ap_settings.service_ap_mode;
      if (!s["mode"].isNull()) {
        mode = s["mode"].as<uint8_t>();
      } else if (!s["enabled"].isNull()) {
        mode = json_bool(s["enabled"], false) ? 1 : 0;
      }
      const int32_t startup_timeout =
          s["startup_timeout_min"].isNull() ? mgr->get_service_ap_settings().startup_timeout_min
                                            : s["startup_timeout_min"].as<int32_t>();
      const int32_t manual_timeout =
          s["manual_timeout_min"].isNull() ? mgr->get_service_ap_settings().manual_timeout_min
                                           : s["manual_timeout_min"].as<int32_t>();
      mgr->set_service_ap_timeouts(startup_timeout, manual_timeout);
      mgr->set_service_ap(json_password_or_current(s, ap_settings.service_ap_password), mode);
      changed = true;
    }
    if (soft_ap["region_ap"].is<JsonObject>()) {
      JsonObject s = soft_ap["region_ap"].as<JsonObject>();
      uint8_t channel = s["channel"].isNull() ? ap_settings.region_ap_channel : s["channel"].as<uint8_t>();
      uint8_t mode = ap_settings.region_ap_mode;
      if (!s["mode"].isNull()) {
        mode = s["mode"].as<uint8_t>();
      } else if (!s["enabled"].isNull()) {
        mode = json_bool(s["enabled"], false) ? 1 : 0;
      }
      mgr->set_region_ap(json_or_current(s["ssid"], ap_settings.region_ap_ssid),
                         json_password_or_current(s, ap_settings.region_ap_password), mode, channel);
      changed = true;
    }
  }

  if (root["cloud"].is<JsonObject>()) {
    JsonObject cloud_obj = root["cloud"].as<JsonObject>();
    if (!cloud_obj["mode"].isNull()) {
      mgr->set_cloud_mode(cloud_obj["mode"].as<uint8_t>());
      changed = true;
    }
  }

  if (changed)
    this->sync_preferences_now_();

  return changed;
}


void ApiCoreV1::build_mqtt(JsonObject root) {
#ifdef USE_MQTT
  root["enabled"] = true;
  root["connected"] = mqtt::global_mqtt_client != nullptr && mqtt::global_mqtt_client->is_connected();
#else
  root["enabled"] = false;
  root["connected"] = false;
#endif
  root["lastConnect"] = storage::store->global->con.lastConnect;
  root["lastDisconnect"] = storage::store->global->con.lastDisconnect;
  root["disconnectCount"] = storage::store->global->con.disconnectCount;
  root["disconnectSecLast"] = storage::store->global->con.disconnectSecLast;
  root["disconnectSecTotal"] = storage::store->global->con.disconnectSecTotal;
}

void ApiCoreV1::build_region(JsonObject root) {
#ifdef GSMART_FEATURE_REGION
  storage::store->region->saveToJson(root);
  root["regionId"] = storage::convertRegionSerialtoStr(storage::store->region->layout.serial);
  root["regionName"] = storage::store->region->metadata.name;
  root["regionDescription"] = storage::store->region->metadata.description;
  root["udpChannel"] = storage::store->region->metadata.udpChannel;
  root["regionNum"] = storage::store->region->metadata.udpChannel;
  root["configVersion"] = storage::store->region->metadata.configVersion;
  root["masterIndex"] = storage::store->region->layout.masterIndex;
  root["selfIndex"] = storage::store->region->selfIndex;
  root["isMaster"] = storage::store->region->isMaster();
  root["active"] = storage::store->region->isRegionActive();
  add_region_runtime(root);

  JsonArray members = root["members"].to<JsonArray>();
  for (int i = 0; i < storage::store->region->layout.memberCount; i++) {
    JsonObject member = members.add<JsonObject>();
    member["index"] = i;
    member["model"] = storage::convertModelToStr(storage::store->region->layout.members[i].modelNum);
    member["modelNum"] = storage::store->region->layout.members[i].modelNum;
    member["mac"] = storage::convertMacToStr(storage::store->region->layout.members[i].mac);
    member["master"] = i == storage::store->region->layout.masterIndex;
    member["self"] = i == storage::store->region->selfIndex;
  }
#else
  root["active"] = false;
  root["members"].to<JsonArray>();
#endif
}

bool ApiCoreV1::apply_region(JsonObject root) {
#ifdef GSMART_FEATURE_REGION
  const uint16_t old_udp_channel = storage::store->region->metadata.udpChannel;

  std::string region_id = json_string(root["regionId"]);
  if (region_id.empty())
    region_id = json_string(root["serial"]);
  if (!region_id.empty())
    storage::store->region->layout.serial = storage::convertRegionSerialtoNum(region_id);

  storage::store->region->loadMetadataFromJson(root);

  if (root["members"].is<JsonArray>()) {
    JsonDocument compact_doc;
    JsonObject compact = compact_doc.to<JsonObject>();
    JsonArray compact_members = compact["mem"].to<JsonArray>();

    uint8_t master_index = 0;
    if (!root["masterIndex"].isNull())
      master_index = root["masterIndex"].as<uint8_t>();
    const std::string master_mac = json_string(root["masterMac"]);

    uint8_t index = 0;
    for (JsonVariant item : root["members"].as<JsonArray>()) {
      if (index >= 16)
        break;

      std::string model = json_string(item["model"]);
      if (model.empty())
        model = json_string(item["b"]);
      if (model.empty() && !item["modelNum"].isNull())
        model = storage::convertModelToStr(item["modelNum"].as<uint8_t>());

      std::string mac = json_string(item["mac"]);
      if (mac.empty())
        mac = json_string(item["m"]);
      if (mac.empty())
        continue;

      JsonObject member = compact_members.add<JsonObject>();
      member["b"] = model;
      member["m"] = mac;

      if (!master_mac.empty() && normalize_token(mac) == normalize_token(master_mac))
        master_index = index;
      index++;
    }

    compact["mst"] = master_index;
    storage::store->region->reloadFromJson(compact);
  } else {
    storage::store->region->reloadFromJson(root);
  }

  storage::store->region->bumpConfigVersion();
  storage::store->region->save();

#ifdef USE_UDPSERVER
  if (udp_server::udpServer != nullptr) {
    if (old_udp_channel != 0 && old_udp_channel != storage::store->region->metadata.udpChannel)
      udp_server::udpServer->sendRegionLayoutPush(false);
    else if (old_udp_channel == 0)
      udp_server::udpServer->sendRegionLayoutPush(true);

    if (storage::store->region->metadata.udpChannel != old_udp_channel)
      udp_server::udpServer->changeChannel(storage::store->region->metadata.udpChannel);

    udp_server::udpServer->sendRegionLayoutPush(storage::store->region->metadata.udpChannel == 0);
    udp_server::udpServer->sendSituationInfo();
    udp_server::udpServer->sendReconfig(udp_server::PacketReconfig{});
  }
#endif
  return true;
#endif
  return false;
}

void ApiCoreV1::build_region_devices(JsonObject root) {
#if defined(USE_UDPSERVER) && defined(GSMART_EMITTER)
  udp_server::udpServer->GlobalDevices.toJson(root);
#else
  root["devices"].to<JsonArray>();
#endif
}

void ApiCoreV1::ping_region() {
#ifdef USE_UDPSERVER
  if (udp_server::udpServer != nullptr)
    udp_server::udpServer->sendPingReq();
#endif
}

void ApiCoreV1::handle_region_ping(JsonObject root, JsonObject response) {
  this->ping_region();

  response["ok"] = true;
  response["sent"] = true;
  if (!root["regionId"].isNull())
    response["regionId"] = root["regionId"].as<std::string>();

  JsonArray responses = response["responses"].to<JsonArray>();
#if defined(USE_UDPSERVER) && defined(GSMART_EMITTER)
  if (udp_server::udpServer != nullptr) {
    const uint32_t now = millis();
    for (size_t i = 0; i < udp_server::udpServer->GlobalDevices.ItemsCount; i++) {
      auto *item = udp_server::udpServer->GlobalDevices.Items[i];
      if (item == nullptr)
        continue;

      JsonObject row = responses.add<JsonObject>();
      item->toJson(row);
      const uint32_t age = now >= item->last_update ? now - item->last_update : 0;
      row["ageMs"] = age;
      row["online"] = age < 300000;
    }
  }
#endif
}

void ApiCoreV1::build_scheduler(JsonObject root) {
#ifdef GSMART_FEATURE_SCHEDULE
  if (storage::store->schedule != nullptr) {
    storage::store->schedule->toJson(root);
    root["enabled"] = storage::store->schedule->enabled;
    root["itemsCount"] = storage::store->schedule->schedule.size();
  } else {
    root["enabled"] = false;
    root["frames"].to<JsonArray>();
  }
#else
  root["enabled"] = false;
  root["frames"].to<JsonArray>();
#endif
}

bool ApiCoreV1::apply_scheduler(JsonObject root) {
#ifdef GSMART_FEATURE_SCHEDULE
  if (storage::store->schedule != nullptr) {
    storage::store->schedule->reloadFromJson(root);
    return true;
  }
#endif
  return false;
}

bool ApiCoreV1::apply_scheduler_state(JsonObject root) {
#ifdef GSMART_FEATURE_SCHEDULE
  if (storage::store->schedule != nullptr && !root["enabled"].isNull()) {
    bool enabled = json_bool(root["enabled"], storage::store->schedule->enabled);
    storage::store->schedule->enabled = enabled;
    storage::store->schedule->saveToFile();
    return true;
  }
#endif
  return false;
}

bool ApiCoreV1::handle_control_mode(JsonObject root, JsonObject response) {
  if (root["mode"].isNull()) {
    response["ok"] = false;
    response["error"] = "invalid_mode";
    response["message"] = "Missing required field: mode.";
    return false;
  }

  const auto mode = radiation_mode_from_api(root["mode"]);
  const std::string scope = normalize_token(json_string(root["scope"], "device"));

  if (scope == "device" || scope.empty()) {
    set_radiation_request_cause(root);
    storage::store->setActiveRadiationMode(time(nullptr), mode, storage::RadiationSource::EXT);
    response["ok"] = true;
    response["mode"] = radiation_mode_to_api(mode);
    response["scope"] = "device";
    response["applied"] = "device";
    return true;
  }

  if (scope != "region") {
    response["ok"] = false;
    response["error"] = "invalid_scope";
    response["message"] = "Supported scopes are device and region.";
    return false;
  }

#ifndef GSMART_FEATURE_REGION
  response["ok"] = false;
  response["error"] = "region_not_available";
  response["message"] = "Region feature is not enabled in this firmware.";
  return false;
#elif !defined(USE_UDPSERVER)
  response["ok"] = false;
  response["error"] = "region_transport_not_available";
  response["message"] = "UDP region transport is not enabled in this firmware.";
  return false;
#else
  if (!storage::store->region->isRegionActive()) {
    response["ok"] = false;
    response["error"] = "region_not_active";
    response["message"] = "Device is not assigned to an active region.";
    return false;
  }

  if (!storage::store->region->isMaster()) {
    response["ok"] = false;
    response["error"] = "not_region_master";
    response["message"] = "Only the region master can control the whole region.";
    return false;
  }

  if (udp_server::udpServer == nullptr) {
    response["ok"] = false;
    response["error"] = "region_transport_not_ready";
    response["message"] = "UDP region transport is not ready.";
    return false;
  }

  set_radiation_request_cause(root);
  storage::store->setActiveRadiationMode(time(nullptr), mode, storage::RadiationSource::REGION);
  udp_server::udpServer->sendControlRadiation(mode, udp_server::KindRadiationSource::REGION_MASTER);
  udp_server::udpServer->sendSituationInfo();

  response["ok"] = true;
  response["mode"] = radiation_mode_to_api(mode);
  response["scope"] = "region";
  response["applied"] = "region";
  response["sent"] = true;
  response["master"] = true;
  response["regionId"] = storage::convertRegionSerialtoStr(storage::store->region->layout.serial);
  JsonObject region = response["region"].to<JsonObject>();
  region["regionId"] = storage::convertRegionSerialtoStr(storage::store->region->layout.serial);
  region["active"] = storage::store->region->isRegionActive();
  region["isMaster"] = true;
  add_region_runtime(region);
  return true;
#endif
}

void ApiCoreV1::handle_identify(JsonObject root, JsonObject response) {
  IdentifyRequest identify;
  identify.target_mac = json_string(root["targetMac"]);
  identify.pattern = json_string(root["pattern"], "default");
  identify.sound = json_string(root["sound"], "identify");
  identify.sound_enabled = json_bool(root["sound"], true);
  identify.light = json_bool(root["light"], true);
  if (!root["durationSec"].isNull())
    identify.duration_sec = root["durationSec"].as<uint32_t>();
  else if (!root["duration"].isNull())
    identify.duration_sec = root["duration"].as<uint32_t>();
  if (identify.duration_sec == 0)
    identify.duration_sec = 3;

  bool matches = true;
  if (!identify.target_mac.empty()) {
    matches = normalize_mac_token(identify.target_mac) == normalize_mac_token(local_mac_string());
  }

  if (!matches) {
    response["ok"] = false;
    response["error"] = "target_mismatch";
    response["message"] = "targetMac does not match this device.";
    return;
  }

  this->trigger_identify(identify);

  response["ok"] = true;
  response["triggered"] = true;
  response["targetMac"] = identify.target_mac;
  response["pattern"] = identify.pattern;
  response["durationSec"] = identify.duration_sec;
  response["sound"] = identify.sound;
  response["soundEnabled"] = identify.sound_enabled;
  response["light"] = identify.light;
}

void ApiCoreV1::handle_restart(JsonObject root, JsonObject response) {
  (void) root;
  static constexpr uint32_t RESTART_DELAY_MS = 500;
  this->set_timeout("api_control_restart", RESTART_DELAY_MS, []() { App.safe_reboot(); });

  response["ok"] = true;
  response["rebootScheduled"] = true;
  response["delayMs"] = RESTART_DELAY_MS;
}

void ApiCoreV1::handle_factory_reset(JsonObject root, JsonObject response) {
  if (!require_confirmation(root, response, "FACTORY_RESET"))
    return;

  static constexpr uint32_t RESTART_DELAY_MS = 750;
  storage::FactoryResetResult result;
  if (storage::store != nullptr) {
    result = storage::store->factory_reset(RESTART_DELAY_MS);
  } else {
    result.preferencesCleared = global_preferences != nullptr && global_preferences->reset();
    result.rebootScheduled = true;
    result.delayMs = RESTART_DELAY_MS;
    this->set_timeout("api_control_factory_reset", RESTART_DELAY_MS, []() { App.safe_reboot(); });
  }

  response["ok"] = true;
  response["factoryReset"] = true;
  response["preferencesCleared"] = result.preferencesCleared;
  response["filesystemCleared"] = result.filesystemCleared;
  response["rebootScheduled"] = result.rebootScheduled;
  response["delayMs"] = result.delayMs;
}

void ApiCoreV1::handle_service_ap(JsonObject root, JsonObject response) {
  auto *mgr = gsmart_wifi_manager::global_gsmart_wifi_manager;
  if (mgr == nullptr) {
    response["ok"] = false;
    response["error"] = "wifi_manager_not_available";
    response["message"] = "GSmart Wi-Fi manager is not enabled in this firmware.";
    return;
  }

  if (root["enabled"].isNull()) {
    response["ok"] = false;
    response["error"] = "missing_enabled";
    response["message"] = "Missing required field: enabled.";
    return;
  }

  const bool enabled = json_bool(root["enabled"], false);
  const bool has_duration_sec = !root["durationSec"].isNull();
  const bool has_timeout_min = !root["timeout_min"].isNull();
  uint32_t duration_sec = 0;
  int32_t timeout_min = mgr->get_service_ap_settings().manual_timeout_min;
  if (has_duration_sec) {
    const int32_t requested = root["durationSec"].as<int32_t>();
    duration_sec = requested > 0 ? static_cast<uint32_t>(requested) : 0;
  } else if (has_timeout_min) {
    timeout_min = root["timeout_min"].as<int32_t>();
    if (timeout_min < 0) {
      response["ok"] = false;
      response["error"] = "invalid_timeout_min";
      response["message"] = "timeout_min must be 0 or greater.";
      return;
    }
  }

  if (enabled) {
    if (has_duration_sec)
      mgr->start_service_ap_for_duration(json_string(root["password"]), duration_sec);
    else
      mgr->start_service_ap(json_string(root["password"]), timeout_min);
  } else {
    mgr->stop_service_ap();
  }

  response["ok"] = true;
  response["enabled"] = enabled;
  response["active"] = mgr->is_service_ap_active();
  if (has_duration_sec)
    response["timeout_min"] = nullptr;
  else
    response["timeout_min"] = timeout_min;
  response["durationSec"] = has_duration_sec ? duration_sec : mgr->get_service_ap_auto_off_remaining_sec();
  response["permanent"] = enabled && !mgr->is_service_ap_auto_off_scheduled();
  response["autoOffScheduled"] = enabled && mgr->is_service_ap_auto_off_scheduled();
  response["autoOffRemainingSec"] = mgr->get_service_ap_auto_off_remaining_sec();
}

void ApiCoreV1::handle_clear_region(JsonObject root, JsonObject response) {
  if (!require_confirmation(root, response, "CLEAR_REGION"))
    return;

#ifdef GSMART_FEATURE_REGION
  if (storage::store == nullptr || storage::store->region == nullptr) {
    response["ok"] = false;
    response["error"] = "region_not_ready";
    return;
  }

  storage::store->region->clear();
#ifdef USE_UDPSERVER
  if (udp_server::udpServer != nullptr)
    udp_server::udpServer->changeChannel(0);
#endif

  response["ok"] = true;
  response["cleared"] = true;
  response["wifiPreserved"] = true;
#else
  response["ok"] = false;
  response["error"] = "region_not_available";
  response["message"] = "Region feature is not enabled in this firmware.";
#endif
}

void ApiCoreV1::handle_clear_usage(JsonObject root, JsonObject response) {
  if (!require_confirmation(root, response, "CLEAR_USAGE"))
    return;

#ifdef GSMART_FEATURE_USAGE
  if (storage::store == nullptr || storage::store->usage == nullptr) {
    response["ok"] = false;
    response["error"] = "usage_not_ready";
    return;
  }

  storage::store->usage->clear();
  response["ok"] = true;
  response["cleared"] = true;
  response["factoryReset"] = false;
#else
  response["ok"] = false;
  response["error"] = "usage_not_available";
  response["message"] = "Usage feature is not enabled in this firmware.";
#endif
}

bool ApiCoreV1::handle_api_config(JsonObject root, JsonObject response) {
  bool changed = false;
  if (!root["wifi_ssid"].isNull()) {
    std::string ssid = root["wifi_ssid"].as<std::string>();
    std::string password = json_string(root["wifi_password"]);
    if (gsmart_wifi_manager::global_gsmart_wifi_manager != nullptr)
      gsmart_wifi_manager::global_gsmart_wifi_manager->set_sta_customer_primary(ssid, password);
    changed = true;
  }
  if (!root["region"].isNull()) {
    int region_serial = root["region"].as<int>();
#ifdef GSMART_FEATURE_REGION
    storage::store->region->layout.serial = region_serial;
    storage::store->region->save();
#endif
    changed = true;
  }
  if (!root["mode"].isNull()) {
    std::string mode = root["mode"].as<std::string>();
#ifdef GSMART_FEATURE_REGION
    if (mode == "master") {
      if (storage::isEmitterModel(storage::store->get_model_num()) && storage::store->region->selfIndex >= 0) {
        storage::store->region->layout.masterIndex = storage::store->region->selfIndex;
        storage::store->region->normalizeMasterIndex();
        storage::store->region->save();
        changed = true;
      } else {
        ESP_LOGW(TAG, "Ignoring region master request: only emitter models can be region master.");
      }
    }
#endif
  }

  response["ok"] = true;
  response["applied"] = changed;
  if (changed)
    this->sync_preferences_now_();
  return changed;
}

bool ApiCoreV1::handle_api_manual_control(JsonObject root, JsonObject response) {
  if (root["command"].isNull()) {
    response["ok"] = false;
    response["error"] = "missing_command";
    return false;
  }
  std::string cmd = normalize_token(root["command"].as<std::string>());
  storage::RadiationMode mode = (cmd == "on") ? storage::RadiationMode::STD : storage::RadiationMode::OFF;
  set_radiation_request_cause(root);
  storage::store->setActiveRadiationMode(time(nullptr), mode, storage::RadiationSource::EXT);

  response["ok"] = true;
  response["mode"] = radiation_mode_to_api(mode);
  return true;
}

// --- Settings: Consumables ---
void ApiCoreV1::build_settings_consumables(JsonObject root) {
#if defined(GSMART_FEATURE_USAGE) && defined(GSMART_EMITTER)
  if (storage::store == nullptr || storage::store->usage == nullptr) return;
  for (int i = 0; i < DEVICE_MAX_LAMP; i++) {
    std::string key = (i == 0) ? "lampA" : ((i == 1) ? "lampB" : (std::string("lamp") + std::to_string(i)));
    JsonObject lampJson = root[key].to<JsonObject>();
    auto &pref = storage::store->usage->lamp[i].pref;
    lampJson["maxHours"] = pref.lastExchangeLiveHour;
    lampJson["powerWatts"] = pref.powerWatts;
    lampJson["burnedHours"] = pref.onSec / 3600;
    lampJson["firstUseDate"] = pref.lastExchangeDate;
  }
#else
  root["enabled"] = false;
#endif
}

bool ApiCoreV1::apply_settings_consumables(JsonObject root) {
#if defined(GSMART_FEATURE_USAGE) && defined(GSMART_EMITTER)
  if (storage::store == nullptr || storage::store->usage == nullptr) return false;
  bool changed = false;
  for (int i = 0; i < DEVICE_MAX_LAMP; i++) {
    std::string key = (i == 0) ? "lampA" : ((i == 1) ? "lampB" : (std::string("lamp") + std::to_string(i)));
    if (!root[key].isNull()) {
      auto lmp = root[key].as<JsonObject>();
      auto &pref = storage::store->usage->lamp[i].pref;
      if (!lmp["maxHours"].isNull()) pref.lastExchangeLiveHour = lmp["maxHours"].as<uint32_t>();
      if (!lmp["powerWatts"].isNull()) pref.powerWatts = lmp["powerWatts"].as<uint16_t>();
      if (!lmp["burnedHours"].isNull()) pref.onSec = lmp["burnedHours"].as<uint32_t>() * 3600;
      if (!lmp["firstUseDate"].isNull()) pref.lastExchangeDate = lmp["firstUseDate"].as<uint32_t>();
      changed = true;
    }
  }
  if (changed) {
    storage::store->usage->save();
    this->sync_preferences_now_();
  }
  return changed;
#else
  return false;
#endif
}

// --- Settings: Modes ---
void ApiCoreV1::build_settings_modes(JsonObject root) {
#ifdef GSMART_FEATURE_FILESYSTEM
  if (storage::store == nullptr || storage::store->settingsMode == nullptr) return;
  storage::store->settingsMode->toJson(root);
#else
  root["enabled"] = false;
#endif
}

bool ApiCoreV1::apply_settings_modes(JsonObject root) {
#ifdef GSMART_FEATURE_FILESYSTEM
  if (storage::store == nullptr || storage::store->settingsMode == nullptr) return false;
  storage::store->settingsMode->fromJson(root);
  storage::store->settingsMode->saveToFile();
  return true;
#else
  return false;
#endif
}

}  // namespace api_core_v1
}  // namespace esphome
