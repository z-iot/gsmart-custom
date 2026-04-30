#include "ConfigConsumable.h"
#include "esphome/components/gsmart_server/web_helpers.h"
#include "esphome/components/json/json_util.h"

ConfigConsumable::ConfigConsumable(std::shared_ptr<AsyncWebServer> server) {
  esphome::gsmart_server::on(server, ConfigConsumable_PATH, HTTP_GET, std::bind(&ConfigConsumable::get, this, std::placeholders::_1));
  esphome::gsmart_server::on(server, ConfigConsumable_PATH, HTTP_POST, std::bind(&ConfigConsumable::post, this, std::placeholders::_1));
}

void ConfigConsumable::get(AsyncWebServerRequest* request) {

  std::string data =  esphome::json::build_json([](JsonObject root) {

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

      });

  request->send(200, "text/json", data.c_str());
}

void ConfigConsumable::post(AsyncWebServerRequest* request) {

  std::string data =  esphome::json::build_json([](JsonObject root) {
      root["xxx"] = "XXXX";
  });

  request->send(200, "text/json", data.c_str());
}