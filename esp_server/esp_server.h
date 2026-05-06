#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/web_server_idf/web_server_idf.h"
#include "esphome/components/json/json_util.h"

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

#include "esphome/core/controller.h"
#include "esphome/core/component_iterator.h"

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
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_LOCK
#include "esphome/components/lock/lock.h"
#endif
#ifdef USE_CLIMATE
#include "esphome/components/climate/climate.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome {

struct UrlMatch {
  StringRef domain;
  StringRef id;
  StringRef method;
  bool valid;
};

namespace web_server {


#ifdef USE_ESP32
using WebServerRequest = ::esphome::web_server_idf::AsyncWebServerRequest;
using WebServerResponse = ::esphome::web_server_idf::AsyncWebServerResponse;
using WebEventSource = ::esphome::web_server_idf::AsyncEventSource;
using WebHandler = ::esphome::web_server_idf::AsyncWebHandler;
#else
using WebServerRequest = AsyncWebServerRequest;
using WebServerResponse = AsyncWebServerResponse;
using WebEventSource = AsyncEventSource;
using WebHandler = AsyncWebHandler;
#endif

class EspServer : public Component, public WebHandler, public ::esphome::Controller, public ComponentIterator {
 public:
  EspServer(web_server_base::WebServerBase *base);

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

#ifdef USE_LOGGER
  void on_log(uint8_t level, const char *tag, const char *message, size_t message_len);
#endif

#ifdef USE_BINARY_SENSOR
  bool on_binary_sensor(binary_sensor::BinarySensor *binary_sensor) override;
#endif
#ifdef USE_COVER
  bool on_cover(cover::Cover *cover) override;
#endif
#ifdef USE_SENSOR
  void on_sensor_update(sensor::Sensor *obj) override;
#endif
#ifdef USE_BINARY_SENSOR
  void on_binary_sensor_update(binary_sensor::BinarySensor *obj) override;
#endif
#ifdef USE_SWITCH
  void on_switch_update(switch_::Switch *obj) override;
#endif
#ifdef USE_TEXT_SENSOR
  void on_text_sensor_update(text_sensor::TextSensor *obj) override;
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
#ifdef USE_NUMBER
  void on_number_update(number::Number *obj) override;
#endif
#ifdef USE_SELECT
  void on_select_update(select::Select *obj) override;
#endif
#ifdef USE_CLIMATE
  void on_climate_update(climate::Climate *obj) override;
#endif

#ifdef USE_FAN
  bool on_fan(fan::Fan *fan) override;
#endif
#ifdef USE_LIGHT
  bool on_light(light::LightState *light) override;
#endif
#ifdef USE_SENSOR
  bool on_sensor(sensor::Sensor *sensor) override;
#endif
#ifdef USE_SWITCH
  bool on_switch(switch_::Switch *a_switch) override;
#endif
#ifdef USE_BUTTON
  bool on_button(button::Button *button) override;
#endif
#ifdef USE_TEXT_SENSOR
  bool on_text_sensor(text_sensor::TextSensor *text_sensor) override;
#endif
#ifdef USE_CLIMATE
  bool on_climate(climate::Climate *climate) override;
#endif
#ifdef USE_NUMBER
  bool on_number(number::Number *number) override;
#endif
#ifdef USE_DATETIME_DATE
  bool on_date(datetime::DateEntity *date) override;
#endif
#ifdef USE_DATETIME_TIME
  bool on_time(datetime::TimeEntity *time) override;
#endif
#ifdef USE_DATETIME_DATETIME
  bool on_datetime(datetime::DateTimeEntity *datetime) override;
#endif
#ifdef USE_TEXT
  bool on_text(text::Text *text) override;
#endif
#ifdef USE_SELECT
  bool on_select(select::Select *select) override;
#endif
#ifdef USE_LOCK
  bool on_lock(lock::Lock *a_lock) override;
#endif
#ifdef USE_VALVE
  bool on_valve(valve::Valve *valve) override;
#endif
#ifdef USE_ALARM_CONTROL_PANEL
  bool on_alarm_control_panel(alarm_control_panel::AlarmControlPanel *a_alarm_control_panel) override;
#endif
#ifdef USE_EVENT
  bool on_event(event::Event *event) override;
#endif
#ifdef USE_UPDATE
  bool on_update(update::UpdateEntity *update) override;
#endif

 protected:
  web_server_base::WebServerBase *base_;
  void handle_sensor_request(WebServerRequest *request, const ::esphome::UrlMatch &match);
  void handle_switch_request(WebServerRequest *request, const ::esphome::UrlMatch &match);
  void handle_button_request(WebServerRequest *request, const ::esphome::UrlMatch &match);
  void handle_binary_sensor_request(WebServerRequest *request, const ::esphome::UrlMatch &match);
  void handle_fan_request(WebServerRequest *request, const ::esphome::UrlMatch &match);
  void handle_light_request(WebServerRequest *request, const ::esphome::UrlMatch &match);
  void handle_text_sensor_request(WebServerRequest *request, const ::esphome::UrlMatch &match);
  void handle_cover_request(WebServerRequest *request, const ::esphome::UrlMatch &match);
  void handle_number_request(WebServerRequest *request, const ::esphome::UrlMatch &match);
  void handle_select_request(WebServerRequest *request, const ::esphome::UrlMatch &match);
  void handle_lock_request(WebServerRequest *request, const ::esphome::UrlMatch &match);
  void handle_climate_request(WebServerRequest *request, const ::esphome::UrlMatch &match);

  std::string sensor_json_(sensor::Sensor *obj, float value, bool include_metadata = false);
  std::string binary_sensor_json_(binary_sensor::BinarySensor *obj, bool value, bool include_metadata = false);
  std::string switch_json_(switch_::Switch *obj, bool value, bool include_metadata = false);
  std::string text_sensor_json_(text_sensor::TextSensor *obj, const std::string &value, bool include_metadata = false);
#ifdef USE_LIGHT
  std::string light_json_(light::LightState *obj, bool include_metadata = false);
#endif
#ifdef USE_FAN
  std::string fan_json_(fan::Fan *obj, bool include_metadata = false);
#endif
#ifdef USE_COVER
  std::string cover_json_(cover::Cover *obj, bool include_metadata = false);
#endif
#ifdef USE_NUMBER
  std::string number_json_(number::Number *obj, float value, bool include_metadata = false);
#endif
#ifdef USE_SELECT
  std::string select_json_(select::Select *obj, const std::string &value, bool include_metadata = false);
#endif
#ifdef USE_BUTTON
  std::string button_json_(button::Button *obj, bool include_metadata = false);
#endif
#ifdef USE_LOCK
  std::string lock_json_(lock::Lock *obj, bool include_metadata = false);
#endif
#ifdef USE_CLIMATE
  std::string climate_json_(climate::Climate *obj, bool include_metadata = false);
#endif

  WebEventSource events_;
  bool include_internal_{false};
  bool expose_log_{true};
};

extern EspServer *global_esp_server;

}  // namespace web_server
}  // namespace esphome
