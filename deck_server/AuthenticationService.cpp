#include "AuthenticationService.h"
#include "esphome/components/web_server_base/web_helpers.h"

// AsyncWebHandler for SIGN_IN_PATH POST with JSON body.
// Body is consumed via handleBody (same pattern as Config* handlers) so we
// don't depend on framework-internal storage of the POST body.
class SignInHandler : public AsyncWebHandler {
 private:
  AuthenticationService *auth_service_;
  std::string body_;

 public:
  SignInHandler(AuthenticationService *auth) : auth_service_(auth) {}

  bool canHandle(AsyncWebServerRequest *request) const override {
    if (request->method() != HTTP_POST) return false;
#if USE_ESP32
    char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
    std::string url = std::string(request->url_to(url_buf));
#else
    std::string url = request->url().c_str();
#endif
    return (url == SIGN_IN_PATH || url == LEGACY_SIGN_IN_PATH);
  }

  void handleRequest(AsyncWebServerRequest *request) override {
    // Body handled by handleBody.
  }

  void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len,
                  size_t index, size_t total) override {
    if (index == 0) body_.clear();
    body_.append((char *) data, len);
    if (index + len != total) return;

    if (body_.empty()) {
      request->send(400);
      return;
    }
    bool ok = esphome::json::parse_json(body_, [this, request](JsonObject root) {
      JsonVariant json = root;
      auth_service_->signIn(request, json);
      return true;
    });
    if (!ok) request->send(400);
  }
};

AuthenticationService::AuthenticationService(std::shared_ptr<AsyncWebServer> server, SecurityManager *securityManager) : _securityManager(securityManager)
{
  esphome::web_server_base::on(server, VERIFY_AUTHORIZATION_PATH, HTTP_GET,
                             std::bind(&AuthenticationService::verifyAuthorization, this, std::placeholders::_1));
  esphome::web_server_base::on(server, LEGACY_VERIFY_AUTHORIZATION_PATH, HTTP_GET,
                             std::bind(&AuthenticationService::verifyAuthorization, this, std::placeholders::_1));

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
  request->send(401);
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
