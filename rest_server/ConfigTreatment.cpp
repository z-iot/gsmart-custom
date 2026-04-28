#include "ConfigTreatment.h"
#include "esphome/components/json/json_util.h"

ConfigTreatment::ConfigTreatment(std::shared_ptr<AsyncWebServer> server) {
  server->on(ConfigTreatment_PATH, HTTP_GET, std::bind(&ConfigTreatment::get, this, std::placeholders::_1));
  server->on(ConfigTreatment_PATH, HTTP_POST, [this](AsyncWebServerRequest *request) {
    esphome::json::parse_json(request->getBody(), [this, request](JsonObject root) {
      this->post(request);
      return true;
    });
  });
}

void ConfigTreatment::get(AsyncWebServerRequest* request) {

  std::string data =  esphome::json::build_json([](JsonObject root) {

      JsonObject manual = root.createNestedObject("min");
      manual["duration"] = 10; //min
      manual["motion-delay"] = 30; //sec

      JsonObject std = root.createNestedObject("std");
      std["duration"] = 30; //min
      std["motion-delay"] = 30; //sec
      
      JsonObject max = root.createNestedObject("max");
      max["duration"] = 60; //min
      max["motion-delay"] = 60; //sec
      });

  request->send(200, "text/json", data.c_str());
}

void ConfigTreatment::post(AsyncWebServerRequest* request) {

  std::string data =  esphome::json::build_json([](JsonObject root) {
      root["xxx"] = "XXXX";
  });

  request->send(200, "text/json", data.c_str());
}
