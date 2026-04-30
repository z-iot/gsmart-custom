#pragma once

#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/component.h"

#include "AuthenticationService.h"
#include "SecurityService.h"

namespace esphome {
namespace gsmart_server {

// Shared HTTP base for control_server and mobile_api: owns the AsyncWebServer
// (via web_server_base) and the singleton auth/security services so both
// upstream-facing components share the same JWT secret and session state.
class GsmartServer : public Component {
 public:
  GsmartServer(web_server_base::WebServerBase *base) : base_(base) {}

  void setup() override;

  // Run before control_server / mobile_api so they can read security() / auth()
  // pointers from setup().
  float get_setup_priority() const override { return setup_priority::WIFI - 0.5f; }

  std::shared_ptr<AsyncWebServer> server() {
    return std::shared_ptr<AsyncWebServer>(this->base_->get_server(), [](AsyncWebServer *) {});
  }
  ::SecurityService *security() { return this->security_; }
  ::AuthenticationService *auth() { return this->auth_; }

 protected:
  web_server_base::WebServerBase *base_;
  ::SecurityService *security_{nullptr};
  ::AuthenticationService *auth_{nullptr};
};

extern GsmartServer *gsmartServer;

}  // namespace gsmart_server
}  // namespace esphome
