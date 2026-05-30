#include "payloads.h"

#include "esphome/components/storage/store.h"
#ifdef GSMART_FEATURE_SCHEDULE
#include "esphome/components/storage/settings_schedule.h"
#endif
#include "esphome/core/helpers.h"

namespace esphome {
namespace api_core_v1 {
namespace payloads {

void system_info_json(JsonObject root) {
  root["today-m3"] = 22.5;

#ifdef ESP32
  root["esp_platform"] = "esp32";
  root["max_alloc_heap"] = ESP.getMaxAllocHeap();
  root["psram_size"] = ESP.getPsramSize();
  root["free_psram"] = ESP.getFreePsram();
#elif defined(ESP8266)
  root["esp_platform"] = "esp8266";
  root["max_alloc_heap"] = ESP.getMaxFreeBlockSize();
  root["heap_fragmentation"] = ESP.getHeapFragmentation();
#endif
  root["cpu_freq_mhz"] = ESP.getCpuFreqMHz();
  root["free_heap"] = ESP.getFreeHeap();
  root["sketch_size"] = ESP.getSketchSize();
  root["free_sketch_space"] = ESP.getFreeSketchSpace();
  root["sdk_version"] = ESP.getSdkVersion();
  root["flash_chip_size"] = ESP.getFlashChipSize();
  root["flash_chip_speed"] = ESP.getFlashChipSpeed();

  JsonObject process = root["process"].to<JsonObject>();
#ifdef GSMART_FEATURE_FILESYSTEM
  root["fs_total"] = esphome::storage::store->file_system_->GetTotalBytes();
  root["fs_used"] = esphome::storage::store->file_system_->GetUsedBytes();

  JsonObject dir = root["root_dir"].to<JsonObject>();
  esphome::storage::store->file_system_->listDir(dir);
#else
  root["fs_total"] = 0;
  root["fs_used"] = 0;
#endif

#ifdef GSMART_FEATURE_SCHEDULE
  JsonObject schedule = root["schedule"].to<JsonObject>();
  esphome::storage::store->schedule->toJson(schedule);
  process["schedule_enabled"] = esphome::storage::store->schedule->enabled;
#else
  process["schedule_enabled"] = false;
#endif
#ifdef GSMART_FEATURE_REGION
  process["isRegionActive"] = esphome::storage::store->region->isRegionActive();
  process["isMaster"] = esphome::storage::store->region->isMaster();
  process["selfIndex"] = esphome::storage::store->region->selfIndex;
  process["masterIndex"] = esphome::storage::store->region->layout.masterIndex;
#endif

  uint8_t selfMac[6];
  esphome::get_mac_address_raw(selfMac);
  process["selfMac"] = esphome::storage::convertMacToStr(selfMac);

  process["activeMode"] = esphome::storage::convertRadiationModeToStr(
      esphome::storage::store->global->radiation.activeMode);

  JsonObject usage = root["usage"].to<JsonObject>();
#if defined(GSMART_FEATURE_USAGE) && defined(GSMART_EMITTER)
  esphome::storage::store->usage->fillAdvertise(usage);
#else
  usage["enabled"] = false;
#endif

  JsonObject errors = root["errors"].to<JsonObject>();
  const auto &error_state = esphome::storage::store->global->errors;
  errors["count"] = error_state.totalCount;
  errors["lastCode"] = error_state.lastCode;
  errors["lastMessage"] = error_state.lastDesc;
  errors["hasError"] = error_state.totalCount > 0;
}

void neighborhood_json(JsonObject root) {
  root["today-m3"] = 22.5;

#ifdef ESP32
  root["esp_platform"] = "esp32";
  root["max_alloc_heap"] = ESP.getMaxAllocHeap();
  root["psram_size"] = ESP.getPsramSize();
  root["free_psram"] = ESP.getFreePsram();
#elif defined(ESP8266)
  root["esp_platform"] = "esp8266";
  root["max_alloc_heap"] = ESP.getMaxFreeBlockSize();
  root["heap_fragmentation"] = ESP.getHeapFragmentation();
#endif
  root["cpu_freq_mhz"] = ESP.getCpuFreqMHz();
  root["free_heap"] = ESP.getFreeHeap();
  root["sketch_size"] = ESP.getSketchSize();
  root["free_sketch_space"] = ESP.getFreeSketchSpace();
  root["sdk_version"] = ESP.getSdkVersion();
  root["flash_chip_size"] = ESP.getFlashChipSize();
  root["flash_chip_speed"] = ESP.getFlashChipSpeed();

#ifdef GSMART_FEATURE_FILESYSTEM
  root["fs_total"] = esphome::storage::store->file_system_->GetTotalBytes();
  root["fs_used"] = esphome::storage::store->file_system_->GetUsedBytes();

  JsonObject dir = root["root_dir"].to<JsonObject>();
  esphome::storage::store->file_system_->listDir(dir);
#else
  root["fs_total"] = 0;
  root["fs_used"] = 0;
#endif
}

void features_json(JsonObject root) {
  root["Model"] = esphome::storage::store->get_model();
  root["Serial"] = esphome::storage::store->get_serial();
}

void scheduller_json(JsonObject root) {
#ifdef GSMART_FEATURE_SCHEDULE
  esphome::storage::store->schedule->toJson(root);
#else
  root["enabled"] = false;
#endif
}

void scheduller_apply(JsonObject root) {
#ifdef GSMART_FEATURE_SCHEDULE
  esphome::storage::store->schedule->reloadFromJson(root);
#endif
}

void neighbor_apply(JsonObject root) {
  // TODO: napojit na storage->region->reloadFromJson(...) ked bude UI ulozene.
}

void config_data_json(JsonObject root) {
  JsonObject network = root["network"].to<JsonObject>();
  network["wifi_login"] = "This is wifi login";
  network["ap_login"] = "This is ap login";
  network["wifi_ap_enable"] = false;
  network["timeZone"] = "Asia/Tehran";

  JsonObject region = root["region"].to<JsonObject>();
  region["group_num"] = 4;
  region["master_serial"] = "123456789";
  region["members"] = "aaabbbb, cccddd";

  JsonObject actuator = root["actuator"].to<JsonObject>();
  actuator["lock"] = false;
  actuator["btn_delays"] = 500;
  actuator["pincode"] = "1234";

  JsonObject emitter = root["emitter"].to<JsonObject>();
  emitter["lock"] = false;
  emitter["sound"] = "sound1";

  JsonObject security = root["security"].to<JsonObject>();
  security["guest_pass"] = "Guest 1234";
  security["guest_email"] = "";
  security["guest_lock"] = false;
  security["guest_pinCode"] = "1234";
  security["user_pass"] = "Guest 1234";
  security["user_email"] = "";
  security["user_lock"] = false;
  security["user_pinCode"] = "1234";

  JsonObject setup = root["setup"].to<JsonObject>();
  setup["brand"] = "Brand1";
  setup["catalog"] = "Catalog1";
  setup["brand_pos"] = 1;
  setup["lampCount"] = 23;
  setup["lampPower"] = 220;
}

void config_device_json(JsonObject root) {
  root["email"] = "promos@promos.company";
  root["keypad-lock"] = "enable";
}

void config_mode_json(JsonObject root) {
  JsonObject manual = root["manual"].to<JsonObject>();
  manual["lamp"] = "top";
  manual["fan"] = 50;
  manual["pir-mode"] = "none";
  manual["pir-delay"] = 30;
  manual["pir-runtime"] = 600;

  JsonObject eco = root["eco"].to<JsonObject>();
  eco["lamp"] = "bottom";
  eco["fan"] = 30;
  eco["pir-mode"] = "active";
  eco["pir-delay"] = 60;
  eco["pir-runtime"] = 120;

  JsonObject normal = root["normal"].to<JsonObject>();
  normal["lamp"] = "alternate";
  normal["fan"] = 70;
  normal["pir-mode"] = "active";
  normal["pir-delay"] = 10;
  normal["pir-runtime"] = 600;

  JsonObject max = root["max"].to<JsonObject>();
  max["lamp"] = "both";
  max["fan"] = 100;
  max["pir-mode"] = "inactive";
  max["pir-delay"] = 5;
  max["pir-runtime"] = 1800;
}

void config_treatment_json(JsonObject root) {
  JsonObject min = root["min"].to<JsonObject>();
  min["duration"] = 10;
  min["motion-delay"] = 30;

  JsonObject std_ = root["std"].to<JsonObject>();
  std_["duration"] = 30;
  std_["motion-delay"] = 30;

  JsonObject max = root["max"].to<JsonObject>();
  max["duration"] = 60;
  max["motion-delay"] = 60;
}

void config_security_json(JsonObject root) {
  JsonObject users = root["users"].to<JsonObject>();

  JsonObject guest = users["guest"].to<JsonObject>();
  guest["username"] = "guest";
  guest["password"] = "xxxsdgwer";
  guest["role"] = "guest";
  guest["email"] = "guest@promos.company";

  JsonObject user = users["user"].to<JsonObject>();
  user["username"] = "user";
  user["password"] = "xxxsdgwer";
  user["role"] = "user";
  user["email"] = "user@promos.company";

  JsonObject admin = users["admin"].to<JsonObject>();
  admin["username"] = "admin";
  admin["password"] = "xxxsdgwer";
  admin["role"] = "admin";
  admin["email"] = "admin@promos.company";

  root["keypad-lock"] = "enable";
}

void config_consumable_json(JsonObject root) {
#if defined(GSMART_FEATURE_USAGE) && defined(GSMART_EMITTER)
  if (esphome::storage::store == nullptr || esphome::storage::store->usage == nullptr) {
    return;
  }
  for (int i = 0; i < DEVICE_MAX_LAMP; i++) {
    std::string key = (i == 0) ? "lampA" : ((i == 1) ? "lampB" : (std::string("lamp") + std::to_string(i)));
    JsonObject lampJson = root[key].to<JsonObject>();
    auto &pref = esphome::storage::store->usage->lamp[i].pref;
    lampJson["durability-max"] = pref.lastExchangeLiveHour;
    lampJson["power"] = pref.powerWatts;
    lampJson["durability-current"] = pref.onSec / 3600;
    lampJson["switching-current"] = pref.startCount;
    lampJson["reset-last"] = pref.lastExchangeDate;
  }
#else
  root["enabled"] = false;
#endif
}

void config_connect_json(JsonObject root) {
  root["enable"] = true;

  JsonObject ap = root["ap"].to<JsonObject>();
  ap["enabled"] = false;
  ap["ssid"] = "";

  JsonObject client = root["client"].to<JsonObject>();
  client["enabled"] = true;
  client["ssid"] = "";

  JsonObject time = root["time"].to<JsonObject>();
  time["enabled"] = true;
  time["server"] = "pool.ntp.org";
  time["tz_label"] = "Europe/Bratislava";
  time["tz_format"] = "CET-1CEST,M3.5.0,M10.5.0/3";
}


const char *config_def_string() {
  static const char items[] =
      "{\"classes\":[{\"title\":\"Actuator\",\"code\":\"actuator\"},{\"title\":\"Emitter\",\"code\":\"emitter\"},{\"title\":\"Security\",\"code\":\"security\"},{\"title\":\"Setup\",\"code\":\"setup\"},{\"title\":\"Network\",\"code\":\"network\"},{\"title\":\"Region\",\"code\":\"region\"}],\"items\":[{\"title\":\"PINcode\",\"code\":\"pin_cd\",\"class\":\"actuator\",\"kind\":\"string\"},{\"title\":\"Locked\",\"code\":\"lock\",\"class\":\"actuator\",\"kind\":\"boolean\"},{\"title\":\"Sleep\",\"code\":\"sleep\",\"class\":\"actuator\",\"kind\":\"boolean\"},{\"title\":\"Brightnes\",\"code\":\"bright\",\"class\":\"actuator\",\"kind\":\"number\",\"props\":{\"min\":0,\"max\":100}},{\"title\":\"Dimmable\",\"code\":\"dim\",\"class\":\"actuator\",\"kind\":\"number\",\"props\":{\"min\":0,\"max\":100}},{\"title\":\"Locked\",\"code\":\"lock\",\"class\":\"emitter\",\"kind\":\"boolean\"},{\"title\":\"sound\",\"code\":\"sound\",\"class\":\"emitter\",\"props\":{\"options\":[\"Silence\",\"Low\",\"Std\",\"Max\"]},\"kind\":\"select\"},{\"title\":\"GuestPassword\",\"code\":\"guest_pass\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"Guestemail\",\"code\":\"guest_email\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"GuestLock\",\"code\":\"guest_lock\",\"class\":\"security\",\"kind\":\"boolean\"},{\"title\":\"GuestPinCode\",\"code\":\"guest_pinCode\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"UserPassword\",\"code\":\"user_pass\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"Useremail\",\"code\":\"user_email\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"UserLock\",\"code\":\"user_lock\",\"class\":\"security\",\"kind\":\"boolean\"},{\"title\":\"UserPinCode\",\"code\":\"user_pinCode\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"AdminPassword\",\"code\":\"admin_pass\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"Adminemail\",\"code\":\"admin_email\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"AdminLock\",\"code\":\"admin_lock\",\"class\":\"security\",\"kind\":\"boolean\"},{\"title\":\"AdminPinCode\",\"code\":\"admin_pinCode\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"Catalog\",\"code\":\"catalog\",\"class\":\"setup\",\"kind\":\"string\"},{\"title\":\"Batch\",\"code\":\"batch\",\"class\":\"setup\",\"kind\":\"string\"},{\"title\":\"BatchPosition\",\"code\":\"batch_pos\",\"class\":\"setup\",\"kind\":\"number\"},{\"title\":\"LampCount\",\"code\":\"lampCount\",\"class\":\"setup\",\"kind\":\"number\"},{\"title\":\"LampPower\",\"code\":\"lampPower\",\"class\":\"setup\",\"kind\":\"number\"},{\"title\":\"WifiSSID\",\"code\":\"wifi_ssid\",\"class\":\"network\",\"kind\":\"string\"},{\"title\":\"Wifipassword\",\"code\":\"wifi_pass\",\"class\":\"network\",\"kind\":\"string\"},{\"title\":\"APenable\",\"code\":\"ap_enable\",\"class\":\"network\",\"kind\":\"boolean\"},{\"title\":\"APpassword\",\"code\":\"ap_pass\",\"class\":\"network\",\"kind\":\"string\"},{\"title\":\"Timezone\",\"code\":\"time_zone\",\"class\":\"network\",\"kind\":\"string\"},{\"title\":\"Regionnumber\",\"code\":\"reg_num\",\"class\":\"region\",\"kind\":\"number\"},{\"title\":\"Masterserial\",\"code\":\"master_serial\",\"class\":\"region\",\"kind\":\"string\"},{\"title\":\"Members\",\"code\":\"members\",\"class\":\"region\",\"kind\":\"string\"}]}";
  return items;
}

}  // namespace payloads
}  // namespace api_core_v1
}  // namespace esphome
