// #ifdef USE_ARDUINO

#include "rest_server.h"
// #include "esphome/core/application.h"
#include "WWWData.h"

// #include "stm32otaHandler.h"

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
      std::shared_ptr<AsyncWebServer> server = this->base_->get_server();
      // Serve static resources from PROGMEM
      WWWData::registerRoutes(
          [server, this](const String &uri, const String &contentType, const uint8_t *content, size_t len)
          {
            ArRequestHandlerFunction requestHandler = [contentType, content, len](AsyncWebServerRequest *request)
            {
              AsyncWebServerResponse *response = request->beginResponse_P(200, contentType, content, len);
              response->addHeader("Content-Encoding", "gzip");
              request->send(response);
            };

            // Serving non matching get requests with "/index.html"
            // OPTIONS get a straight up 200 response
            if (uri.equals("/index.html"))
            {
              AsyncCallbackWebHandler *handler = new AsyncCallbackWebHandler();
              handler->setUri(uri.c_str());
              handler->setMethod(HTTP_GET);
              handler->onRequest(requestHandler);
              this->base_->add_handler(handler);

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
              server->on(uri.c_str(), HTTP_GET, requestHandler);
          }
          });

      // STM32OTA
      // this->base_->add_handler(new stm32::STM32OTARequestHandler(this->base_));

      // rest info
      InfoFeature infoFeature = InfoFeature(server);
      InfoSystem infoSystem = InfoSystem(server);
      InfoNeighborhood infoNeighborhood = InfoNeighborhood(server);
      // rest config
      ConfigDevice configDevice = ConfigDevice(server);
      ConfigScheduller configScheduller = ConfigScheduller(server);
      ConfigMode configMode = ConfigMode(server);
      ConfigTreatment configTreatment = ConfigTreatment(server);
      ConfigConnect configConnect = ConfigConnect(server);
      ConfigSecurity configSecurity = ConfigSecurity(server);
      ConfigConsumable configConsumable = ConfigConsumable(server);

      ConfigDef configDef = ConfigDef(server);
      ConfigData configData = ConfigData(server);


      SecurityService *securityService = new SecurityService(server);
      AuthenticationService *authenticationService = new AuthenticationService(server, securityService);


#if defined(ENABLE_CORS)
      DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", CORS_ORIGIN);
      DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Accept, Content-Type, Authorization");
      DefaultHeaders::Instance().addHeader("Access-Control-Allow-Credentials", "true");
#endif
    }

  }
}

// #endif  // USE_ARDUINO
