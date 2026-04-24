#ifndef AuthenticationService_H_
#define AuthenticationService_H_

// #include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include "SecurityManager.h"
#include <ArduinoJWT.h>

#define VERIFY_AUTHORIZATION_PATH "/sec/me"
#define SIGN_IN_PATH "/sec/signin"

#define MAX_AUTHENTICATION_SIZE 256

#ifndef FACTORY_JWT_SECRET
#define FACTORY_JWT_SECRET "#9999-#8888"
#endif

class AuthenticationService
{
public:
  AuthenticationService(std::shared_ptr<AsyncWebServer> server, SecurityManager *securityManager);

  boolean validatePayload(JsonObject &parsedPayload, String username);
  String generateJWT(String username);

private:
  // ArduinoJsonJWT *_jwtHandler = new ArduinoJsonJWT(FACTORY_JWT_SECRET);
  
  ArduinoJWT *jwt = new ArduinoJWT(FACTORY_JWT_SECRET);
  SecurityManager *_securityManager;
  // AsyncCallbackJsonWebHandler _signInHandler;

  // endpoint functions
  void signIn(AsyncWebServerRequest *request, JsonVariant &json);
  void verifyAuthorization(AsyncWebServerRequest *request);
};

#endif // end SecurityManager_h
