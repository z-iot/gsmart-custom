#include "ConfigScheduller.h"
#include "esphome/components/storage/store.h"
// #include "esphome/components/storage/settings_schedule.h"


ConfigScheduller::ConfigScheduller(std::shared_ptr<AsyncWebServer> server)
{
  server->on(ConfigScheduller_PATH, HTTP_GET, std::bind(&ConfigScheduller::get, this, std::placeholders::_1));
  
  server->on(ConfigScheduller_PATH, HTTP_POST, [this](AsyncWebServerRequest *request) {
    esphome::json::parse_json(request->post_query_, [this, request](JsonObject root) {
      JsonVariant json = root;
      this->post(request, json);
      return true;
    });
  });
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

  AsyncWebServerResponse *response = request->beginResponse(401);
  request->send(response);
}
