#pragma once

#include "esphome/components/web_server_base/web_server_base.h"


#define InfoFeature_PATH "/rest/features"

class InfoFeature {
 public:
  InfoFeature(std::shared_ptr<AsyncWebServer> server);

 private:
  void get(AsyncWebServerRequest* request);
};
