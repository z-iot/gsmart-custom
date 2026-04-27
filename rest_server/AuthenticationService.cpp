#include "AuthenticationService.h"

AuthenticationService::AuthenticationService(std::shared_ptr<AsyncWebServer> server, SecurityManager *securityManager) : _securityManager(securityManager)
// _signInHandler(SIGN_IN_PATH, std::bind(&AuthenticationService::signIn, this, std::placeholders::_1, std::placeholders::_2))
{
  // _jwtHandler = ArduinoJsonJWT(FACTORY_JWT_SECRET);
  // jwt->allocateJWTMemory();
  server->on(VERIFY_AUTHORIZATION_PATH, HTTP_GET, std::bind(&AuthenticationService::verifyAuthorization, this, std::placeholders::_1));

  server->on(SIGN_IN_PATH, HTTP_POST, [this](AsyncWebServerRequest *request) {
    esphome::json::parse_json(request->post_query_, [this, request](JsonObject root) {
      JsonVariant json = root;
      this->signIn(request, json);
      return true;
    });
  });
}

/**
 * Verifys that the request supplied a valid JWT.
 */
void AuthenticationService::verifyAuthorization(AsyncWebServerRequest *request)
{
  Authentication authentication = _securityManager->authenticateRequest(request);
  request->send(authentication.authenticated ? 200 : 401);
  // request->send(200);
}

/**
 * Signs in a user if the username and password match. Provides a JWT to be used in the Authorization header in
 * subsequent requests.
 */
void AuthenticationService::signIn(AsyncWebServerRequest *request, JsonVariant &json)
{
  // ESP_LOGW("JWT", "signIn");
  if (json.is<JsonObject>())
  {
    String username = json["username"];
    String password = json["password"];
    // ESP_LOGW("JWT", "ADDR: %d", _securityManager);
    Authentication authentication = _securityManager->authenticate(username, password);
    // User user = User("service", "xxx", "service");
    // Authentication authentication = Authentication(username);
    if (authentication.authenticated)
    {
      std::string data = esphome::json::build_json([this, username](JsonObject jsonObject) {
        jsonObject["access_token"] = generateJWT(username);
      });
      request->send(200, "application/json", data.c_str());
      return;
    }
  }
  AsyncWebServerResponse *response = request->beginResponse(401);
  request->send(response);
}
inline void populateJWTPayload(JsonObject &payload, String username)
{
  payload["username"] = username;
  payload["created"] = 230410;
  // payload["role"] = user->role;
}

boolean AuthenticationService::validatePayload(JsonObject &parsedPayload, String username)
{
  DynamicJsonDocument jsonDocument(MAX_JWT_SIZE);
  JsonObject payload = jsonDocument.to<JsonObject>();
  populateJWTPayload(payload, username);
  return payload == parsedPayload;
}

String AuthenticationService::generateJWT(String username)
{
  DynamicJsonDocument jsonDocument(MAX_JWT_SIZE);
  JsonObject payload = jsonDocument.to<JsonObject>();
  populateJWTPayload(payload, username);
  // String jwt;
  // serializeJson(payload, jwt);
  // return jwt;

  // return _jwtHandler->buildJWT(payload);

  return jwt->buildJWT(payload);

  // String bodyEncoded = encode(body.c_str(), body.length());

  // String header = JWT_HEADER;
  // String msg = header + '.' + bodyEncoded;
  // // String msgSign = sign(msg);
  // String msgSign = "eeee";

  // String res = msg + '.' + msgSign;
  // return res;
}
