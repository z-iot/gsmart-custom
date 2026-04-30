#pragma once

#include "esphome/components/web_server_base/web_server_base.h"


#define InfoFeature_PATH "/api/mobile/v1/features"

class InfoFeature {
 public:
  InfoFeature(std::shared_ptr<AsyncWebServer> server);

 private:
  void get(AsyncWebServerRequest* request);
};
