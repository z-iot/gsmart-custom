#pragma once

#include "esphome/components/web_server_base/web_server_base.h"

#include "esphome/components/json/json_util.h"

#define ConfigScheduller_PATH "/cfg/scheduller"

class ConfigScheduller {
 public:
  ConfigScheduller(std::shared_ptr<AsyncWebServer> server);

 private:
  void get(AsyncWebServerRequest* request);
  void post(AsyncWebServerRequest *request, JsonVariant &json);
};

