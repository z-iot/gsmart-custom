#include "esp32_dac_extra.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32

#ifdef USE_ARDUINO
#include <esp32-hal-dac.h>
#include "driver/dac.h"
#endif
#ifdef USE_ESP_IDF
#include <driver/dac.h>
#endif

namespace esphome {
namespace esp32_dac {

static const char *const TAG = "esp32_dac_extra";

void ESP32DACextra::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESP32 DACextra Output...");
  this->pin_->setup();
  this->turn_off();

#ifdef USE_ESP_IDF
  auto channel = pin_->get_pin() == 25 ? DAC_CHANNEL_1 : DAC_CHANNEL_2;
  dac_output_enable(channel);
#endif
}

void ESP32DACextra::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP32 DACextra:");
  LOG_PIN("  Pin: ", this->pin_);
  LOG_FLOAT_OUTPUT(this);
}

void ESP32DACextra::write_state(float state) {
  if (this->pin_->is_inverted())
    state = 1.0f - state;
 

#ifdef USE_ESP_IDF
  auto channel = pin_->get_pin() == 25 ? DAC_CHANNEL_1 : DAC_CHANNEL_2;
  dac_output_voltage(channel, (uint8_t) state);
#endif
#ifdef USE_ARDUINO
  if (state == 0.0f)
  {
    auto channel = this->pin_->get_pin() == 25 ? DAC_CHANNEL_1 : DAC_CHANNEL_2;
    dac_output_disable(channel);
    // dacDisable(this->pin_->get_pin());
    pinMode(this->pin_->get_pin(), OUTPUT);
    digitalWrite(this->pin_->get_pin(), false);
  }
  else if (state == 1.0f)
  {
    auto channel = this->pin_->get_pin() == 25 ? DAC_CHANNEL_1 : DAC_CHANNEL_2;
    dac_output_disable(channel);
    // dacDisable(this->pin_->get_pin());
    pinMode(this->pin_->get_pin(), OUTPUT);
    digitalWrite(this->pin_->get_pin(), true);
  }
  else
  {
    state = state * 255;
    dacWrite(this->pin_->get_pin(), state);
  }
#endif
}

}  // namespace esp32_dac
}  // namespace esphome

#endif
