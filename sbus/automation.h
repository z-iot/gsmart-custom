#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "sbus.h"

namespace esphome {
namespace sbus {

class SbusDatapointUpdateTrigger : public Trigger<SbusDatapoint> {
 public:
  explicit SbusDatapointUpdateTrigger(Sbus *parent, uint8_t sensor_id) {
    parent->register_listener(sensor_id, [this](const SbusDatapoint &dp) { this->trigger(dp); });
  }
};

class SbusRawDatapointUpdateTrigger : public Trigger<std::vector<uint8_t>> {
 public:
  explicit SbusRawDatapointUpdateTrigger(Sbus *parent, uint8_t sensor_id);
};

class SbusBoolDatapointUpdateTrigger : public Trigger<bool> {
 public:
  explicit SbusBoolDatapointUpdateTrigger(Sbus *parent, uint8_t sensor_id);
};

class SbusIntDatapointUpdateTrigger : public Trigger<int> {
 public:
  explicit SbusIntDatapointUpdateTrigger(Sbus *parent, uint8_t sensor_id);
};

class SbusUIntDatapointUpdateTrigger : public Trigger<uint32_t> {
 public:
  explicit SbusUIntDatapointUpdateTrigger(Sbus *parent, uint8_t sensor_id);
};

class SbusStringDatapointUpdateTrigger : public Trigger<std::string> {
 public:
  explicit SbusStringDatapointUpdateTrigger(Sbus *parent, uint8_t sensor_id);
};

}  // namespace sbus
}  // namespace esphome
