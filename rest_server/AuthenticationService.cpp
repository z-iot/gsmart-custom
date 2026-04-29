#include "AuthenticationService.h"
#include "esphome/core/log.h"

static const char *const TAG = "auth_service";

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
    ESP_LOGI(TAG, ">>> canHandle: checking url=%s", url_buf);
    return (url == SIGN_IN_PATH);
  }

  void handleRequest(esphome::web_server_idf::AsyncWebServerRequest *request) override {
    ESP_LOGI(TAG, ">>> handleRequest START");
    // Framework already consumed the POST body into request->post_query_
    // (see web_server_idf.cpp request_post_handler). Don't re-read with
    // httpd_req_recv — the socket has no more data and casting the request
    // pointer to httpd_req_t* is undefined behavior (vptr is at offset 0).
    const std::string &body = request->post_query_;
    ESP_LOGI(TAG, ">>> handleRequest: body length=%zu", body.length());

    if (body.empty()) {
      ESP_LOGW(TAG, ">>> handleRequest: No body data");
      request->send(400);
      return;
    }

    ESP_LOGI(TAG, ">>> handleRequest: Body: %s", body.c_str());

    bool ok = esphome::json::parse_json(body, [this, request](JsonObject root) {
      ESP_LOGI(TAG, ">>> JSON parsed, calling signIn");
      JsonVariant json = root;
      auth_service_->signIn(request, json);
      return true;
    });
    if (!ok) {
      ESP_LOGW(TAG, ">>> handleRequest: JSON parse failed");
      request->send(400);
    }
  }
};

AuthenticationService::AuthenticationService(std::shared_ptr<AsyncWebServer> server, SecurityManager *securityManager) : _securityManager(securityManager)
{
  ESP_LOGI(TAG, ">>> AuthenticationService constructor START");

  server->on(VERIFY_AUTHORIZATION_PATH, HTTP_GET, std::bind(&AuthenticationService::verifyAuthorization, this, std::placeholders::_1));

  // Register sign-in handler
  server->addHandler(new SignInHandler(this));
  ESP_LOGI(TAG, ">>> AuthenticationService: SignIn handler registered");
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
  ESP_LOGI(TAG, ">>> signIn() START");

  if (json.is<JsonObject>())
  {
    ESP_LOGI(TAG, ">>> signIn: JSON is valid object");
    JsonObject obj = json.as<JsonObject>();
    String username = obj["username"];
    String password = obj["password"];

    ESP_LOGI(TAG, ">>> signIn: Extracted credentials - username='%s', password='%s'", username.c_str(), password.c_str());

    ESP_LOGI(TAG, ">>> signIn: Calling authenticate()...");
    Authentication authentication = _securityManager->authenticate(username, password);
    ESP_LOGI(TAG, ">>> signIn: authenticate() returned, authenticated=%d", authentication.authenticated);

    if (authentication.authenticated)
    {
      ESP_LOGI(TAG, ">>> signIn: AUTHENTICATED! Generating JWT for user: %s", username.c_str());
      std::string data = esphome::json::build_json([this, username](JsonObject jsonObject) {
        ESP_LOGI(TAG, ">>> signIn: Inside JWT build_json lambda");
        jsonObject["access_token"] = generateJWT(username);
        ESP_LOGI(TAG, ">>> signIn: JWT generated");
      });
      ESP_LOGI(TAG, ">>> signIn: Sending 200 response with JWT data");
      request->send(200, "application/json", data.c_str());
      ESP_LOGI(TAG, ">>> signIn: Response sent successfully!");
      return;
    }
    else
    {
      ESP_LOGW(TAG, ">>> signIn: AUTHENTICATION FAILED for user: %s", username.c_str());
    }
  }
  else
  {
    ESP_LOGW(TAG, ">>> signIn: ERROR - JSON is not an object!");
  }
  ESP_LOGI(TAG, ">>> signIn: Sending 401 response");
  AsyncWebServerResponse *response = request->beginResponse(401);
  request->send(response);
  ESP_LOGI(TAG, ">>> signIn: 401 response sent");
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
  JsonDocument jsonDocument;
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
