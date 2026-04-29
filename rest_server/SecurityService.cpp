#include "SecurityService.h"
#include "esphome/components/storage/store.h"

SecurityService::SecurityService(std::shared_ptr<AsyncWebServer> server) : _jwtHandler(FACTORY_JWT_SECRET)
{
  // server->on(ConfigScheduller_PATH, HTTP_GET, std::bind(&ConfigScheduller::get, this, std::placeholders::_1));
  // // server->on(ConfigScheduller_PATH, HTTP_POST, std::bind(&ConfigScheduller::post, this, std::placeholders::_1));
  // AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler(ConfigScheduller_PATH, std::bind(&ConfigScheduller::post, this, std::placeholders::_1, std::placeholders::_2));
  // handler->setMethod(HTTP_POST);
  // // handler.setMaxContentLength(MAX_TIME_SIZE);
  // server->addHandler(handler);

  // addUpdateHandler([&](const String& originId) { configureJWTHandler(); }, false);
}

// void SecurityService::get(AsyncWebServerRequest *request)
// {

//   std::string data = esphome::json::build_json([](JsonObject root)
//                                                { esphome::storage::store->schedule->toJson(root); });

//   request->send(200, "application/json", data.c_str());
// }

// void SecurityService::post(AsyncWebServerRequest *request, JsonVariant &json)
// {
//   if (json.is<JsonObject>())
//   {
//     JsonObject root = json.as<JsonObject>();
//     esphome::storage::store->schedule->reloadFromJson(root);

//     // String username = json["username"];
//     // String password = json["password"];
//     // if (authentication.authenticated) {
//     // User* user = authentication.user;
//     // AsyncJsonResponse* response = new AsyncJsonResponse(false, MAX_AUTHENTICATION_SIZE);
//     // JsonObject jsonObject = response->getRoot();
//     // jsonObject["access_token"] = _securityManager->generateJWT(user);as
//     // response->setLength();
//     // request->send(response);

//     std::string data = esphome::json::build_json([](JsonObject root)
//                                                  { root["xxx"] = "XXXX"; });
//     request->send(200, "application/json", data.c_str());
//     return;
//   }

//   AsyncWebServerResponse *response = request->beginResponse(401);
//   request->send(response);
// }

void SecurityService::begin()
{
  // _fsPersistence.readFromFS();
  configureJWTHandler();
}

Authentication SecurityService::authenticateRequest(AsyncWebServerRequest *request)
{
#if USE_ESP32
  auto authorizationHeader = request->get_header(AUTHORIZATION_HEADER);
  if (authorizationHeader.has_value())
  {
    String value(authorizationHeader->c_str());
    if (value.startsWith(AUTHORIZATION_HEADER_PREFIX))
    {
      value = value.substring(AUTHORIZATION_HEADER_PREFIX_LEN);
      return authenticateJWT(value);
    }
  }
#else
  AsyncWebHeader *authorizationHeader = request->getHeader(AUTHORIZATION_HEADER);
  if (authorizationHeader)
  {
    String value = authorizationHeader->value();
    if (value.startsWith(AUTHORIZATION_HEADER_PREFIX))
    {
      value = value.substring(AUTHORIZATION_HEADER_PREFIX_LEN);
      return authenticateJWT(value);
    }
  }
#endif
  else if (request->hasParam(ACCESS_TOKEN_PARAMATER))
  {
    AsyncWebParameter *tokenParamater = request->getParam(ACCESS_TOKEN_PARAMATER);
    String value(tokenParamater->value().c_str());
    return authenticateJWT(value);
  }
  return Authentication();
}

void SecurityService::configureJWTHandler()
{
  if (_state.jwtSecret.isEmpty())
    _state.jwtSecret = FACTORY_JWT_SECRET;
  _jwtHandler.setSecret(_state.jwtSecret);
}

Authentication SecurityService::authenticateJWT(String &jwt)
{
  JsonDocument payloadDocument;
  _jwtHandler.parseJWT(jwt, payloadDocument);
  if (payloadDocument.is<JsonObject>())
  {
    JsonObject parsedPayload = payloadDocument.as<JsonObject>();
    String username = parsedPayload["username"];
    if (username == "admin" || username == "service")
      return Authentication(username);
    // for (User _user : _state.users)
    // {
    //   if (_user.username == username && validatePayload(parsedPayload, &_user))
    //   {
    //     return Authentication(_user);
    //   }
    // }
  }
  return Authentication();
  // User user = User("admin", "xxx", "customer");
  // return Authentication(user);
}

Authentication SecurityService::authenticate(const String &username, const String &password)
{
  if (username == "admin" && password == "12345678") {
    return Authentication(username);
  }

  if (username == "service" && password == "1234") {
    return Authentication(username);
  }

  return Authentication();
}

inline void populateJWTPayload(JsonObject &payload, String username)
{
  payload["username"] = username;
  // payload["role"] = user->role;
}

boolean SecurityService::validatePayload(JsonObject &parsedPayload, String username)
{
  JsonDocument jsonDocument;
  JsonObject payload = jsonDocument.to<JsonObject>();
  populateJWTPayload(payload, username);
  return payload == parsedPayload;
}

String SecurityService::generateJWT(String username)
{
  JsonDocument jsonDocument;
  JsonObject payload = jsonDocument.to<JsonObject>();
  populateJWTPayload(payload, username);
  return _jwtHandler.buildJWT(payload);
}

ArRequestFilterFunction SecurityService::filterRequest(AuthenticationPredicate predicate)
{
  return [this, predicate](AsyncWebServerRequest *request)
  {
    Authentication authentication = authenticateRequest(request);
    return predicate(authentication);
  };
}

ArRequestHandlerFunction SecurityService::wrapRequest(ArRequestHandlerFunction onRequest,
                                                      AuthenticationPredicate predicate)
{
  return [this, onRequest, predicate](AsyncWebServerRequest *request)
  {
    Authentication authentication = authenticateRequest(request);
    if (!predicate(authentication))
    {
      request->send(401);
      return;
    }
    onRequest(request);
  };
}

ArJsonRequestHandlerFunction SecurityService::wrapCallback(ArJsonRequestHandlerFunction onRequest,
                                                           AuthenticationPredicate predicate)
{
  return [this, onRequest, predicate](AsyncWebServerRequest *request, JsonVariant &json)
  {
    Authentication authentication = authenticateRequest(request);
    if (!predicate(authentication))
    {
      request->send(401);
      return;
    }
    onRequest(request, json);
  };
}
