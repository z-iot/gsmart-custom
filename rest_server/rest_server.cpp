// #ifdef USE_ARDUINO

#include "rest_server.h"
// #include "esphome/core/application.h"
#include "WWWData.h"

#include "stm32updater/stm32otaHandler.h"

#include "ArduinoJsonJWT.h"
#include "AuthenticationService.h"
#include "SecurityService.h"

#include "InfoFeature.h"
#include "InfoSystem.h"
#include "InfoNeighborhood.h"
#include "ConfigDevice.h"
#include "ConfigScheduller.h"
#include "ConfigMode.h"
#include "ConfigTreatment.h"
#include "ConfigConnect.h"
#include "ConfigSecurity.h"
#include "ConfigConsumable.h"

#include "ConfigDef.h"
#include "ConfigData.h"

namespace esphome
{
  namespace rest_server
  {

    void RestServer::setupServer()
    {
      std::shared_ptr<AsyncWebServer> server(this->base_->get_server(), [](AsyncWebServer *) {});
      // Serve static resources from PROGMEM
      WWWData::registerRoutes(
          [server, this](const String &uri, const String &contentType, const uint8_t *content, size_t len)
          {
            ArRequestHandlerFunction requestHandler = [contentType, content, len](AsyncWebServerRequest *request)
            {
              AsyncWebServerResponse *response = request->beginResponse(200, contentType.c_str(), content, len);
              response->addHeader("Content-Encoding", "gzip");
              request->send(response);
            };

            // Serving non matching get requests with "/index.html"
            // OPTIONS get a straight up 200 response
            if (uri.equals("/index.html"))
            {
              server->on(uri.c_str(), HTTP_GET, ArRequestHandlerFunction(requestHandler));

              server->onNotFound([requestHandler](AsyncWebServerRequest *request)
                                 {
                if (request->method() == HTTP_GET) {
                  requestHandler(request);
                } else if (request->method() == HTTP_OPTIONS) {
                  request->send(200);
                } else {
                  request->send(404);
                } });
            }
            else
            {
              server->on(uri.c_str(), HTTP_GET, ArRequestHandlerFunction(requestHandler));
          }
          });

      // STM32OTA
      this->base_->add_handler(new stm32::STM32OTARequestHandler(this->base_));

      // rest info — must be heap-allocated; handlers bind `this` and would dangle
      // if these were stack locals destroyed when setupServer() returns.
      new InfoFeature(server);
      new InfoSystem(server);
      new InfoNeighborhood(server);
      // rest config
      new ConfigDevice(server);
      new ConfigScheduller(server);
      new ConfigMode(server);
      new ConfigTreatment(server);
      new ConfigConnect(server);
      new ConfigSecurity(server);
      new ConfigConsumable(server);

      new ConfigDef(server);
      new ConfigData(server);


      SecurityService *securityService = new SecurityService(server);
      securityService->begin();
      new AuthenticationService(server, securityService);


#if defined(ENABLE_CORS)
      DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", CORS_ORIGIN);
      DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Accept, Content-Type, Authorization");
      DefaultHeaders::Instance().addHeader("Access-Control-Allow-Credentials", "true");
#endif
    }

  }
}

// #endif  // USE_ARDUINO
