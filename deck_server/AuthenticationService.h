#ifndef AuthenticationService_H_
#define AuthenticationService_H_

// #include <AsyncJson.h>
#include "esphome/components/web_server_base/web_server_base.h"

#include "SecurityManager.h"

#define VERIFY_AUTHORIZATION_PATH "/api/mobile/v1/me"
#define SIGN_IN_PATH "/api/mobile/v1/signin"
#define LEGACY_VERIFY_AUTHORIZATION_PATH "/sec/me"
#define LEGACY_SIGN_IN_PATH "/sec/signin"

#define MAX_AUTHENTICATION_SIZE 256

class AuthenticationService
{
public:
  AuthenticationService(std::shared_ptr<AsyncWebServer> server, SecurityManager *securityManager);

  boolean validatePayload(JsonObject &parsedPayload, String username);
  String generateJWT(String username);
  void signIn(AsyncWebServerRequest *request, JsonVariant &json);
  void verifyAuthorization(AsyncWebServerRequest *request);

private:
  SecurityManager *_securityManager;
  // AsyncCallbackJsonWebHandler _signInHandler;
};

#endif // end SecurityManager_h
