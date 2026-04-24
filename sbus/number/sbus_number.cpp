#include "esphome/core/log.h"
#include "sbus_number.h"

namespace esphome {
namespace sbus {

static const char *const TAG = "sbus.number";

void SbusNumber::setup() {
  this->parent_->register_listener(this->number_id_, [this](const SbusDatapoint &datapoint) {
    if (datapoint.type == SbusDatapointType::INTEGER) {
      ESP_LOGV(TAG, "MCU reported number %u is: %d", datapoint.id, datapoint.value_int);
      this->publish_state(datapoint.value_int);
    }
    // this->type_ = datapoint.type;
  });
}

void SbusNumber::control(float value) {
  ESP_LOGV(TAG, "Setting number %u: %f", this->number_id_, value);
  this->parent_->set_integer_datapoint_value(this->number_id_, value);
  // if (this->type_ == SbusDatapointType::INTEGER) {
  //   this->parent_->set_integer_datapoint_value(this->number_id_, value);
  // }
  // this->publish_state(value);
}

void SbusNumber::dump_config() {
  LOG_NUMBER("", "Sbus Number", this);
  ESP_LOGCONFIG(TAG, "  Number has datapoint ID %u", this->number_id_);
}

}  // namespace sbus
}  // namespace esphome
