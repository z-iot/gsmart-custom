#include "ConfigMode.h"
#include "esphome/components/json/json_util.h"

namespace {
class ConfigModeHandler : public esphome::web_server_idf::AsyncWebHandler {
 public:
  ConfigModeHandler(ConfigMode *handler) : handler_(handler) {}

  bool canHandle(esphome::web_server_idf::AsyncWebServerRequest *request) const override {
    if (request->method() != HTTP_POST) return false;
    char url_buf[esphome::web_server_idf::AsyncWebServerRequest::URL_BUF_SIZE];
    return request->url_to(url_buf) == ConfigMode_PATH;
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
  ConfigMode *handler_;
  std::string body_;
};
}  // namespace

ConfigMode::ConfigMode(std::shared_ptr<AsyncWebServer> server) {
  server->on(ConfigMode_PATH, HTTP_GET, std::bind(&ConfigMode::get, this, std::placeholders::_1));
  server->addHandler(new ConfigModeHandler(this));
}

void ConfigMode::get(AsyncWebServerRequest* request) {

  std::string data =  esphome::json::build_json([](JsonObject root) {

      JsonObject manual = root["manual"].to<JsonObject>();
      manual["lamp"] = "top"; //top bottom alternate
      manual["fan"] = 50;
      manual["pir-mode"] = "none"; //none inactive
      manual["pir-delay"] = 30; //sec
      manual["pir-runtime"] = 600; //sec

      JsonObject eco = root["eco"].to<JsonObject>();
      eco["lamp"] = "bottom"; //top bottom alternate
      eco["fan"] = 30;
      eco["pir-mode"] = "active"; //none inactive
      eco["pir-delay"] = 60; //sec
      eco["pir-runtime"] = 120; //sec

      JsonObject normal = root["normal"].to<JsonObject>();
      normal["lamp"] = "alternate"; //top bottom alternate
      normal["fan"] = 70;
      normal["pir-mode"] = "active"; //none inactive
      normal["pir-delay"] = 10; //sec
      normal["pir-runtime"] = 600; //sec

      JsonObject max = root["max"].to<JsonObject>();
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
