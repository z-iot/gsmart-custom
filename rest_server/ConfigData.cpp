#include "ConfigData.h"
#include "esphome/components/json/json_util.h"

ConfigData::ConfigData(std::shared_ptr<AsyncWebServer> server) {
  server->on(ConfigData_PATH, HTTP_GET, std::bind(&ConfigData::get, this, std::placeholders::_1));
  server->on(ConfigData_PATH, HTTP_POST, std::bind(&ConfigData::post, this, std::placeholders::_1));
}

void ConfigData::get(AsyncWebServerRequest* request) {

  std::string data =  esphome::json::build_json([](JsonObject root) {

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
      });

  request->send(200, "text/json", data.c_str());
}

void ConfigData::post(AsyncWebServerRequest* request) {

  std::string data =  esphome::json::build_json([](JsonObject root) {
      root["xxx"] = "XXXX";
  });

  request->send(200, "text/json", data.c_str());
}