#include "ConfigDevice.h"
#include "esphome/components/gsmart_server/web_helpers.h"
#include "esphome/components/json/json_util.h"

namespace {
class ConfigDeviceHandler : public esphome::web_server_idf::AsyncWebHandler {
 public:
  ConfigDeviceHandler(ConfigDevice *handler) : handler_(handler) {}

  bool canHandle(esphome::web_server_idf::AsyncWebServerRequest *request) const override {
    if (request->method() != HTTP_POST) return false;
    char url_buf[esphome::web_server_idf::AsyncWebServerRequest::URL_BUF_SIZE];
    return request->url_to(url_buf) == ConfigDevice_PATH;
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
  ConfigDevice *handler_;
  std::string body_;
};
}  // namespace

ConfigDevice::ConfigDevice(std::shared_ptr<AsyncWebServer> server) {
  esphome::gsmart_server::on(server, ConfigDevice_PATH, HTTP_GET, std::bind(&ConfigDevice::get, this, std::placeholders::_1));
  server->addHandler(new ConfigDeviceHandler(this));
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
