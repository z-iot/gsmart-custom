#pragma once

#include "esphome/components/web_server_base/web_server_base.h"


#define ConfigDevice_PATH "/cfg/device"

class ConfigDevice {
 public:
  ConfigDevice(std::shared_ptr<AsyncWebServer> server);

 private:
  void get(AsyncWebServerRequest* request);
  void post(AsyncWebServerRequest* request);
};

