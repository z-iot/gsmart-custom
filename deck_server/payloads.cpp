#include "payloads.h"

#include "esphome/components/storage/store.h"
#include "esphome/components/storage/settings_schedule.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace gsmart_server {
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

  root["fs_total"] = esphome::storage::fileSystem->GetTotalBytes();
  root["fs_used"] = esphome::storage::fileSystem->GetUsedBytes();

  // TODO vyhodit
  JsonObject dir = root["root_dir"].to<JsonObject>();
  esphome::storage::fileSystem->listDir(dir);

  JsonObject schedule = root["schedule"].to<JsonObject>();
  esphome::storage::store->schedule->toJson(schedule);

  JsonObject process = root["process"].to<JsonObject>();
  process["schedule_enabled"] = esphome::storage::store->schedule->enabled;
  process["isRegionActive"] = esphome::storage::store->region->isRegionActive();
  process["isMaster"] = esphome::storage::store->region->isMaster();
  process["selfIndex"] = esphome::storage::store->region->selfIndex;
  process["masterIndex"] = esphome::storage::store->region->layout.masterIndex;

  uint8_t selfMac[6];
  esphome::get_mac_address_raw(selfMac);
  process["selfMac"] = esphome::storage::convertMacToStr(selfMac);

  process["activeMode"] = esphome::storage::convertRadiationModeToStr(
      esphome::storage::store->global->radiation.activeMode);

  JsonObject usage = root["usage"].to<JsonObject>();
  esphome::storage::store->usage->fillAdvertise(usage);
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

  root["fs_total"] = esphome::storage::fileSystem->GetTotalBytes();
  root["fs_used"] = esphome::storage::fileSystem->GetUsedBytes();

  // TODO vyhodit
  JsonObject dir = root["root_dir"].to<JsonObject>();
  esphome::storage::fileSystem->listDir(dir);
}

void features_json(JsonObject root) {
  root["Model"] = esphome::storage::store->get_model();
  root["Serial"] = esphome::storage::store->get_serial();
}

void scheduller_json(JsonObject root) { esphome::storage::store->schedule->toJson(root); }

void scheduller_apply(JsonObject root) { esphome::storage::store->schedule->reloadFromJson(root); }

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
  JsonObject lampA = root["lampA"].to<JsonObject>();
  lampA["durability-max"] = 8000;
  lampA["power"] = 35;
  lampA["durability-current"] = 1234;
  lampA["switching-current"] = 555;
  lampA["reset-last"] = "22-01-05";

  JsonObject lampB = root["lampB"].to<JsonObject>();
  lampB["durability-max"] = 8000;
  lampB["power"] = 35;
  lampB["durability-current"] = 1234;
  lampB["switching-current"] = 555;
  lampB["reset-last"] = "22-02-15";
}

void config_connect_json(JsonObject root) {
  root["enable"] = true;

  JsonObject ap = root["ap"].to<JsonObject>();
  ap["enabled"] = true;
  ap["ssid"] = "promos-AP";
  ap["password"] = "123456";

  JsonObject client = root["client"].to<JsonObject>();
  client["enabled"] = true;
  client["ssid"] = "promos";
  client["password"] = "654321";

  JsonObject time = root["time"].to<JsonObject>();
  time["enabled"] = true;
  time["server"] = "time.google.com";
  time["tz_label"] = "Europe/Bratislava";
  time["tz_format"] = "GMT0BST,M3.5.0/1,M10.5.0";
}

const char *config_def_string() {
  static const char items[] =
      "{\"classes\":[{\"title\":\"Actuator\",\"code\":\"actuator\"},{\"title\":\"Emitter\",\"code\":\"emitter\"},{\"title\":\"Security\",\"code\":\"security\"},{\"title\":\"Setup\",\"code\":\"setup\"},{\"title\":\"Network\",\"code\":\"network\"},{\"title\":\"Region\",\"code\":\"region\"}],\"items\":[{\"title\":\"PINcode\",\"code\":\"pin_cd\",\"class\":\"actuator\",\"kind\":\"string\"},{\"title\":\"Locked\",\"code\":\"lock\",\"class\":\"actuator\",\"kind\":\"boolean\"},{\"title\":\"Sleep\",\"code\":\"sleep\",\"class\":\"actuator\",\"kind\":\"boolean\"},{\"title\":\"Brightnes\",\"code\":\"bright\",\"class\":\"actuator\",\"kind\":\"number\",\"props\":{\"min\":0,\"max\":100}},{\"title\":\"Dimmable\",\"code\":\"dim\",\"class\":\"actuator\",\"kind\":\"number\",\"props\":{\"min\":0,\"max\":100}},{\"title\":\"Locked\",\"code\":\"lock\",\"class\":\"emitter\",\"kind\":\"boolean\"},{\"title\":\"sound\",\"code\":\"sound\",\"class\":\"emitter\",\"props\":{\"options\":[\"Silence\",\"Low\",\"Std\",\"Max\"]},\"kind\":\"select\"},{\"title\":\"GuestPassword\",\"code\":\"guest_pass\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"Guestemail\",\"code\":\"guest_email\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"GuestLock\",\"code\":\"guest_lock\",\"class\":\"security\",\"kind\":\"boolean\"},{\"title\":\"GuestPinCode\",\"code\":\"guest_pinCode\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"UserPassword\",\"code\":\"user_pass\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"Useremail\",\"code\":\"user_email\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"UserLock\",\"code\":\"user_lock\",\"class\":\"security\",\"kind\":\"boolean\"},{\"title\":\"UserPinCode\",\"code\":\"user_pinCode\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"AdminPassword\",\"code\":\"admin_pass\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"Adminemail\",\"code\":\"admin_email\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"AdminLock\",\"code\":\"admin_lock\",\"class\":\"security\",\"kind\":\"boolean\"},{\"title\":\"AdminPinCode\",\"code\":\"admin_pinCode\",\"class\":\"security\",\"kind\":\"string\"},{\"title\":\"Catalog\",\"code\":\"catalog\",\"class\":\"setup\",\"kind\":\"string\"},{\"title\":\"Batch\",\"code\":\"batch\",\"class\":\"setup\",\"kind\":\"string\"},{\"title\":\"BatchPosition\",\"code\":\"batch_pos\",\"class\":\"setup\",\"kind\":\"number\"},{\"title\":\"LampCount\",\"code\":\"lampCount\",\"class\":\"setup\",\"kind\":\"number\"},{\"title\":\"LampPower\",\"code\":\"lampPower\",\"class\":\"setup\",\"kind\":\"number\"},{\"title\":\"WifiSSID\",\"code\":\"wifi_ssid\",\"class\":\"network\",\"kind\":\"string\"},{\"title\":\"Wifipassword\",\"code\":\"wifi_pass\",\"class\":\"network\",\"kind\":\"string\"},{\"title\":\"APenable\",\"code\":\"ap_enable\",\"class\":\"network\",\"kind\":\"boolean\"},{\"title\":\"APpassword\",\"code\":\"ap_pass\",\"class\":\"network\",\"kind\":\"string\"},{\"title\":\"Timezone\",\"code\":\"time_zone\",\"class\":\"network\",\"kind\":\"string\"},{\"title\":\"Regionnumber\",\"code\":\"reg_num\",\"class\":\"region\",\"kind\":\"number\"},{\"title\":\"Masterserial\",\"code\":\"master_serial\",\"class\":\"region\",\"kind\":\"string\"},{\"title\":\"Members\",\"code\":\"members\",\"class\":\"region\",\"kind\":\"string\"}]}";
  return items;
}

}  // namespace payloads
}  // namespace gsmart_server
}  // namespace esphome
