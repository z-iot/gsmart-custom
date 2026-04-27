#pragma once

#include "esphome/components/web_server_base/web_server_base.h"


#define ConfigSecurity_PATH "/cfg/security"

class ConfigSecurity {
 public:
  ConfigSecurity(std::shared_ptr<AsyncWebServer> server);

 private:
  void get(AsyncWebServerRequest* request);
  void post(AsyncWebServerRequest* request);
};

