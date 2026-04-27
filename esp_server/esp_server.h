#pragma once

// #ifdef USE_ARDUINO

#include "esphome/components/web_server_base/web_server_base.h"
#include "web_server.h"

namespace esphome {
namespace web_server {

class EspServer : public WebServer {
 public:
  EspServer(web_server_base::WebServerBase *base) : WebServer(base) {}

  void setup() override;
  // void loop() override;
  // void dump_config() override;
  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;

 protected:
};

}  // namespace web_server
}  // namespace esphome

// #endif  // USE_ARDUINO
