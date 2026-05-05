#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/json/json_util.h"

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

#include "esphome/core/controller.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_LIGHT
#include "esphome/components/light/light_state.h"
#endif
#ifdef USE_FAN
#include "esphome/components/fan/fan.h"
#endif
#ifdef USE_COVER
#include "esphome/components/cover/cover.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_LOCK
#include "esphome/components/lock/lock.h"
#endif

namespace esphome {

namespace web_server_idf {
class AsyncWebServerRequest;
class AsyncWebServerResponse;
class AsyncEventSource;
class AsyncWebHandler;
}  // namespace web_server_idf

namespace web_server {

class WebServer;

#if USE_ESP32
using WebServerRequest = web_server_idf::AsyncWebServerRequest;
using WebServerResponse = web_server_idf::AsyncWebServerResponse;
using WebEventSource = web_server_idf::AsyncEventSource;
using WebHandler = web_server_idf::AsyncWebHandler;
#else
using WebServerRequest = AsyncWebServerRequest;
using WebServerResponse = AsyncWebServerResponse;
using WebEventSource = AsyncEventSource;
using WebHandler = AsyncWebHandler;
#endif

class EspServer : public Component, public WebHandler, public ::esphome::Controller {
 public:
  EspServer(web_server_base::WebServerBase *base, WebServer *parent);

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void setup_interval();

  bool canHandle(WebServerRequest *request) const override;
  void handleRequest(WebServerRequest *request) override;

  void set_include_internal(bool include_internal) { include_internal_ = include_internal; }
  void set_expose_log(bool expose_log) { expose_log_ = expose_log; }

  void handle_index_request(WebServerRequest *request);
  void close_event_sources(const char *reason = nullptr);

#ifdef USE_SENSOR
  void on_sensor_update(sensor::Sensor *obj) override;
#endif
#ifdef USE_BINARY_SENSOR
  void on_binary_sensor_update(binary_sensor::BinarySensor *obj) override;
#endif
#ifdef USE_SWITCH
  void on_switch_update(switch_::Switch *obj) override;
#endif
#ifdef USE_LIGHT
  void on_light_update(light::LightState *obj) override;
#endif
#ifdef USE_FAN
  void on_fan_update(fan::Fan *obj) override;
#endif
#ifdef USE_COVER
  void on_cover_update(cover::Cover *obj) override;
#endif
#ifdef USE_TEXT_SENSOR
  void on_text_sensor_update(text_sensor::TextSensor *obj) override;
#endif
#ifdef USE_NUMBER
  void on_number_update(number::Number *obj) override;
#endif
#ifdef USE_SELECT
  void on_select_update(select::Select *obj) override;
#endif
#ifdef USE_LOCK
  void on_lock_update(lock::Lock *obj) override;
#endif

#ifdef USE_LOGGER
  void on_log(uint8_t level, const char *tag, const char *message, size_t message_len);
#endif

 protected:
  web_server_base::WebServerBase *base_;
  WebServer *parent_;
  WebEventSource events_;
  bool include_internal_{false};
  bool expose_log_{true};
};

extern EspServer *global_esp_server;

}  // namespace web_server
}  // namespace esphome
