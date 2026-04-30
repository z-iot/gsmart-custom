#include "ConfigSecurity.h"
#include "esphome/components/gsmart_server/web_helpers.h"
#include "esphome/components/json/json_util.h"

ConfigSecurity::ConfigSecurity(std::shared_ptr<AsyncWebServer> server) {
  esphome::gsmart_server::on(server, ConfigSecurity_PATH, HTTP_GET, std::bind(&ConfigSecurity::get, this, std::placeholders::_1));
  esphome::gsmart_server::on(server, ConfigSecurity_PATH, HTTP_POST, std::bind(&ConfigSecurity::post, this, std::placeholders::_1));
}

void ConfigSecurity::get(AsyncWebServerRequest* request) {

  std::string data =  esphome::json::build_json([](JsonObject root) {

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