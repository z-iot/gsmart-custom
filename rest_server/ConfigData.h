#pragma once

#include "esphome/components/web_server_base/web_server_base.h"


#define ConfigData_PATH "/cfg/config"

class ConfigData {
 public:
  ConfigData(std::shared_ptr<AsyncWebServer> server);

 private:
  void get(AsyncWebServerRequest* request);
  void post(AsyncWebServerRequest* request);
};

