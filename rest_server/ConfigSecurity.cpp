#include "ConfigSecurity.h"
#include "esphome/components/json/json_util.h"

ConfigSecurity::ConfigSecurity(std::shared_ptr<AsyncWebServer> server) {
  server->on(ConfigSecurity_PATH, HTTP_GET, std::bind(&ConfigSecurity::get, this, std::placeholders::_1));
  server->on(ConfigSecurity_PATH, HTTP_POST, std::bind(&ConfigSecurity::post, this, std::placeholders::_1));
}

void ConfigSecurity::get(AsyncWebServerRequest* request) {

  std::string data =  esphome::json::build_json([](JsonObject root) {

      JsonObject users = root.createNestedObject("users");

      JsonObject guest = users.createNestedObject("guest");
      guest["username"] = "guest";
      guest["password"] = "xxxsdgwer";
      guest["role"] = "guest";
      guest["email"] = "guest@promos.company";
      JsonObject user = users.createNestedObject("user");
      user["username"] = "user";
      user["password"] = "xxxsdgwer";
      user["role"] = "user";
      user["email"] = "user@promos.company";
      JsonObject admin = users.createNestedObject("admin");
      admin["username"] = "admin";
      admin["password"] = "xxxsdgwer";
      admin["role"] = "admin";
      admin["email"] = "admin@promos.company";

      root["keypad-lock"] = "enable"; //disable
      });

  request->send(200, "text/json", data.c_str());
}

void ConfigSecurity::post(AsyncWebServerRequest* request) {

  std::string data =  esphome::json::build_json([](JsonObject root) {
      root["xxx"] = "XXXX";
  });

  request->send(200, "text/json", data.c_str());
}