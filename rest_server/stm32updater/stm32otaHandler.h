#ifndef STM32OTAHANDLER_H
#define STM32OTAHANDLER_H

#include <ESPAsyncWebServer.h>
#include "esphome/components/web_server_base/web_server_base.h"

namespace esphome {
namespace stm32 {

class STM32OTARequestHandler : public AsyncWebHandler
{
public:
  STM32OTARequestHandler(web_server_base::WebServerBase *parent) : parent_(parent) {}
  void handleRequest(AsyncWebServerRequest *request) override;
  void handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
                    bool final) override;
  bool canHandle(AsyncWebServerRequest *request) override
  {
    return request->url() == "/updatestm" && request->method() == HTTP_POST;
  }

  bool isRequestHandlerTrivial() override { return false; }

protected:
  uint32_t last_ota_progress_{0};
  uint32_t ota_read_length_{0};
  web_server_base::WebServerBase *parent_;
};

}
}
#endif
