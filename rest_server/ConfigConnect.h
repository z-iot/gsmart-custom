#pragma once

#include "esphome/components/web_server_base/web_server_base.h"

#define ConfigConnect_PATH "/cfg/connect"

class ConfigConnect {
 public:
  ConfigConnect(std::shared_ptr<AsyncWebServer> server);
  void get(AsyncWebServerRequest* request);
  void post(AsyncWebServerRequest* request);
};


