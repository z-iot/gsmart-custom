#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sbus/sbus.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace sbus {

class SbusBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_sensor_id(uint8_t sensor_id) { this->sensor_id_ = sensor_id; }

  void set_sbus_parent(Sbus *parent) { this->parent_ = parent; }

 protected:
  Sbus *parent_;
  uint8_t sensor_id_{0};
};

}  // namespace sbus
}  // namespace esphome
