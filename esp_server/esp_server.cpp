#ifdef USE_ARDUINO

#include "esp_server.h"

// #include "esphome/core/log.h"
// #include "esphome/core/application.h"
// #include "esphome/core/entity_base.h"
// #include "esphome/core/util.h"
// #include "esphome/components/json/json_util.h"
// #include "esphome/components/network/util.h"
#ifdef USE_UDPSERVER
#include "esphome/components/udp_server/udp_server.h"
#endif
#ifdef USE_STORAGE
#include "esphome/components/storage/store.h"
#endif

namespace esphome
{
  namespace web_server
  {

    // static const char *const TAG = "esp_server";

    void EspServer::setup()
    {
      WebServer::setup();
#ifdef USE_UDPSERVER
      if (udp_server::udpServer != nullptr)
      {
        udp_server::udpServer->add_on_neighbor_callback(
            [this](udp_server::DeviceItem *deviceItem)
            { this->events_.send(deviceItem->toEventMessage().c_str(), "neighbor"); });
        udp_server::udpServer->add_on_control_callback(
            [this](udp_server::PacketControl packet)
            { this->events_.send(udp_server::packetControlToJsonStr(packet).c_str(), "udp_control"); });
        udp_server::udpServer->add_on_status_callback(
            [this](udp_server::PacketStatus packet)
            { this->events_.send(udp_server::packetStatusToJsonStr(packet).c_str(), "udp_status"); });
        udp_server::udpServer->add_on_identity_callback(
            [this](udp_server::PacketIdentity packet)
            { this->events_.send(udp_server::packetIdentityToJsonStr(packet).c_str(), "udp_identity"); });
      }
#endif
#ifdef USE_STORAGE
      if (storage::store != nullptr)
      {
        storage::store->add_on_situation_change(
            [this]()
            { this->events_.send(storage::store->global->situationToJsonStr().c_str(), "situation"); });
      }

      // this->events_.onConnect([this](AsyncEventSourceClient *client)
      //                         { client->send(storage::store->global->situationToJsonStr().c_str(), "situation", millis(), 30000); });
#endif
    }

    bool EspServer::canHandle(AsyncWebServerRequest *request) const
    {

      if (request->url() == "/esp")
        return true;
      if (request->url() == "/")
        return false;
      return WebServer::canHandle(request);
    }

    void EspServer::handleRequest(AsyncWebServerRequest *request)
    {
      if (request->url() == "/esp")
      {
        this->handle_index_request(request);
        return;
      }
      if (request->url() == "/")
      {
        return;
      }
      WebServer::handleRequest(request);
    }

  } // namespace web_server
} // namespace esphome

#endif // USE_ARDUINO
