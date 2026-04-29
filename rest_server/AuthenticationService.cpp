#include "AuthenticationService.h"

// Simple AsyncWebHandler for /sec/signin POST with JSON body
class SignInHandler : public esphome::web_server_idf::AsyncWebHandler {
 private:
  AuthenticationService *auth_service_;

 public:
  SignInHandler(AuthenticationService *auth) : auth_service_(auth) {}

  bool canHandle(esphome::web_server_idf::AsyncWebServerRequest *request) const override {
    if (request->method() != HTTP_POST) return false;
    char url_buf[esphome::web_server_idf::AsyncWebServerRequest::URL_BUF_SIZE];
    auto url = request->url_to(url_buf);
    return (url == SIGN_IN_PATH);
  }

  void handleRequest(esphome::web_server_idf::AsyncWebServerRequest *request) override {
    // Framework already consumed the POST body into request->post_query_
    // (see web_server_idf.cpp request_post_handler). Don't re-read with
    // httpd_req_recv — the socket has no more data and casting the request
    // pointer to httpd_req_t* is undefined behavior (vptr is at offset 0).
    const std::string &body = request->post_query_;

    if (body.empty()) {
      request->send(400);
      return;
    }

    bool ok = esphome::json::parse_json(body, [this, request](JsonObject root) {
      JsonVariant json = root;
      auth_service_->signIn(request, json);
      return true;
    });
    if (!ok) {
      request->send(400);
    }
  }
};

AuthenticationService::AuthenticationService(std::shared_ptr<AsyncWebServer> server, SecurityManager *securityManager) : _securityManager(securityManager)
{
  server->on(VERIFY_AUTHORIZATION_PATH, HTTP_GET, std::bind(&AuthenticationService::verifyAuthorization, this, std::placeholders::_1));

  // Register sign-in handler
  server->addHandler(new SignInHandler(this));
}

/**
 * Verifys that the request supplied a valid JWT.
 */
void AuthenticationService::verifyAuthorization(AsyncWebServerRequest *request)
{
  Authentication authentication = _securityManager->authenticateRequest(request);
  if (!authentication.authenticated)
  {
    request->send(401);
    return;
  }

  std::string data = esphome::json::build_json([this, &authentication](JsonObject jsonObject) {
    jsonObject["access_token"] = _securityManager->generateJWT(authentication.username);
  });
  request->send(200, "application/json", data.c_str());
}

/**
 * Signs in a user if the username and password match. Provides a JWT to be used in the Authorization header in
 * subsequent requests.
 */
void AuthenticationService::signIn(AsyncWebServerRequest *request, JsonVariant &json)
{
  if (json.is<JsonObject>())
  {
    JsonObject obj = json.as<JsonObject>();
    String username = obj["username"];
    String password = obj["password"];

    Authentication authentication = _securityManager->authenticate(username, password);

    if (authentication.authenticated)
    {
      std::string data = esphome::json::build_json([this, &authentication](JsonObject jsonObject) {
        jsonObject["access_token"] = _securityManager->generateJWT(authentication.username);
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
  JsonDocument jsonDocument;
  JsonObject payload = jsonDocument.to<JsonObject>();
  populateJWTPayload(payload, username);
  return payload == parsedPayload;
}

String AuthenticationService::generateJWT(String username)
{
  return _securityManager->generateJWT(username);
}
