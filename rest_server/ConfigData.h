#pragma once

#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
// #include <SPIFFS.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif

#include <ESPAsyncWebServer.h>

#define ConfigData_PATH "/cfg/config"

class ConfigData {
 public:
  ConfigData(std::shared_ptr<AsyncWebServer> server);

 private:
  void get(AsyncWebServerRequest* request);
  void post(AsyncWebServerRequest* request);
};

