#include "control_server.h"
#include "WWWData.h"
#include "esphome/components/gsmart_server/web_helpers.h"

namespace esphome {
namespace control_server {

void ControlServer::setup() {
  this->base_->init();

  std::shared_ptr<AsyncWebServer> server(this->base_->get_server(), [](AsyncWebServer *) {});

  WWWData::registerRoutes(
      [server](const String &uri, const String &contentType, const uint8_t *content, size_t len) {
        std::function<void(AsyncWebServerRequest *)> requestHandler =
            [contentType, content, len](AsyncWebServerRequest *request) {
              AsyncWebServerResponse *response = request->beginResponse(200, contentType.c_str(), content, len);
              response->addHeader("Content-Encoding", "gzip");
              request->send(response);
            };

        if (uri.equals("/index.html")) {
          // SPA fallback: any unmatched GET serves index.html. OPTIONS get a 200.
          server->onNotFound([requestHandler](AsyncWebServerRequest *request) {
            if (request->method() == HTTP_GET) {
              requestHandler(request);
            } else if (request->method() == HTTP_OPTIONS) {
              request->send(200);
            } else {
              request->send(404);
            }
          });
          esphome::gsmart_server::on(server, uri.c_str(), HTTP_GET, std::move(requestHandler));
        } else {
          esphome::gsmart_server::on(server, uri.c_str(), HTTP_GET, std::move(requestHandler));
        }
      });

#if defined(ENABLE_CORS)
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", CORS_ORIGIN);
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Accept, Content-Type, Authorization");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Credentials", "true");
#endif
}

}  // namespace control_server
}  // namespace esphome
