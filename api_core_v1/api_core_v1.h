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

/// One firmware image and where it is going.
///
/// Filled either from a `g-node.firmware.update.set` command or from the answer
/// to the nightly self-service check, which is deliberately the same JSON. The
/// two entry points then share the code below rather than growing a second
/// update path that drifts from the tested one.
struct FirmwareUpdatePlan {
  std::string url;
  std::string version;
  std::string file_id;
  std::string release_id;
  std::string md5;
  std::string ota_password;
  std::string target_serial;
  std::string target_ip;
  uint32_t file_size{0};
  uint16_t ota_port{0};
  /// The payload behind `url` is gzip and the target has to inflate it. Only the
  /// ESP8266 backend can, and it is the only way a rex image fits its 1 MB part.
  /// Never set for a raw image - the target would inflate garbage into flash.
  bool compressed{false};
  /// "manual" for a command somebody sent, "auto" for the nightly window. Rides
  /// along on every firmware.update.* event, so a night of failures in the log
  /// can be told apart from a technician's own attempt.
  std::string trigger{"manual"};
};

/// Reads a plan out of the shared command body. Exposed so the nightly check can
/// parse the very same object the cloud command carries.
FirmwareUpdatePlan parse_firmware_update_plan(JsonObject root);

class ApiCoreV1 : public Component {
 public:
  std::string get_version_path() const { return "v1"; }
  void set_firmware_version(const std::string &firmware_version) { this->firmware_version_ = firmware_version; }
  /// The version this build reports everywhere else - status, diagnostics and
  /// the full heartbeat the cloud stores as lcFwVer. The nightly check compares
  /// against that same value, so it has to come from here and nowhere else.
  std::string firmware_version() const { return this->get_firmware_version_(); }

  // Info & Status
  void build_info(JsonObject root);
  void build_status(JsonObject root);
  void build_diagnostics(JsonObject root);
  void set_glink_diagnostics_provider(std::function<void(JsonObject)> provider) {
    this->glink_diagnostics_provider_ = std::move(provider);
  }
  /// The emitter answers whether the event actually left the device. A firmware
  /// event dropped because the cloud link is not up yet is the difference
  /// between "this update was never reported" and "this update never happened",
  /// and only the caller can decide to try again later.
  void set_firmware_event_emitter(std::function<bool(const char *, JsonObject)> emitter) {
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
  // Kompaktny sucet prevadzky - kolko sa svietilo a kolkokrat. `build_consumption`
  // hovori to iste aj s poliom kanalov; to je na dotaz, nie do kazdej udalosti.
  void build_usage_summary(JsonObject root);
  bool apply_settings_consumables(JsonObject root);
  void build_settings_modes(JsonObject root);
  bool apply_settings_modes(JsonObject root);

  // Control
  bool handle_control_mode(JsonObject root, JsonObject response);
  void handle_identify(JsonObject root, JsonObject response);
  void handle_restart(JsonObject root, JsonObject response);
  void handle_firmware_update(JsonObject root, JsonObject response);

  // Firmware update, shared by the cloud command and the nightly check.

  /// Downloads and installs `plan` on this device. Returns once the download has
  /// been scheduled, not once it is done: a successful self update never
  /// returns at all, it reboots. `delay_ms` exists so a command can answer its
  /// caller before the device goes off the air; the nightly check passes 0.
  bool start_self_firmware_update(const FirmwareUpdatePlan &plan, uint32_t delay_ms = 250);

  /// Streams `plan` to a region member over ESPHome OTA on the LAN and blocks
  /// until it is installed or has failed. Emits started/progress/completed or
  /// failed on the way, so callers do not each re-report the same thing.
  bool push_firmware_to_member(const FirmwareUpdatePlan &plan);

  /// Sends one firmware.update.* event. Returns false when the cloud link could
  /// not take it, which is what lets a post-reboot verdict wait and retry.
  bool report_firmware_event(const char *phase, const std::string &role, const FirmwareUpdatePlan &plan,
                             uint32_t bytes_sent = 0, const char *error = nullptr);
  void handle_factory_reset(JsonObject root, JsonObject response);
  void handle_service_ap(JsonObject root, JsonObject response);
  void handle_clear_region(JsonObject root, JsonObject response);
  /// Zahodi tyzdenny kalendar. Chodi s `handle_clear_region`, aby stary plan
  /// neprezil vymazanie regionu a nespinal kus podla cudzej miestnosti.
  void clear_schedule_into(JsonObject response);
  void handle_clear_usage(JsonObject root, JsonObject response);

  // LAN scan - "who else is on my segment". Only emitter builds carry it: the
  // relay needs `ota_push` for a find to be worth anything, and a rex has
  // neither that nor the heap for a sweep.

  /// Which subnets this device can reach without a router. The app's network
  /// picker is filled from here rather than from a guess, because a device
  /// cannot scan a segment it has no address on.
  void build_lan_networks(JsonObject root);
  void handle_lan_scan_start(JsonObject root, JsonObject response);
  void handle_lan_scan_stop(JsonObject root, JsonObject response);
  /// The running (or last) sweep, results included. Polled rather than pushed:
  /// a sweep outlives any command timeout the cloud is willing to hold open.
  void build_lan_scan(JsonObject root);
  void handle_lan_target_command(JsonObject root, JsonObject response);
  void build_lan_action(JsonObject root);

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
  bool emit_firmware_event_(const char *phase, const std::string &role, const std::string &target_serial,
                            const std::string &target_ip, const std::string &version, const std::string &file_id,
                            const std::string &release_id, uint32_t file_size, uint32_t bytes_sent,
                            const char *error = nullptr, const char *trigger = "manual");

  CallbackManager<void(IdentifyRequest)> identify_callback_{};
  std::string firmware_version_{};
  std::function<void(JsonObject)> glink_diagnostics_provider_{};
  std::function<bool(const char *, JsonObject)> firmware_event_emitter_{};
};

class IdentifyTrigger : public Trigger<IdentifyRequest> {
 public:
  explicit IdentifyTrigger(ApiCoreV1 *parent) {
    parent->add_on_identify_callback([this](IdentifyRequest request) { this->trigger(request); });
  }
};

}  // namespace api_core_v1
}  // namespace esphome
