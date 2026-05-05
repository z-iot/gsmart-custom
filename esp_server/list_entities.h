#pragma once

// Forward declarations to avoid dependency cycles
namespace esphome {
namespace web_server_idf {
class AsyncWebServerRequest;
}
}

// Include the official one
#include "esphome/components/web_server/list_entities.h"

namespace esphome {
namespace web_server {

#if USE_ESP32
using WebServerRequest = web_server_idf::AsyncWebServerRequest;
#else
class AsyncWebServerRequest;
using WebServerRequest = AsyncWebServerRequest;
#endif

}  // namespace web_server
}  // namespace esphome
