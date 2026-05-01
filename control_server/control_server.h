#pragma once

#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/component.h"

namespace esphome {
namespace control_server {

// Serves the device-local web UI (statika z WWWData) on the root path.
// Pairs with deck_server (auth) and optionally with mobile_api (REST).
class ControlServer : public Component {
 public:
  ControlServer(web_server_base::WebServerBase *base) : base_(base) {}

  void setup() override;

  // After deck_server (WIFI - 0.5f) so auth services already exist.
  float get_setup_priority() const override { return setup_priority::WIFI - 1.0f; }

 protected:
  web_server_base::WebServerBase *base_;
};

}  // namespace control_server
}  // namespace esphome
