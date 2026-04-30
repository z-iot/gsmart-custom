#pragma once

#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/component.h"

namespace esphome {
namespace mobile_api {

class MobileApi : public Component {
 public:
  MobileApi(web_server_base::WebServerBase *base) : base_(base) {}

  void setup() override;

  // After gsmart_server (WIFI - 0.5f) so auth/security services already exist.
  float get_setup_priority() const override { return setup_priority::WIFI - 1.0f; }

 protected:
  web_server_base::WebServerBase *base_;
};

}  // namespace mobile_api
}  // namespace esphome
