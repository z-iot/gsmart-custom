#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace efm8_bus {

#define EFM8BUS_CMD_VERSION 0x21
#define EFM8BUS_CMD_STATUS 0x22
#define EFM8BUS_CMD_PWM 0x23

#define EFM8_OUTPUT_NUM 3
class Efm8Output;

class Efm8Input : public binary_sensor::BinarySensor {
 public:
  void set_channel(uint8_t channel) { channel_ = channel; }
  void process(uint16_t data) { this->publish_state(data & (1 << this->channel_)); }

 protected:
  uint8_t channel_;
};

class Efm8BusComponent : public Component {
 public:
  void set_clk_pin(GPIOPin *clk_pin) { clk_pin_ = clk_pin; }
  void set_data_pin(GPIOPin *data_pin) { data_pin_ = data_pin; }
  void register_input(Efm8Input *channel) { this->inputs_.push_back(channel); }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_output_value_(uint8_t channel, uint8_t value);
  void loop() override;
  // {
    // check datavalid if sdo is high
    // if (!this->sdo_pin_->digital_read()) {
    //   return;
    // }
    // uint16_t touched = 0;
    // for (uint8_t i = 0; i < 16; i++) {
    //   this->scl_pin_->digital_write(false);
    //   delayMicroseconds(2);  // 500KHz
    //   bool bitval = !this->sdo_pin_->digital_read();
    //   this->scl_pin_->digital_write(true);
    //   delayMicroseconds(2);  // 500KHz

    //   touched |= uint16_t(bitval) << i;
    // }
    // for (auto *channel : this->channels_) {
    //   channel->process(touched);
    // }
  // }

 protected:
  void write_byte_(uint8_t value);
  uint8_t read_byte_();
  uint8_t read_version();

  
  GPIOPin *clk_pin_;
  GPIOPin *data_pin_;
  std::vector<Efm8Input *> inputs_{};
  std::vector<uint8_t> pwm_amounts_;
  bool update_{true};
  uint32_t last_transfer{0};
};

class Efm8Output : public output::FloatOutput {
  public:
  void set_parent(Efm8BusComponent *parent) { parent_ = parent; }
  void set_channel(uint8_t channel) { channel_ = channel; }

  protected:
  void write_state(float state) override {
    // if (this->channel_ == 0)
    //   ESP_LOGD("efm8_bus", "set buzzer output %f", state);

    auto amount = uint8_t(state * 255);
    this->parent_->set_output_value_(this->channel_, amount);
  }

  Efm8BusComponent *parent_;
  uint8_t channel_;
};

}
}
