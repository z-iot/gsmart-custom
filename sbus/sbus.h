#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

namespace esphome
{
  namespace sbus
  {

#include "sbusGlobal.h"

    enum class SbusState : uint8_t
    {
      SBUS_STATE_UNKNOWN = 0,
      SBUS_STATE_NORMAL,
      SBUS_STATE_FLASH,
    };

    struct SbusDatapointListener
    {
      uint8_t datapoint_id;
      std::function<void(SbusDatapoint)> on_datapoint;
    };

    class Sbus : public Component, public uart::UARTDevice
    {
    public:
      float get_setup_priority() const override { return setup_priority::LATE; }
      void setup() override;
      void loop() override;
      void dump_config() override;
      void register_listener(uint8_t datapoint_id, const std::function<void(SbusDatapoint)> &func);
      void set_raw_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value);
      void set_boolean_datapoint_value(uint8_t datapoint_id, bool value);
      void set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value);
      void set_string_datapoint_value(uint8_t datapoint_id, const std::string &value);
      void set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value);
      void set_bitmask_datapoint_value(uint8_t datapoint_id, uint32_t value, uint8_t length);
      void query_datapoint(uint8_t datapoint_id);
      SbusState get_state();
      void add_on_initialized_callback(std::function<void()> callback)
      {
        this->initialized_callback_.add(std::move(callback));
      }

    protected:
      void handle_char_(uint8_t c);
      void handle_datapoints_(const uint8_t *buffer, size_t len);
      optional<SbusDatapoint> get_datapoint_(uint8_t datapoint_id);
      bool validate_message_();

      void handle_command_(uint8_t command, const uint8_t *buffer, size_t len);
      void send_raw_command_(SbusCommand command);
      void process_command_queue_();
      void send_command_(const SbusCommand &command);
      void send_empty_command_(SbusCommandType command);
      void set_numeric_datapoint_value_(uint8_t datapoint_id, SbusDatapointType datapoint_type, uint32_t value,
                                        uint8_t length, bool forced);
      void set_string_datapoint_value_(uint8_t datapoint_id, const std::string &value, bool forced);
      void set_raw_datapoint_value_(uint8_t datapoint_id, const std::vector<uint8_t> &value, bool forced);
      void send_datapoint_command_(uint8_t datapoint_id, SbusDatapointType datapoint_type, std::vector<uint8_t> data);

      SbusState state_ = SbusState::SBUS_STATE_UNKNOWN;
      uint32_t last_command_timestamp_ = 0;
      uint32_t last_rx_char_timestamp_ = 0;
      std::vector<SbusDatapointListener> listeners_;
      std::vector<SbusDatapoint> datapoints_;
      std::vector<uint8_t> rx_message_;
      std::vector<SbusCommand> command_queue_;
      CallbackManager<void()> initialized_callback_{};
    };

  } // namespace sbus
} // namespace esphome
