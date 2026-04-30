#pragma once

#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/component.h"

namespace esphome {
namespace stm32_ota {

class STM32OTA : public Component {
 public:
  STM32OTA(web_server_base::WebServerBase *base) : base_(base) {}

  void setup() override;

  float get_setup_priority() const override { return setup_priority::WIFI - 1.0f; }

 protected:
  web_server_base::WebServerBase *base_;
};

}  // namespace stm32_ota
}  // namespace esphome
