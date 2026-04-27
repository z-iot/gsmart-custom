#pragma once

#include "esphome/components/web_server_base/web_server_base.h"


#define ConfigMode_PATH "/cfg/mode"

class ConfigMode {
 public:
  ConfigMode(std::shared_ptr<AsyncWebServer> server);

 private:
  void get(AsyncWebServerRequest* request);
  void post(AsyncWebServerRequest* request);
};

