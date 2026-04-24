#include "ConfigConnect.h"
#include "esphome/components/json/json_util.h"

#define FACTORY_Connect_enabled true
#define FACTORY_Connect_server "time.google.com"
#define FACTORY_Connect_tz_label "Europe/Bratislava"
#define FACTORY_Connect_tz_format "GMT0BST,M3.5.0/1,M10.5.0"

enum class StateUpdateResult
{
  CHANGED = 0, // The update changed the state and propagation should take place if required
  UNCHANGED,   // The state was unchanged, propagation should not take place
  ERROR        // There was a problem updating the state, propagation should not take place
};

class SettingsConnect
{
public:
  bool enabled;
  String tzLabel;
  String tzFormat;
  String server;

  static void read(SettingsConnect &settings, JsonObject &root)
  {
    root["enabled"] = settings.enabled;
    root["server"] = settings.server;
    root["tz_label"] = settings.tzLabel;
    root["tz_format"] = settings.tzFormat;
  }

  static StateUpdateResult update(JsonObject &root, SettingsConnect &settings)
  {
    settings.enabled = root["enabled"] | FACTORY_Connect_enabled;
    settings.server = root["server"] | FACTORY_Connect_server;
    settings.tzLabel = root["tz_label"] | FACTORY_Connect_tz_label;
    settings.tzFormat = root["tz_format"] | FACTORY_Connect_tz_format;
    return StateUpdateResult::CHANGED;
  }
};

ConfigConnect::ConfigConnect(std::shared_ptr<AsyncWebServer> server)
{
  server->on(ConfigConnect_PATH, HTTP_GET, std::bind(&ConfigConnect::get, this, std::placeholders::_1));
  server->on(ConfigConnect_PATH, HTTP_POST, std::bind(&ConfigConnect::post, this, std::placeholders::_1));
}

void ConfigConnect::get(AsyncWebServerRequest *request)
{

  std::string data = esphome::json::build_json([](JsonObject root){
      root["enable"] = true;

      JsonObject ap = root.createNestedObject("ap");
      ap["enabled"] = true;
      ap["ssid"] = "promos-AP";
      ap["password"] = "123456";
      JsonObject client = root.createNestedObject("client");
      client["enabled"] = true;
      client["ssid"] = "promos";
      client["password"] = "654321";
      JsonObject time = root.createNestedObject("time");
      time["enabled"] = FACTORY_Connect_enabled;
      time["server"] = FACTORY_Connect_server;
      time["tz_label"] = FACTORY_Connect_tz_label;
      time["tz_format"] = FACTORY_Connect_tz_format; });

  request->send(200, "text/json", data.c_str());
}

void ConfigConnect::post(AsyncWebServerRequest *request)
{

  std::string data = esphome::json::build_json([](JsonObject root)
                                               { root["xxx"] = "XXXX"; });

  request->send(200, "text/json", data.c_str());
}