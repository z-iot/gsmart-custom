#include "ConfigTreatment.h"
#include "esphome/components/gsmart_server/web_helpers.h"
#include "esphome/components/json/json_util.h"

namespace {
class ConfigTreatmentHandler : public esphome::web_server_idf::AsyncWebHandler {
 public:
  ConfigTreatmentHandler(ConfigTreatment *handler) : handler_(handler) {}

  bool canHandle(esphome::web_server_idf::AsyncWebServerRequest *request) const override {
    if (request->method() != HTTP_POST) return false;
    char url_buf[esphome::web_server_idf::AsyncWebServerRequest::URL_BUF_SIZE];
    return request->url_to(url_buf) == ConfigTreatment_PATH;
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
  ConfigTreatment *handler_;
  std::string body_;
};
}  // namespace

ConfigTreatment::ConfigTreatment(std::shared_ptr<AsyncWebServer> server) {
  esphome::gsmart_server::on(server, ConfigTreatment_PATH, HTTP_GET, std::bind(&ConfigTreatment::get, this, std::placeholders::_1));
  server->addHandler(new ConfigTreatmentHandler(this));
}

void ConfigTreatment::get(AsyncWebServerRequest* request) {

  std::string data =  esphome::json::build_json([](JsonObject root) {

      JsonObject manual = root["min"].to<JsonObject>();
      manual["duration"] = 10; //min
      manual["motion-delay"] = 30; //sec

      JsonObject std = root["std"].to<JsonObject>();
      std["duration"] = 30; //min
      std["motion-delay"] = 30; //sec

      JsonObject max = root["max"].to<JsonObject>();
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
