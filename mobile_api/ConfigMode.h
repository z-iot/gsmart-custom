#pragma once

#include "esphome/components/web_server_base/web_server_base.h"


#define ConfigMode_PATH "/api/mobile/v1/mode"

class ConfigMode {
 public:
  ConfigMode(std::shared_ptr<AsyncWebServer> server);
  void get(AsyncWebServerRequest* request);
  void post(AsyncWebServerRequest* request);
};

