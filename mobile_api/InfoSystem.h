#pragma once

#include "esphome/components/web_server_base/web_server_base.h"


#define InfoSystem_PATH "/api/mobile/v1/system"

class InfoSystem {
 public:
  InfoSystem(std::shared_ptr<AsyncWebServer> server);

 private:
  void get(AsyncWebServerRequest* request);
};

