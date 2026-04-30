#pragma once

#include "esphome/components/web_server_base/web_server_base.h"


#define ConfigData_PATH "/api/mobile/v1/config"

class ConfigData {
 public:
  ConfigData(std::shared_ptr<AsyncWebServer> server);

 private:
  void get(AsyncWebServerRequest* request);
  void post(AsyncWebServerRequest* request);
};

