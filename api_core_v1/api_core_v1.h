#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/json/json_util.h"
#include "esphome/core/automation.h"
#include <string>
#include <functional>

namespace esphome {
namespace api_core_v1 {

struct IdentifyRequest {
  std::string target_mac;
  std::string pattern;
  std::string sound;
  uint32_t duration_sec{3};
  bool light{true};
  bool sound_enabled{true};
};

class ApiCoreV1 : public Component {
 public:
  std::string get_version_path() const { return "v1"; }

  // Info & Status
  void build_info(JsonObject root);
  void build_status(JsonObject root);
  void build_diagnostics(JsonObject root);
  void build_consumption(JsonObject root);

  // Network
  void build_network(JsonObject root);
  void build_network_scan(JsonObject root);
  bool apply_network(JsonObject root);
  void build_mqtt(JsonObject root);

  // Region
  void build_region(JsonObject root);
  bool apply_region(JsonObject root);
  void build_region_devices(JsonObject root);
  void ping_region();

  // Scheduler
  void build_scheduler(JsonObject root);
  bool apply_scheduler(JsonObject root);
  bool apply_scheduler_state(JsonObject root);

  // Settings
  void build_settings_consumables(JsonObject root);
  bool apply_settings_consumables(JsonObject root);
  void build_settings_modes(JsonObject root);
  bool apply_settings_modes(JsonObject root);

  // Control
  bool handle_control_mode(JsonObject root, JsonObject response);
  void handle_identify(JsonObject root, JsonObject response);
  bool handle_api_config(JsonObject root, JsonObject response);
  bool handle_api_manual_control(JsonObject root, JsonObject response);

  // Callbacks for actions
  void add_on_identify_callback(std::function<void(IdentifyRequest)> &&callback) {
    this->identify_callback_.add(std::move(callback));
  }

  void trigger_identify(IdentifyRequest request) { this->identify_callback_.call(request); }

  CallbackManager<void(IdentifyRequest)> *get_identify_callback() { return &this->identify_callback_; }

 protected:
  CallbackManager<void(IdentifyRequest)> identify_callback_{};
};

class IdentifyTrigger : public Trigger<IdentifyRequest> {
 public:
  explicit IdentifyTrigger(ApiCoreV1 *parent) {
    parent->add_on_identify_callback([this](IdentifyRequest request) { this->trigger(request); });
  }
};

}  // namespace api_core_v1
}  // namespace esphome
