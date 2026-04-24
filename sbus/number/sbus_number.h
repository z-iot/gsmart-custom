#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sbus/sbus.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace sbus {

class SbusNumber : public number::Number, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_number_id(uint8_t number_id) { this->number_id_ = number_id; }

  void set_sbus_parent(Sbus *parent) { this->parent_ = parent; }

 protected:
  void control(float value) override;

  Sbus *parent_;
  uint8_t number_id_{0};
  // SbusDatapointType type_{};
};

}  // namespace sbus
}  // namespace esphome
