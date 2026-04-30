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
  root["mode"] = radiation_mode_to_api(active_mode);
  root["radiate"] = active_mode != storage::RadiationMode::NONE;
  root["source"] = radiation_source_to_api(storage::store->global->radiation.lastSource);
  root["lastStartSec"] = storage::store->global->radiation.lastStart;
  root["lastStopSec"] = storage::store->global->radiation.lastStop;
  root["remainingSec"] = storage::store->getTimerDurationSec(time(nullptr));
  root["uptimeSec"] = millis() / 1000;

  add_wifi_runtime(root);

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

}  // namespace

void MobileApi::setup() {
  this->base_->init();

  std::shared_ptr<AsyncWebServer> server(this->base_->get_server(), [](AsyncWebServer *) {});

  register_json_get(server, api_uri("/info"), build_info);
  register_json_get(server, api_uri("/status"), build_status);
  register_json_get(server, api_uri("/diagnostics"), &gs::payloads::system_info_json);
  register_json_get(server, api_uri("/consumption"), build_consumption);

  register_json_get(server, api_uri("/network"), build_network);
  register_json_post(server, api_uri("/network"), [](AsyncWebServerRequest *request, JsonObject root) {
    apply_network(root);
    send_ok(request, [](JsonObject response) { response["applied"] = true; });
  });
  register_json_get(server, api_uri("/network/mqtt"), build_mqtt);
  register_post_not_implemented(server, api_uri("/network/mqtt"), "MQTT settings storage is not implemented yet.");

  register_json_post(server, api_uri("/control/mode"), [](AsyncWebServerRequest *request, JsonObject root) {
    const auto mode = radiation_mode_from_api(root["mode"]);
    storage::store->setActiveRadiationMode(time(nullptr), mode, storage::RadiationSource::EXT);
    send_ok(request, [mode](JsonObject response) {
      response["mode"] = radiation_mode_to_api(mode);
      response["scope"] = "device";
    });
  });
  register_post_not_implemented(server, api_uri("/control/identify"), "Device identify action is not implemented yet.");

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
