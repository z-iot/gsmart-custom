#include "SecurityService.h"
#include "esphome/components/storage/store.h"

SecurityService::SecurityService(std::shared_ptr<AsyncWebServer> server) : _jwtHandler(FACTORY_JWT_SECRET)
{
}

void SecurityService::begin()
{
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
  const AsyncWebHeader *authorizationHeader = request->getHeader(AUTHORIZATION_HEADER);
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
  if (request->hasParam(ACCESS_TOKEN_PARAMATER))
  {
    const AsyncWebParameter *tokenParamater = request->getParam(ACCESS_TOKEN_PARAMATER);
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
  }
  return Authentication();
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
