#include "ConfigDevice.h"
#include "esphome/components/json/json_util.h"

ConfigDevice::ConfigDevice(std::shared_ptr<AsyncWebServer> server) {
  server->on(ConfigDevice_PATH, HTTP_GET, std::bind(&ConfigDevice::get, this, std::placeholders::_1));
  server->on(ConfigDevice_PATH, HTTP_POST, [this](AsyncWebServerRequest *request) {
    esphome::json::parse_json(request->post_query_, [this, request](JsonObject root) {
      this->post(request);
      return true;
    });
  });
}

void ConfigDevice::get(AsyncWebServerRequest* request) {

  std::string data =  esphome::json::build_json([](JsonObject root) {

      root["email"] = "promos@promos.company";
      root["keypad-lock"] = "enable"; //disable
      });

  request->send(200, "text/json", data.c_str());
}

void ConfigDevice::post(AsyncWebServerRequest* request) {

  std::string data =  esphome::json::build_json([](JsonObject root) {
      root["xxx"] = "XXXX";
  });

  request->send(200, "text/json", data.c_str());
}
