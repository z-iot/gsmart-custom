#include "stm32_ota.h"
#include "stm32otaHandler.h"

namespace esphome {
namespace stm32_ota {

void STM32OTA::setup() {
  this->base_->init();
  this->base_->add_handler(new stm32::STM32OTARequestHandler(this->base_));
}

}  // namespace stm32_ota
}  // namespace esphome
