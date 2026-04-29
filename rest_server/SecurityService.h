#pragma once

#include "esphome/components/web_server_base/web_server_base.h"

#include "esphome/components/json/json_util.h"
#include "SecurityManager.h"

#ifndef FACTORY_JWT_SECRET
#define FACTORY_JWT_SECRET "#{random}-#{random}"
#endif

#ifndef FACTORY_ADMIN_USERNAME
#define FACTORY_ADMIN_USERNAME "admin"
#endif

#ifndef FACTORY_ADMIN_PASSWORD
#define FACTORY_ADMIN_PASSWORD "admin"
#endif

#ifndef FACTORY_GUEST_USERNAME
#define FACTORY_GUEST_USERNAME "guest"
#endif

#ifndef FACTORY_GUEST_PASSWORD
#define FACTORY_GUEST_PASSWORD "guest"
#endif

class SecuritySettings {
 public:
  String jwtSecret{FACTORY_JWT_SECRET};
  // std::vector<User> users;

  // static void read(SecuritySettings& settings, JsonObject& root) {
  //   // secret
  //   root["jwt_secret"] = settings.jwtSecret;

  //   // users
  //   JsonArray users = root["users"].to<JsonArray>();
  //   for (User user : settings.users) {
  //     JsonObject userRoot = users.add<JsonObject>();
  //     userRoot["username"] = user.username;
  //     userRoot["password"] = user.password;
  //     userRoot["admin"] = user.admin;
  //   }
  // }

  // static StateUpdateResult update(JsonObject& root, SecuritySettings& settings) {
  //   // secret
  //   settings.jwtSecret = root["jwt_secret"] | SettingValue::format(FACTORY_JWT_SECRET);

  //   // users
  //   settings.users.clear();
  //   if (root["users"].is<JsonArray>()) {
  //     for (JsonVariant user : root["users"].as<JsonArray>()) {
  //       settings.users.push_back(User(user["username"], user["password"], user["admin"]));
  //     }
  //   } else {
  //     settings.users.push_back(User(FACTORY_ADMIN_USERNAME, FACTORY_ADMIN_PASSWORD, true));
  //     settings.users.push_back(User(FACTORY_GUEST_USERNAME, FACTORY_GUEST_PASSWORD, false));
  //   }
  //   return StateUpdateResult::CHANGED;
  // }
};

class SecurityService : public SecurityManager
{
public:
  SecurityService(std::shared_ptr<AsyncWebServer> server);

  void begin();

  // Functions to implement SecurityManager
  Authentication authenticate(const String &username, const String &password);
  Authentication authenticateRequest(AsyncWebServerRequest *request);
  String generateJWT(String username);
  ArRequestFilterFunction filterRequest(AuthenticationPredicate predicate);
  ArRequestHandlerFunction wrapRequest(ArRequestHandlerFunction onRequest, AuthenticationPredicate predicate);
  ArJsonRequestHandlerFunction wrapCallback(ArJsonRequestHandlerFunction callback, AuthenticationPredicate predicate);

private:
  ArduinoJsonJWT _jwtHandler;
  SecuritySettings _state;

  Authentication authenticateJWT(String &jwt);
  boolean validatePayload(JsonObject &parsedPayload, String username);
  
  void configureJWTHandler();
};
