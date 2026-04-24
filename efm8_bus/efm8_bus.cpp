#include "efm8_bus.h"
#include "esphome/core/log.h"

namespace esphome
{
  namespace efm8_bus
  {

    static const char *const TAG = "efm8_bus";

    void Efm8BusComponent::setup()
    {
      ESP_LOGCONFIG(TAG, "Setting up efm8_bus... ");
      this->pwm_amounts_.resize(EFM8_OUTPUT_NUM, 0);
      this->clk_pin_->setup();
      this->data_pin_->setup();
      this->clk_pin_->pin_mode(gpio::FLAG_OUTPUT);
      this->data_pin_->pin_mode(gpio::FLAG_OUTPUT);
      // this->data_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
      this->clk_pin_->digital_write(true);
      this->data_pin_->digital_write(false);
      // delay(2);
    }
    void Efm8BusComponent::dump_config()
    {
      ESP_LOGCONFIG(TAG, "efm8_bus:");
      ESP_LOGCONFIG(TAG, "  Efm8 FW version: %u", this->read_version());
      LOG_PIN("  CLK pin: ", this->clk_pin_);
      LOG_PIN("  DATA pin: ", this->data_pin_);
    }

    void Efm8BusComponent::set_output_value_(uint8_t channel, uint8_t value)
    {
      ESP_LOGV(TAG, "set output %u to %u", channel, value);
      if (this->pwm_amounts_[channel] != value)
      {
        this->update_ = true;
      }
      this->pwm_amounts_[channel] = value;
    }

    void Efm8BusComponent::loop()
    {
      uint32_t cur = millis();
      if (this->last_transfer + 5 > cur)
        return;
      this->last_transfer = cur;

      if (this->update_)
      {
        this->write_byte_(EFM8BUS_CMD_PWM);
        for (auto pwm_amount : this->pwm_amounts_)
          this->write_byte_(pwm_amount);
      }
      else
      {
        this->write_byte_(EFM8BUS_CMD_STATUS);
      }

      uint8_t status = this->read_byte_();
      if (!(status & 0xA0))
        ESP_LOGD(TAG, "bad status received  %u", status);

      for (auto *input : this->inputs_)
        input->process(status);

      this->update_ = false;
    }

    uint8_t Efm8BusComponent::read_version()
    {
      // delay(5);
      this->write_byte_(EFM8BUS_CMD_VERSION);
      uint8_t ver = this->read_byte_();
      uint8_t status = this->read_byte_();
      if (!(status & 0xA0))
        return 0;
      return ver;
    }

    void Efm8BusComponent::write_byte_(uint8_t value)
    {
      for (uint8_t i = 8; i > 0; i--)
      {
        this->clk_pin_->digital_write(true);
        this->data_pin_->digital_write(!(value & (1 << (i - 1))));
        this->clk_pin_->digital_write(false);
        delayMicroseconds(5);
      }
      this->clk_pin_->digital_write(true);
      this->data_pin_->digital_write(false);
      delayMicroseconds(8);
    }

    uint8_t Efm8BusComponent::read_byte_()
    {
      this->data_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
      uint8_t value = 0;
      for (uint8_t i = 8; i > 0; i--)
      {
        this->clk_pin_->digital_write(false);
        delayMicroseconds(10);
        bool bitval = !this->data_pin_->digital_read();
        value |= uint8_t(bitval) << (i - 1);
        this->clk_pin_->digital_write(true);
        // delayMicroseconds(5);
      }
      delayMicroseconds(5);
      this->data_pin_->pin_mode(gpio::FLAG_OUTPUT);
      this->data_pin_->digital_write(false);
      delayMicroseconds(10);
      return value;
    }

  }
}
