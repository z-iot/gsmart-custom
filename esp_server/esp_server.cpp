#ifdef USE_ARDUINO

#include <cinttypes>
#include <cstring>

#include "esp_server.h"
#include "esphome/components/json/json_util.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "sdkconfig.h"
#endif

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

    static const char *const TAG = "esp_server";

    EspServer *global_esp_server = nullptr;

#ifdef USE_ESP32
    static uint32_t diag_heap_free()
    {
      return static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_8BIT));
    }
    static uint32_t diag_heap_largest()
    {
      return static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    }
    static uint32_t diag_heap_min_free()
    {
      return static_cast<uint32_t>(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
    }
    static uint32_t diag_stack_hwm()
    {
      return static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr));
    }
#else
    static uint32_t diag_heap_free() { return 0; }
    static uint32_t diag_heap_largest() { return 0; }
    static uint32_t diag_heap_min_free() { return 0; }
    static uint32_t diag_stack_hwm() { return 0; }
#endif

    void EspServer::setup()
    {
      global_esp_server = this;
      WebServer::setup();

      this->set_interval(30000, [this]()
                         {
                           const auto clients = this->get_event_source_count();
                           if (clients == 0)
                             return;
                           ESP_LOGD(TAG, "diag: sse=%zu buffered=%zu deferred=%zu heap=%" PRIu32 " largest=%" PRIu32 " min=%" PRIu32,
                                    clients, this->get_event_source_buffered_bytes(), this->get_event_source_deferred_count(),
                                    diag_heap_free(), diag_heap_largest(), diag_heap_min_free()); });
#ifdef USE_UDPSERVER
      if (udp_server::udpServer != nullptr)
      {
        udp_server::udpServer->add_on_neighbor_callback(
            [this](udp_server::DeviceItem *deviceItem)
            { this->events_.try_send_nodefer(deviceItem->toEventMessage().c_str(), "neighbor"); });
        udp_server::udpServer->add_on_control_callback(
            [this](udp_server::PacketControl packet)
            { this->events_.try_send_nodefer(udp_server::packetControlToJsonStr(packet).c_str(), "udp_control"); });
        udp_server::udpServer->add_on_status_callback(
            [this](udp_server::PacketStatus packet)
            { this->events_.try_send_nodefer(udp_server::packetStatusToJsonStr(packet).c_str(), "udp_status"); });
        udp_server::udpServer->add_on_identity_callback(
            [this](udp_server::PacketIdentity packet)
            { this->events_.try_send_nodefer(udp_server::packetIdentityToJsonStr(packet).c_str(), "udp_identity"); });
      }
#endif
#ifdef USE_STORAGE
      if (storage::store != nullptr)
      {
        storage::store->add_on_situation_change(
            [this]()
            { this->events_.try_send_nodefer(storage::store->global->situationToJsonStr().c_str(), "situation"); });
      }

      // this->events_.onConnect([this](AsyncEventSourceClient *client)
      //                         { client->send(storage::store->global->situationToJsonStr().c_str(), "situation", millis(), 30000); });
#endif
    }

    bool EspServer::canHandle(AsyncWebServerRequest *request) const
    {
#ifdef USE_ESP32
      char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
      auto url = request->url_to(url_buf);
      const size_t url_len = url.size();
      auto starts_with = [&](const char *prefix, size_t plen) {
        return url.size() >= plen && std::memcmp(url.c_str(), prefix, plen) == 0;
      };
#else
      const auto &url = request->url();
      const size_t url_len = url.length();
      auto starts_with = [&](const char *prefix, size_t plen) { return url.startsWith(prefix); };
#endif
      if ((url == "/esp" || starts_with("/esp/", 5)) && url != "/esp/events")
      {
        ESP_LOGI(TAG,
                 "canHandle method=%u url=%.*s heap=%" PRIu32 " largest=%" PRIu32 " stack_hwm=%" PRIu32,
                 request->method(), (int) url_len, url.c_str(), diag_heap_free(), diag_heap_largest(),
                 diag_stack_hwm());
      }
      if (url == "/esp")
        return true;
      if (url == "/esp/diag")
        return true;
      if (url == "/esp/close-events")
        return true;
      if (url == "/")
        return false;
      if (starts_with("/api/", 5))
        return false;
      return WebServer::canHandle(request);
    }

    void EspServer::handleRequest(AsyncWebServerRequest *request)
    {
#ifdef USE_ESP32
      char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
      auto url = request->url_to(url_buf);
      const size_t url_len = url.size();
#else
      const auto &url = request->url();
      const size_t url_len = url.length();
#endif
      if ((url == "/esp" || (url_len >= 5 && std::memcmp(url.c_str(), "/esp/", 5) == 0)) && url != "/esp/events")
      {
        ESP_LOGI(TAG,
                 "handle begin method=%u url=%.*s heap=%" PRIu32 " largest=%" PRIu32 " stack_hwm=%" PRIu32,
                 request->method(), (int) url_len, url.c_str(), diag_heap_free(), diag_heap_largest(),
                 diag_stack_hwm());
      }
      if (url == "/esp")
      {
        this->handle_index_request(request);
        ESP_LOGI(TAG, "handle /esp index sent stack_hwm=%" PRIu32, diag_stack_hwm());
        return;
      }
      if (url == "/esp/diag")
      {
        std::string data = json::build_json([this](JsonObject root)
                                            {
                                              root["uptime_ms"] = millis();
                                              root["sse_clients"] = this->get_event_source_count();
                                              root["sse_buffered"] = this->get_event_source_buffered_bytes();
                                              root["sse_deferred"] = this->get_event_source_deferred_count();
                                              root["heap_free"] = diag_heap_free();
                                              root["heap_largest"] = diag_heap_largest();
                                              root["heap_min_free"] = diag_heap_min_free();
#ifdef USE_ESP32
                                              root["lwip_max_sockets"] = CONFIG_LWIP_MAX_SOCKETS;
#endif
                                            });
        request->send(200, "application/json", data.c_str());
        ESP_LOGI(TAG, "handle /esp/diag sent bytes=%zu stack_hwm=%" PRIu32, data.size(), diag_stack_hwm());
        return;
      }
      if (url == "/esp/close-events")
      {
        this->close_event_sources("manual /esp/close-events");
        request->send(200, "application/json", "{\"ok\":true}");
        ESP_LOGI(TAG, "handle /esp/close-events sent stack_hwm=%" PRIu32, diag_stack_hwm());
        return;
      }
      if (url == "/")
      {
        request->redirect("/esp");
        return;
      }
      WebServer::handleRequest(request);
      if ((url == "/esp" || (url_len >= 5 && std::memcmp(url.c_str(), "/esp/", 5) == 0)) && url != "/esp/events")
      {
        ESP_LOGI(TAG, "handle delegated done url=%.*s stack_hwm=%" PRIu32, (int) url_len, url.c_str(),
                 diag_stack_hwm());
      }
    }

  } // namespace web_server
} // namespace esphome

#endif // USE_ARDUINO
