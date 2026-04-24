#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sbus/sbus.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace sbus {

class SbusSwitch : public switch_::Switch, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_switch_id(uint8_t switch_id) { this->switch_id_ = switch_id; }

  void set_sbus_parent(Sbus *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override;

  Sbus *parent_;
  uint8_t switch_id_{0};
};

}  // namespace sbus
}  // namespace esphome
