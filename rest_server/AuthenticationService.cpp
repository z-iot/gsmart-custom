#include "AuthenticationService.h"

AuthenticationService::AuthenticationService(std::shared_ptr<AsyncWebServer> server, SecurityManager *securityManager) : _securityManager(securityManager)
// _signInHandler(SIGN_IN_PATH, std::bind(&AuthenticationService::signIn, this, std::placeholders::_1, std::placeholders::_2))
{
  // _jwtHandler = ArduinoJsonJWT(FACTORY_JWT_SECRET);
  // jwt->allocateJWTMemory();
  server->on(VERIFY_AUTHORIZATION_PATH, HTTP_GET, std::bind(&AuthenticationService::verifyAuthorization, this, std::placeholders::_1));

  AsyncCallbackJsonWebHandler *signinHandler = new AsyncCallbackJsonWebHandler(SIGN_IN_PATH, std::bind(&AuthenticationService::signIn, this, std::placeholders::_1, std::placeholders::_2));
  signinHandler->setMethod(HTTP_POST);
  signinHandler->setMaxContentLength(MAX_AUTHENTICATION_SIZE);
  server->addHandler(signinHandler);
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
      // User *user = authentication.user;
      AsyncJsonResponse *response = new AsyncJsonResponse(false, MAX_AUTHENTICATION_SIZE);
      JsonObject jsonObject = response->getRoot();
      // uint32_t duration = millis();
      // jsonObject["access_token"] = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1c2VybmFtZSI6ImdvZ2Fub3ZpY0BnbWFpbC5jb20iLCJyb2xlIjoiY3VzdG9tZXIifQ.s69PT-wD3ur0rwFMsD-RgFp_D0EF6mrfiw59fbhhpz0";
      jsonObject["access_token"] = generateJWT(username);
      // duration = millis() - duration;
      // jsonObject["duration"] = duration;
      // jsonObject["access_token"] = "xxxxxxxx";
      // jsonObject["role"] = user->role;
      response->setLength();
      request->send(response);
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

  String body;
  serializeJson(payload, body);
  // jwt->encodeJWT(&body[0]);
  // char *bodyBuf = (char*)body.c_str();

  return jwt->encodeJWT(body);

  // String bodyEncoded = encode(body.c_str(), body.length());

  // String header = JWT_HEADER;
  // String msg = header + '.' + bodyEncoded;
  // // String msgSign = sign(msg);
  // String msgSign = "eeee";

  // String res = msg + '.' + msgSign;
  // return res;
}
