#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/json/json_util.h"
#include "esphome/core/automation.h"
#include <string>
#include <functional>
#include <utility>

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
  void set_firmware_version(const std::string &firmware_version) { this->firmware_version_ = firmware_version; }

  // Info & Status
  void build_info(JsonObject root);
  void build_status(JsonObject root);
  void build_diagnostics(JsonObject root);
  void set_glink_diagnostics_provider(std::function<void(JsonObject)> provider) {
    this->glink_diagnostics_provider_ = std::move(provider);
  }
  void set_firmware_event_emitter(std::function<void(const char *, JsonObject)> emitter) {
    this->firmware_event_emitter_ = std::move(emitter);
  }
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
  void handle_region_ping(JsonObject root, JsonObject response);

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
  void handle_restart(JsonObject root, JsonObject response);
  void handle_firmware_update(JsonObject root, JsonObject response);
  void handle_factory_reset(JsonObject root, JsonObject response);
  void handle_service_ap(JsonObject root, JsonObject response);
  void handle_clear_region(JsonObject root, JsonObject response);
  void handle_clear_usage(JsonObject root, JsonObject response);
  bool handle_api_config(JsonObject root, JsonObject response);
  bool handle_api_manual_control(JsonObject root, JsonObject response);

  // Callbacks for actions
  void add_on_identify_callback(std::function<void(IdentifyRequest)> &&callback) {
    this->identify_callback_.add(std::move(callback));
  }

  void trigger_identify(IdentifyRequest request) { this->identify_callback_.call(request); }

  CallbackManager<void(IdentifyRequest)> *get_identify_callback() { return &this->identify_callback_; }

 protected:
  std::string get_build_code_() const;
  std::string get_firmware_version_() const;
  std::string get_device_name_() const;
  void sync_preferences_now_() const;
  void emit_firmware_event_(const char *phase, const std::string &role, const std::string &target_serial,
                            const std::string &target_ip, const std::string &version, const std::string &file_id,
                            const std::string &release_id, uint32_t file_size, uint32_t bytes_sent,
                            const char *error = nullptr);

  CallbackManager<void(IdentifyRequest)> identify_callback_{};
  std::string firmware_version_{};
  std::function<void(JsonObject)> glink_diagnostics_provider_{};
  std::function<void(const char *, JsonObject)> firmware_event_emitter_{};
};

class IdentifyTrigger : public Trigger<IdentifyRequest> {
 public:
  explicit IdentifyTrigger(ApiCoreV1 *parent) {
    parent->add_on_identify_callback([this](IdentifyRequest request) { this->trigger(request); });
  }
};

}  // namespace api_core_v1
}  // namespace esphome
