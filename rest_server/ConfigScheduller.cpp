#include "ConfigScheduller.h"
#include "esphome/components/gsmart_server/web_helpers.h"
#include "esphome/components/storage/store.h"
// #include "esphome/components/storage/settings_schedule.h"

namespace {
class ConfigSchedullerHandler : public esphome::web_server_idf::AsyncWebHandler {
 public:
  ConfigSchedullerHandler(ConfigScheduller *handler) : handler_(handler) {}

  bool canHandle(esphome::web_server_idf::AsyncWebServerRequest *request) const override {
    if (request->method() != HTTP_POST) return false;
    char url_buf[esphome::web_server_idf::AsyncWebServerRequest::URL_BUF_SIZE];
    return request->url_to(url_buf) == ConfigScheduller_PATH;
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
        JsonVariant json = root;
        this->handler_->post(request, json);
        return true;
      });
    }
  }

 private:
  ConfigScheduller *handler_;
  std::string body_;
};
}  // namespace

ConfigScheduller::ConfigScheduller(std::shared_ptr<AsyncWebServer> server)
{
  esphome::gsmart_server::on(server, ConfigScheduller_PATH, HTTP_GET, std::bind(&ConfigScheduller::get, this, std::placeholders::_1));
  server->addHandler(new ConfigSchedullerHandler(this));
}

void ConfigScheduller::get(AsyncWebServerRequest *request)
{

  std::string data = esphome::json::build_json([](JsonObject root)
                                               { esphome::storage::store->schedule->toJson(root); });

  request->send(200, "application/json", data.c_str());
}

void ConfigScheduller::post(AsyncWebServerRequest *request, JsonVariant &json)
{
  if (json.is<JsonObject>())
  {
    JsonObject root = json.as<JsonObject>();
    esphome::storage::store->schedule->reloadFromJson(root);

    // String username = json["username"];
    // String password = json["password"];
    // if (authentication.authenticated) {
    // User* user = authentication.user;
    // AsyncJsonResponse* response = new AsyncJsonResponse(false, MAX_AUTHENTICATION_SIZE);
    // JsonObject jsonObject = response->getRoot();
    // jsonObject["access_token"] = _securityManager->generateJWT(user);as
    // response->setLength();
    // request->send(response);

    std::string data = esphome::json::build_json([](JsonObject root)
                                                 { root["xxx"] = "XXXX"; });
    request->send(200, "application/json", data.c_str());
    return;
  }

  request->send(401);
}
