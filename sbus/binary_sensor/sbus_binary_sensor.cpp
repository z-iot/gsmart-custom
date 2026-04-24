#include "esphome/core/log.h"
#include "sbus_binary_sensor.h"

namespace esphome {
namespace sbus {

static const char *const TAG = "sbus.binary_sensor";

void SbusBinarySensor::setup() {
  this->parent_->register_listener(this->sensor_id_, [this](const SbusDatapoint &datapoint) {
    ESP_LOGV(TAG, "MCU reported binary sensor %u is: %s", datapoint.id, ONOFF(datapoint.value_bool));
    this->publish_state(datapoint.value_bool);
  });
}

void SbusBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Sbus Binary Sensor:");
  ESP_LOGCONFIG(TAG, "  Binary Sensor has datapoint ID %u", this->sensor_id_);
}

}  // namespace sbus
}  // namespace esphome
