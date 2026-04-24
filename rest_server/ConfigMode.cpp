#include "ConfigMode.h"
#include "esphome/components/json/json_util.h"

ConfigMode::ConfigMode(std::shared_ptr<AsyncWebServer> server) {
  server->on(ConfigMode_PATH, HTTP_GET, std::bind(&ConfigMode::get, this, std::placeholders::_1));
  server->on(ConfigMode_PATH, HTTP_POST, std::bind(&ConfigMode::post, this, std::placeholders::_1));
}

void ConfigMode::get(AsyncWebServerRequest* request) {

  std::string data =  esphome::json::build_json([](JsonObject root) {

      JsonObject manual = root.createNestedObject("manual");
      manual["lamp"] = "top"; //top bottom alternate
      manual["fan"] = 50;
      manual["pir-mode"] = "none"; //none inactive
      manual["pir-delay"] = 30; //sec
      manual["pir-runtime"] = 600; //sec

      JsonObject eco = root.createNestedObject("eco");
      eco["lamp"] = "bottom"; //top bottom alternate
      eco["fan"] = 30;
      eco["pir-mode"] = "active"; //none inactive
      eco["pir-delay"] = 60; //sec
      eco["pir-runtime"] = 120; //sec
      
      JsonObject normal = root.createNestedObject("normal");
      normal["lamp"] = "alternate"; //top bottom alternate
      normal["fan"] = 70;
      normal["pir-mode"] = "active"; //none inactive
      normal["pir-delay"] = 10; //sec
      normal["pir-runtime"] = 600; //sec
      
      JsonObject max = root.createNestedObject("max");
      max["lamp"] = "both"; //top bottom alternate
      max["fan"] = 100;
      max["pir-mode"] = "inactive"; //none inactive
      max["pir-delay"] = 5; //sec
      max["pir-runtime"] = 1800; //sec      

      });

  request->send(200, "text/json", data.c_str());
}

void ConfigMode::post(AsyncWebServerRequest* request) {

  std::string data =  esphome::json::build_json([](JsonObject root) {
      root["xxx"] = "XXXX";
  });

  request->send(200, "text/json", data.c_str());
}