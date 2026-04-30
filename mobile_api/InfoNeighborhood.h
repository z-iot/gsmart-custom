#pragma once

#include "esphome/components/web_server_base/web_server_base.h"


#define InfoNeighborhood_PATH "/api/mobile/v1/neighborhood"

class InfoNeighborhood {
 public:
  InfoNeighborhood(std::shared_ptr<AsyncWebServer> server);

 private:
  void get(AsyncWebServerRequest* request);
};

