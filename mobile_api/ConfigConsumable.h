#pragma once

#include "esphome/components/web_server_base/web_server_base.h"


#define ConfigConsumable_PATH "/api/mobile/v1/consumable"

class ConfigConsumable {
 public:
  ConfigConsumable(std::shared_ptr<AsyncWebServer> server);

 private:
  void get(AsyncWebServerRequest* request);
  void post(AsyncWebServerRequest* request);
};

