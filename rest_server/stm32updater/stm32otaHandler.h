#ifndef STM32OTAHANDLER_H
#define STM32OTAHANDLER_H

#include "esphome/components/web_server_idf/web_server_idf.h"
#include "esphome/components/web_server_base/web_server_base.h"

namespace esphome {
namespace stm32 {

class STM32OTARequestHandler : public AsyncWebHandler
{
public:
  STM32OTARequestHandler(web_server_base::WebServerBase *parent) : parent_(parent) {}
  void handleRequest(AsyncWebServerRequest *request) override;
  void handleUpload(AsyncWebServerRequest *request, const std::string &filename, size_t index, uint8_t *data, size_t len,
                    bool final) override;
  bool canHandle(AsyncWebServerRequest *request) const override
  {
    if (request->method() != HTTP_POST)
      return false;
    char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
    return request->url_to(url_buf) == "/updatestm";
  }

  bool isRequestHandlerTrivial() const override { return false; }

protected:
  uint32_t last_ota_progress_{0};
  uint32_t ota_read_length_{0};
  web_server_base::WebServerBase *parent_;
};

}
}
#endif
