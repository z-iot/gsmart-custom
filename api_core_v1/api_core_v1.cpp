#include "api_core_v1.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include "payloads.h"
#include "esphome/components/storage/store.h"
#include "esphome/components/storage/util.h"
#include "esphome/components/wifi/wifi_component.h"
#ifdef ESP32
#include <esp_wifi.h>
#endif

#include "gsmart_wifi_manager/gsmart_wifi_manager.h"

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

void add_wifi_runtime(JsonObject root) {
  if (wifi::global_wifi_component == nullptr)
    return;

  root["connected"] = wifi::global_wifi_component->is_connected();
  root["apActive"] = esphome::wifi::global_wifi_component->is_ap_active();
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

void add_lamps_status(JsonObject root) {
  root["lampState"] = false;
  root["lampStateA"] = false;
  root["lampStateB"] = false;

  JsonArray lamps = root["lamps"].to<JsonArray>();

#if defined(GSMART_FEATURE_USAGE) && defined(GSMART_EMITTER)
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
  root["motion"] = false;

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

void add_fans_status(JsonObject root) {
  root["fanSpeedA"] = 0;
  root["fanSpeedB"] = 0;

  JsonArray fans = root["fans"].to<JsonArray>();

#if defined(GSMART_FEATURE_USAGE) && defined(GSMART_EMITTER)
  int fan_count = storage::store->usage->beam.pref.fanCount;
  if (fan_count < 0 || fan_count > DEVICE_MAX_FAN)
    fan_count = DEVICE_MAX_FAN;

  for (int i = 0; i < fan_count; i++) {
    const bool running = storage::store->usage->fan[i].lastStart > storage::store->usage->fan[i].lastStop;
    JsonObject fan = fans.add<JsonObject>();
    fan["channel"] = i + 1;
    fan["running"] = running;
    fan["speed"] = 0;
    fan["rotationCount"] = storage::store->usage->fan[i].rotationCount;
    fan["onSec"] = storage::store->usage->fan[i].onSec;
    fan["startCount"] = storage::store->usage->fan[i].startCount;
    fan["stopCount"] = storage::store->usage->fan[i].stopCount;
    fan["lastStartSec"] = storage::store->usage->fan[i].lastStart;
    fan["lastStopSec"] = storage::store->usage->fan[i].lastStop;
  }
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

}  // namespace

void ApiCoreV1::build_info(JsonObject root) {
  uint8_t mac[6];
  get_mac_address_raw(mac);

  uint8_t build_hi = 0;
  uint8_t build_lo = 0;
  storage::store->getBuildNumber(build_hi, build_lo);

  root["api"] = "mobile.v1";
  root["model"] = storage::store->get_model();
  root["modelNum"] = storage::store->get_model_num();
  root["serial"] = storage::store->get_serial();
  root["mac"] = storage::convertMacToStr(mac);
  root["name"] = std::string("G-Smart-") + storage::store->get_serial();
  root["fwVersion"] = str_sprintf("%u.%u", build_hi, build_lo);
  root["build"] = str_sprintf("%u.%u", build_hi, build_lo);

  add_wifi_runtime(root);

  JsonObject capabilities = root["capabilities"].to<JsonObject>();
  capabilities["control"] = true;
  capabilities["diagnostics"] = true;
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
  add_fans_status(root);
  add_error_status(root);

  JsonObject situation = root["situation"].to<JsonObject>();
  add_situation(situation);

#ifdef GSMART_FEATURE_REGION
  JsonObject region = root["region"].to<JsonObject>();
  region["active"] = storage::store->region->isRegionActive();
  region["isMaster"] = storage::store->region->isMaster();
  region["selfIndex"] = storage::store->region->selfIndex;
  region["masterIndex"] = storage::store->region->layout.masterIndex;
#endif
}

void ApiCoreV1::build_diagnostics(JsonObject root) {
  root["model"] = storage::store->get_model();
  root["serial"] = storage::store->get_serial();
  root["uptimeSec"] = millis() / 1000;

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

  JsonObject firmware = root["firmware"].to<JsonObject>();
#ifdef ESP32
  firmware["platform"] = "esp32";
#elif defined(ESP8266)
  firmware["platform"] = "esp8266";
#else
  firmware["platform"] = "unknown";
#endif
  firmware["cpuFreqMhz"] = ESP.getCpuFreqMHz();
  firmware["sketchSize"] = ESP.getSketchSize();
  firmware["freeSketchSpace"] = ESP.getFreeSketchSpace();
  firmware["sdkVersion"] = ESP.getSdkVersion();
  firmware["flashChipSize"] = ESP.getFlashChipSize();
  firmware["flashChipSpeed"] = ESP.getFlashChipSpeed();

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
  add_fans_status(root);
  add_error_status(root);

  JsonArray relays = root["relays"].to<JsonArray>();
  JsonArray triacs = root["triacs"].to<JsonArray>();
#if defined(GSMART_FEATURE_USAGE) && defined(GSMART_EMITTER)
  int lamp_count = storage::store->usage->beam.pref.lampCount;
  if (lamp_count < 0 || lamp_count > DEVICE_MAX_LAMP)
    lamp_count = DEVICE_MAX_LAMP;
  for (int i = 0; i < lamp_count; i++) {
    JsonObject relay = relays.add<JsonObject>();
    relay["channel"] = i + 1;
    relay["diagnosticAvailable"] = false;

    JsonObject triac = triacs.add<JsonObject>();
    triac["channel"] = i + 1;
    triac["diagnosticAvailable"] = false;
  }
#endif
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

  JsonObject sta = root["sta"].to<JsonObject>();
  const auto &settings = mgr->get_settings();

  auto add_net = [&](JsonObject obj, const char *ssid, const char *pswd) {
    obj["ssid"] = ssid;
    obj["password_set"] = (pswd[0] != 0);
  };

  add_net(sta["service"].to<JsonObject>(), settings.service_ssid, settings.service_password);
  add_net(sta["customer_primary"].to<JsonObject>(), settings.customer_primary_ssid, settings.customer_primary_password);
  add_net(sta["customer_secondary"].to<JsonObject>(), settings.customer_secondary_ssid, settings.customer_secondary_password);

  JsonObject soft_ap = root["soft_ap"].to<JsonObject>();

  auto add_ap = [&](JsonObject obj, const char *ssid, const char *pswd, bool enabled) {
    obj["ssid"] = ssid;
    obj["password_set"] = (pswd[0] != 0);
    obj["enabled"] = enabled;
  };

  add_ap(soft_ap["service_ap"].to<JsonObject>(), settings.service_ap_ssid, settings.service_ap_password,
         settings.service_ap_enabled);
  auto region_ap = soft_ap["region_ap"].to<JsonObject>();
  add_ap(region_ap, settings.region_ap_ssid, settings.region_ap_password, settings.region_ap_enabled);
  region_ap["sta_policy"] = (settings.region_ap_sta_policy == 1 ? "ap_only" : "apsta");
}

bool ApiCoreV1::apply_network(JsonObject root) {
  auto *mgr = gsmart_wifi_manager::global_gsmart_wifi_manager;
  if (mgr == nullptr)
    return false;

  bool changed = false;

  // Legacy mapping
  if (!root["wifi_ssid"].isNull()) {
    mgr->set_sta_customer_primary(root["wifi_ssid"].as<std::string>(), json_string(root["wifi_password"]));
    changed = true;
  }

  if (root["client"].is<JsonObject>()) {
    JsonObject client = root["client"].as<JsonObject>();
    std::string ssid = json_string(client["ssid"]);
    std::string password = json_string(client["password"]);
    if (!ssid.empty()) {
      mgr->set_sta_customer_primary(ssid, password);
      changed = true;
    }
  }

  // New API
  if (root["sta"].is<JsonObject>()) {
    JsonObject sta = root["sta"].as<JsonObject>();
    if (sta["service"].is<JsonObject>()) {
      JsonObject s = sta["service"].as<JsonObject>();
      mgr->set_sta_service(json_string(s["ssid"]), json_string(s["password"]));
      changed = true;
    }
    if (sta["customer_primary"].is<JsonObject>()) {
      JsonObject s = sta["customer_primary"].as<JsonObject>();
      mgr->set_sta_customer_primary(json_string(s["ssid"]), json_string(s["password"]));
      changed = true;
    }
    if (sta["customer_secondary"].is<JsonObject>()) {
      JsonObject s = sta["customer_secondary"].as<JsonObject>();
      mgr->set_sta_customer_secondary(json_string(s["ssid"]), json_string(s["password"]));
      changed = true;
    }
  }

  if (root["soft_ap"].is<JsonObject>()) {
    JsonObject soft_ap = root["soft_ap"].as<JsonObject>();
    if (soft_ap["service_ap"].is<JsonObject>()) {
      JsonObject s = soft_ap["service_ap"].as<JsonObject>();
      mgr->set_service_ap(json_string(s["ssid"]), json_string(s["password"]), json_bool(s["enabled"], true));
      changed = true;
    }
    if (soft_ap["region_ap"].is<JsonObject>()) {
      JsonObject s = soft_ap["region_ap"].as<JsonObject>();
      uint8_t policy = (json_string(s["sta_policy"]) == "ap_only" ? 1 : 0);
      mgr->set_region_ap(json_string(s["ssid"]), json_string(s["password"]), json_bool(s["enabled"], false), policy);
      changed = true;
    }
  }

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
  root["masterIndex"] = storage::store->region->layout.masterIndex;
  root["selfIndex"] = storage::store->region->selfIndex;
  root["isMaster"] = storage::store->region->isMaster();
  root["active"] = storage::store->region->isRegionActive();

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
  std::string region_id = json_string(root["regionId"]);
  if (region_id.empty())
    region_id = json_string(root["serial"]);
  if (!region_id.empty())
    storage::store->region->layout.serial = storage::convertRegionSerialtoNum(region_id);

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
    return true;
  } else {
    storage::store->region->reloadFromJson(root);
    return true;
  }
#endif
  return false;
}

void ApiCoreV1::build_region_devices(JsonObject root) {
#ifdef USE_UDPSERVER
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

  storage::store->setActiveRadiationMode(time(nullptr), mode, storage::RadiationSource::EXT);
  udp_server::udpServer->sendControlRadiation(mode, udp_server::KindRadiationSource::SOURCE_EXT);

  response["ok"] = true;
  response["mode"] = radiation_mode_to_api(mode);
  response["scope"] = "region";
  response["applied"] = "region";
  response["sent"] = true;
  response["master"] = true;
  response["regionId"] = storage::convertRegionSerialtoStr(storage::store->region->layout.serial);
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

bool ApiCoreV1::handle_api_config(JsonObject root, JsonObject response) {
  bool changed = false;
  if (!root["wifi_ssid"].isNull()) {
    std::string ssid = root["wifi_ssid"].as<std::string>();
    std::string password = json_string(root["wifi_password"]);
    wifi::global_wifi_component->save_wifi_sta(ssid, password);
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
      storage::store->region->layout.masterIndex = storage::store->region->selfIndex;
      storage::store->region->save();
    }
#endif
    changed = true;
  }

  response["ok"] = true;
  response["applied"] = changed;
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
  storage::store->setActiveRadiationMode(time(nullptr), mode, storage::RadiationSource::EXT);

  response["ok"] = true;
  response["mode"] = radiation_mode_to_api(mode);
  return true;
}

// --- Settings: Consumables ---
void ApiCoreV1::build_settings_consumables(JsonObject root) {
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
}

bool ApiCoreV1::apply_settings_consumables(JsonObject root) {
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
  }
  return changed;
}

// --- Settings: Modes ---
void ApiCoreV1::build_settings_modes(JsonObject root) {
  if (storage::store == nullptr || storage::store->settingsMode == nullptr) return;
  storage::store->settingsMode->toJson(root);
}

bool ApiCoreV1::apply_settings_modes(JsonObject root) {
  if (storage::store == nullptr || storage::store->settingsMode == nullptr) return false;
  storage::store->settingsMode->fromJson(root);
  storage::store->settingsMode->saveToFile();
  return true;
}

}  // namespace api_core_v1
}  // namespace esphome
