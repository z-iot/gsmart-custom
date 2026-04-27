#pragma once

#include "esphome/components/web_server_base/web_server_base.h"


#define ConfigDef_PATH "/cfg/def"

class ConfigDef {
 public:
  ConfigDef(std::shared_ptr<AsyncWebServer> server);

 private:
  void get(AsyncWebServerRequest* request);
  void post(AsyncWebServerRequest* request);
};

