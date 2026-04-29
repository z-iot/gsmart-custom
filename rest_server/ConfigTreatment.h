#pragma once

#include "esphome/components/web_server_base/web_server_base.h"


#define ConfigTreatment_PATH "/cfg/treatment"

class ConfigTreatment {
 public:
  ConfigTreatment(std::shared_ptr<AsyncWebServer> server);
  void get(AsyncWebServerRequest* request);
  void post(AsyncWebServerRequest* request);
};

