#include "esphome/core/log.h"

#include "automation.h"

static const char *const TAG = "sbus.automation";

namespace esphome {
namespace sbus {

void check_expected_datapoint(const SbusDatapoint &dp, SbusDatapointType expected) {
  if (dp.type != expected) {
    ESP_LOGW(TAG, "Sbus sensor %u expected datapoint type %#02hhX but got %#02hhX", dp.id,
             static_cast<uint8_t>(expected), static_cast<uint8_t>(dp.type));
  }
}

SbusRawDatapointUpdateTrigger::SbusRawDatapointUpdateTrigger(Sbus *parent, uint8_t sensor_id) {
  parent->register_listener(sensor_id, [this](const SbusDatapoint &dp) {
    check_expected_datapoint(dp, SbusDatapointType::RAW);
    this->trigger(dp.value_raw);
  });
}

SbusBoolDatapointUpdateTrigger::SbusBoolDatapointUpdateTrigger(Sbus *parent, uint8_t sensor_id) {
  parent->register_listener(sensor_id, [this](const SbusDatapoint &dp) {
    check_expected_datapoint(dp, SbusDatapointType::BOOLEAN);
    this->trigger(dp.value_bool);
  });
}

SbusIntDatapointUpdateTrigger::SbusIntDatapointUpdateTrigger(Sbus *parent, uint8_t sensor_id) {
  parent->register_listener(sensor_id, [this](const SbusDatapoint &dp) {
    check_expected_datapoint(dp, SbusDatapointType::INTEGER);
    this->trigger(dp.value_int);
  });
}

SbusUIntDatapointUpdateTrigger::SbusUIntDatapointUpdateTrigger(Sbus *parent, uint8_t sensor_id) {
  parent->register_listener(sensor_id, [this](const SbusDatapoint &dp) {
    check_expected_datapoint(dp, SbusDatapointType::INTEGER);
    this->trigger(dp.value_uint);
  });
}

SbusStringDatapointUpdateTrigger::SbusStringDatapointUpdateTrigger(Sbus *parent, uint8_t sensor_id) {
  parent->register_listener(sensor_id, [this](const SbusDatapoint &dp) {
    check_expected_datapoint(dp, SbusDatapointType::STRING);
    this->trigger(dp.value_string);
  });
}

}  // namespace sbus
}  // namespace esphome
