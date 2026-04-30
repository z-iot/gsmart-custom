#include "ConfigConnect.h"
#include "esphome/components/gsmart_server/web_helpers.h"
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

namespace {
class ConfigConnectHandler : public esphome::web_server_idf::AsyncWebHandler {
 public:
  ConfigConnectHandler(ConfigConnect *handler) : handler_(handler) {}

  bool canHandle(esphome::web_server_idf::AsyncWebServerRequest *request) const override {
    if (request->method() != HTTP_POST) return false;
    char url_buf[esphome::web_server_idf::AsyncWebServerRequest::URL_BUF_SIZE];
    return request->url_to(url_buf) == ConfigConnect_PATH;
  }


  void handleRequest(esphome::web_server_idf::AsyncWebServerRequest *request) override {
    // Body will be handled by handleBody
  }

  void handleBody(esphome::web_server_idf::AsyncWebServerRequest *request, uint8_t *data, size_t len,
                  size_t index, size_t total) override {
    if (index == 0) body_.clear();
    body_.append((char *)data, len);
    if (index + len == total) {
      esphome::json::parse_json(body_, [this, request](JsonObject root) {
        this->handler_->post(request);
        return true;
      });
    }
  }

 private:
  ConfigConnect *handler_;
  std::string body_;
};
}  // namespace

ConfigConnect::ConfigConnect(std::shared_ptr<AsyncWebServer> server)
{
  esphome::gsmart_server::on(server, ConfigConnect_PATH, HTTP_GET, std::bind(&ConfigConnect::get, this, std::placeholders::_1));
  server->addHandler(new ConfigConnectHandler(this));
}

void ConfigConnect::get(AsyncWebServerRequest *request)
{

  std::string data = esphome::json::build_json([](JsonObject root){
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
