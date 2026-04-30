#pragma once

#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#include <functional>
#include <string>
#include <utility>

namespace esphome {
namespace mobile_api {

struct IdentifyRequest {
  std::string target_mac;
  std::string pattern;
  std::string sound;
  uint32_t duration_sec{3};
  bool light{true};
  bool sound_enabled{true};
};

class MobileApi : public Component {
 public:
  MobileApi(web_server_base::WebServerBase *base) : base_(base) {}

  void setup() override;

  // After gsmart_server (WIFI - 0.5f) so auth/security services already exist.
  float get_setup_priority() const override { return setup_priority::WIFI - 1.0f; }

  void add_on_identify_callback(std::function<void(IdentifyRequest)> &&callback) {
    this->identify_callback_.add(std::move(callback));
  }

  void trigger_identify(IdentifyRequest request) { this->identify_callback_.call(request); }

 protected:
  web_server_base::WebServerBase *base_;
  CallbackManager<void(IdentifyRequest)> identify_callback_{};
};

class IdentifyTrigger : public Trigger<IdentifyRequest> {
 public:
  explicit IdentifyTrigger(MobileApi *parent) {
    parent->add_on_identify_callback([this](IdentifyRequest request) { this->trigger(request); });
  }
};

}  // namespace mobile_api
}  // namespace esphome
