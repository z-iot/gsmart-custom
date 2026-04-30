#include "mobile_api.h"

#include "esphome/components/gsmart_server/payloads.h"
#include "esphome/components/gsmart_server/web_helpers.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/storage/store.h"
#include "esphome/components/storage/util.h"
#include "esphome/components/wifi/wifi_component.h"

#ifdef USE_MQTT
#include "esphome/components/mqtt/mqtt_client.h"
#endif

#ifdef USE_UDPSERVER
#include "esphome/components/udp_server/udp_server.h"
#endif

#include <algorithm>
#include <cctype>
#include <ctime>
#include <functional>
#include <string>

namespace esphome {
namespace mobile_api {

namespace gs = esphome::gsmart_server;

namespace {

constexpr const char *MOBILE_API_PREFIX = "/api/mobile/v1";

std::string api_uri(const char *path) { return std::string(MOBILE_API_PREFIX) + path; }

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
    default:
      return "none";
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
      default:
        return storage::RadiationMode::NONE;
    }
  }

  std::string mode = normalize_token(value.as<std::string>());
  if (mode == "min" || mode == "minimum")
    return storage::RadiationMode::MIN;
  if (mode == "std" || mode == "standard" || mode == "normal")
    return storage::RadiationMode::STD;
  if (mode == "max" || mode == "maximum")
    return storage::RadiationMode::MAX;
  return storage::RadiationMode::NONE;
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

void send_json(AsyncWebServerRequest *request, std::function<void(JsonObject)> builder, int status = 200) {
  std::string data = esphome::json::build_json([&builder](JsonObject root) { builder(root); });
  request->send(status, "application/json", data.c_str());
}

void send_error(AsyncWebServerRequest *request, int status, const char *code, const char *message) {
  send_json(
      request,
      [code, message](JsonObject root) {
        root["ok"] = false;
        root["error"] = code;
        root["message"] = message;
      },
      status);
}

void send_ok(AsyncWebServerRequest *request, std::function<void(JsonObject)> extra = nullptr) {
  send_json(request, [extra](JsonObject root) {
    root["ok"] = true;
    if (extra)
      extra(root);
  });
}

void register_json_get(const std::shared_ptr<AsyncWebServer> &server, const std::string &uri,
                       std::function<void(JsonObject)> builder) {
  gs::on(server, uri.c_str(), HTTP_GET, [builder](AsyncWebServerRequest *request) { send_json(request, builder); });
}

void register_json_post(const std::shared_ptr<AsyncWebServer> &server, const std::string &uri,
                        std::function<void(AsyncWebServerRequest *, JsonObject)> handler) {
  gs::on_post_json(server, uri.c_str(),
                   [handler](AsyncWebServerRequest *request, JsonObject root) { handler(request, root); });
}

void register_post_not_implemented(const std::shared_ptr<AsyncWebServer> &server, const std::string &uri,
                                   const char *feature) {
  gs::on(server, uri.c_str(), HTTP_POST, [feature](AsyncWebServerRequest *request) {
    send_error(request, 501, "not_implemented", feature);
  });
}

void add_wifi_runtime(JsonObject root) {
  if (wifi::global_wifi_component == nullptr)
    return;

  root["connected"] = wifi::global_wifi_component->is_connected();
  root["apActive"] = wifi::global_wifi_component->isApActive();
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

void build_info(JsonObject root) {
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

void build_status(JsonObject root) {
  const auto active_mode = storage::store->global->radiation.activeMode;
  root["model"] = storage::store->get_model();
  root["serial"] = storage::store->get_serial();
  root["mode"] = radiation_mode_to_api(active_mode);
  root["radiate"] = active_mode != storage::RadiationMode::NONE;
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

void build_diagnostics(JsonObject root) {
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
  filesystem["total"] = storage::store->fileSystem->GetTotalBytes();
  filesystem["used"] = storage::store->fileSystem->GetUsedBytes();
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

void build_network(JsonObject root) {
  gs::payloads::config_connect_json(root);

  if (root["ap"].is<JsonObject>())
    root["ap"].as<JsonObject>().remove("password");
  if (root["client"].is<JsonObject>())
    root["client"].as<JsonObject>().remove("password");

  JsonObject client = root["client"].is<JsonObject>() ? root["client"].as<JsonObject>() : root["client"].to<JsonObject>();
  add_wifi_runtime(client);
}

void apply_network(JsonObject root) {
  if (wifi::global_wifi_component == nullptr)
    return;

  if (root["client"].is<JsonObject>()) {
    JsonObject client = root["client"].as<JsonObject>();
    std::string ssid = json_string(client["ssid"]);
    std::string password = json_string(client["password"]);
    if (!ssid.empty())
      wifi::global_wifi_component->save_wifi_sta(ssid, password);
  }

  if (root["ap"].is<JsonObject>()) {
    JsonObject ap = root["ap"].as<JsonObject>();
    if (!ap["enabled"].isNull()) {
      if (json_bool(ap["enabled"], wifi::global_wifi_component->isApActive()))
        wifi::global_wifi_component->enableAp();
      else
        wifi::global_wifi_component->disableAp();
    }
  }
}

void build_mqtt(JsonObject root) {
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

void build_consumption(JsonObject root) {
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

void build_region(JsonObject root) {
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

void apply_region(JsonObject root) {
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
  } else {
    storage::store->region->reloadFromJson(root);
  }
#endif
}

void build_region_devices(JsonObject root) {
#ifdef USE_UDPSERVER
  udp_server::udpServer->GlobalDevices.toJson(root);
#else
  root["devices"].to<JsonArray>();
#endif
}

void ping_region(JsonObject root) {
#ifdef USE_UDPSERVER
  udp_server::udpServer->sendPingReq();
#endif
}

IdentifyRequest identify_request_from_json(JsonObject root) {
  IdentifyRequest request;
  request.target_mac = json_string(root["targetMac"]);
  request.pattern = json_string(root["pattern"], "default");
  request.sound = json_string(root["sound"], "identify");
  request.sound_enabled = json_bool(root["sound"], true);
  request.light = json_bool(root["light"], true);
  if (!root["durationSec"].isNull())
    request.duration_sec = root["durationSec"].as<uint32_t>();
  else if (!root["duration"].isNull())
    request.duration_sec = root["duration"].as<uint32_t>();
  if (request.duration_sec == 0)
    request.duration_sec = 3;
  return request;
}

bool identify_target_matches_this_device(const IdentifyRequest &identify) {
  if (identify.target_mac.empty())
    return true;
  return normalize_mac_token(identify.target_mac) == normalize_mac_token(local_mac_string());
}

void handle_control_mode(AsyncWebServerRequest *request, JsonObject root) {
  if (root["mode"].isNull()) {
    send_error(request, 400, "invalid_mode", "Missing required field: mode.");
    return;
  }

  const auto mode = radiation_mode_from_api(root["mode"]);
  const std::string scope = normalize_token(json_string(root["scope"], "device"));

  if (scope == "device" || scope.empty()) {
    storage::store->setActiveRadiationMode(time(nullptr), mode, storage::RadiationSource::EXT);
    send_ok(request, [mode](JsonObject response) {
      response["mode"] = radiation_mode_to_api(mode);
      response["scope"] = "device";
      response["applied"] = "device";
    });
    return;
  }

  if (scope != "region") {
    send_error(request, 400, "invalid_scope", "Supported scopes are device and region.");
    return;
  }

#ifndef GSMART_FEATURE_REGION
  send_error(request, 501, "region_not_available", "Region feature is not enabled in this firmware.");
  return;
#elif !defined(USE_UDPSERVER)
  send_error(request, 501, "region_transport_not_available", "UDP region transport is not enabled in this firmware.");
  return;
#else
  if (!storage::store->region->isRegionActive()) {
    send_error(request, 409, "region_not_active", "Device is not assigned to an active region.");
    return;
  }

  if (!storage::store->region->isMaster()) {
    send_error(request, 409, "not_region_master", "Only the region master can control the whole region.");
    return;
  }

  if (udp_server::udpServer == nullptr) {
    send_error(request, 503, "region_transport_not_ready", "UDP region transport is not ready.");
    return;
  }

  storage::store->setActiveRadiationMode(time(nullptr), mode, storage::RadiationSource::EXT);
  udp_server::udpServer->sendControlRadiation(mode, udp_server::KindRadiationSource::SOURCE_EXT);

  send_ok(request, [mode](JsonObject response) {
    response["mode"] = radiation_mode_to_api(mode);
    response["scope"] = "region";
    response["applied"] = "region";
    response["sent"] = true;
    response["master"] = true;
    response["regionId"] = storage::convertRegionSerialtoStr(storage::store->region->layout.serial);
  });
#endif
}

void handle_identify(MobileApi *api, AsyncWebServerRequest *request, JsonObject root) {
  IdentifyRequest identify = identify_request_from_json(root);
  if (!identify_target_matches_this_device(identify)) {
    send_error(request, 409, "target_mismatch", "targetMac does not match this device.");
    return;
  }

  api->trigger_identify(identify);

  send_ok(request, [identify](JsonObject response) {
    response["triggered"] = true;
    response["targetMac"] = identify.target_mac;
    response["pattern"] = identify.pattern;
    response["durationSec"] = identify.duration_sec;
    response["sound"] = identify.sound;
    response["soundEnabled"] = identify.sound_enabled;
    response["light"] = identify.light;
  });
}

}  // namespace

void MobileApi::setup() {
  this->base_->init();

  std::shared_ptr<AsyncWebServer> server(this->base_->get_server(), [](AsyncWebServer *) {});

  register_json_get(server, api_uri("/info"), build_info);
  register_json_get(server, api_uri("/status"), build_status);
  register_json_get(server, api_uri("/diagnostics"), build_diagnostics);
  register_json_get(server, api_uri("/consumption"), build_consumption);

  register_json_get(server, api_uri("/network"), build_network);
  register_json_post(server, api_uri("/network"), [](AsyncWebServerRequest *request, JsonObject root) {
    apply_network(root);
    send_ok(request, [](JsonObject response) { response["applied"] = true; });
  });
  register_json_get(server, api_uri("/network/mqtt"), build_mqtt);
  register_post_not_implemented(server, api_uri("/network/mqtt"), "MQTT settings storage is not implemented yet.");

  register_json_post(server, api_uri("/control/mode"), handle_control_mode);
  register_json_post(server, api_uri("/control/identify"), [this](AsyncWebServerRequest *request, JsonObject root) {
    handle_identify(this, request, root);
  });

  register_json_get(server, api_uri("/scheduler"), &gs::payloads::scheduller_json);
  register_json_post(server, api_uri("/scheduler"), [](AsyncWebServerRequest *request, JsonObject root) {
    gs::payloads::scheduller_apply(root);
    send_ok(request, [](JsonObject response) { response["saved"] = true; });
  });
  register_json_post(server, api_uri("/scheduler/state"), [](AsyncWebServerRequest *request, JsonObject root) {
#ifdef GSMART_FEATURE_SCHEDULE
    const bool enabled = !root["enabled"].isNull() ? json_bool(root["enabled"], storage::store->schedule->enabled)
                                                   : json_bool(root["state"], storage::store->schedule->enabled);
    storage::store->schedule->enabled = enabled;
    storage::store->schedule->saveToFile();
    send_ok(request, [enabled](JsonObject response) {
      response["enabled"] = enabled;
      response["state"] = enabled ? "enabled" : "disabled";
    });
#else
    send_error(request, 501, "not_available", "Scheduler feature is not enabled in this firmware.");
#endif
  });

  register_json_get(server, api_uri("/region"), build_region);
  register_json_post(server, api_uri("/region"), [](AsyncWebServerRequest *request, JsonObject root) {
    apply_region(root);
    send_ok(request, [](JsonObject response) { response["saved"] = true; });
  });
  register_json_get(server, api_uri("/region/devices"), build_region_devices);
  register_json_post(server, api_uri("/region/ping"), [](AsyncWebServerRequest *request, JsonObject root) {
    ping_region(root);
    send_ok(request, [](JsonObject response) { response["sent"] = true; });
  });
}

}  // namespace mobile_api
}  // namespace esphome
