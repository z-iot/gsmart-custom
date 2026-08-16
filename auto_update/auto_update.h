#pragma once

#ifdef USE_ESP32

#include "esphome/components/api_core_v1/api_core_v1.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include <string>
#include <vector>

namespace esphome {
namespace auto_update {

/// Persisted across reboots, including the reboot a self update causes.
///
/// Written to flash *before* the download starts, which is what makes a device
/// that dies mid-flash stay quiet until tomorrow instead of walking into the
/// same failure every few minutes for the rest of the night.
struct AutoUpdateState {
  /// YYYYMMDD of the last attempt. One attempt per device per day, whatever the
  /// outcome.
  uint32_t last_attempt_day;
  /// Phase at the moment of the last write: 0 idle, 1 self flash in flight.
  uint8_t phase;
  uint8_t reserved[3];
  /// The version the in-flight self flash was installing, so the next boot can
  /// tell "it worked" from "it came back running the old image".
  char pending_version[24];
  char pending_file_id[28];
  char pending_release_id[28];
  uint32_t pending_file_size;
} __attribute__((packed));

enum AutoUpdatePhase : uint8_t {
  AUTO_UPDATE_PHASE_IDLE = 0,
  AUTO_UPDATE_PHASE_SELF_FLASHING = 1,
};

/**
 * The nightly self-service firmware update.
 *
 * Every device picks one moment inside the configured window - derived from its
 * own MAC, so it is stable across reboots and different from its neighbours' -
 * asks the board what it should be running, and installs it. A thousand devices
 * therefore arrive spread across two hours instead of all at 01:00.
 *
 * A region master also reports what it can see on the LAN and relays whatever
 * the board sends back for those members, before touching its own image: a self
 * update reboots, and anything not relayed by then would wait another day.
 */
class AutoUpdateComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_api_core(api_core_v1::ApiCoreV1 *core) { this->core_ = core; }
  void set_time(time::RealTimeClock *time) { this->time_ = time; }
  void set_board_url(const std::string &url) { this->board_url_ = url; }
  void set_promoss_secret(const std::string &secret) { this->promoss_secret_ = secret; }
  void set_channel(const std::string &channel) { this->channel_ = channel; }
  void set_window(uint8_t start_hour, uint8_t end_hour) {
    this->window_start_hour_ = start_hour;
    this->window_end_hour_ = end_hour;
  }
  void set_enabled(bool enabled) { this->enabled_ = enabled; }
  void set_timeout_ms(uint16_t timeout_ms) { this->timeout_ms_ = timeout_ms; }

  /// Seconds after midnight this device checks at. Deterministic in the MAC, so
  /// it survives a reboot and does not re-roll into a second attempt.
  uint32_t slot_seconds() const;
  /// Runs the whole check now, ignoring the window and the day guard. Wired to a
  /// button so a night's behaviour can be reproduced on the bench.
  bool run_now();

 protected:
  bool run_check_(bool respect_day_guard);
  bool fetch_plan_(const std::string &request_body, std::string &response_body);
  void apply_plan_(JsonObject root);
  void report_boot_verdict_();
  void mark_attempt_(uint32_t day);
  void save_state_();
  std::string build_request_body_(const std::string &nonce) const;
  void collect_members_(JsonArray members) const;
  std::string device_mac_() const;
  std::string device_serial_() const;
  std::string device_model_() const;
  std::string derived_device_secret_() const;
  std::string hmac_sha256_hex_(const std::string &key, const std::string &message) const;
  std::string random_nonce_() const;
  static uint32_t day_number_(const ESPTime &now);

  api_core_v1::ApiCoreV1 *core_{nullptr};
  time::RealTimeClock *time_{nullptr};
  std::string board_url_{};
  std::string promoss_secret_{};
  std::string channel_{"stable"};
  uint8_t window_start_hour_{1};
  uint8_t window_end_hour_{3};
  uint16_t timeout_ms_{15000};
  bool enabled_{true};

  AutoUpdateState state_{};
  ESPPreferenceObject pref_{};
  uint32_t last_tick_ms_{0};
  /// Set when the previous boot left a self flash in flight. Held until the
  /// cloud link accepts the verdict, so an update that finished while the link
  /// was down is still reported once it comes back.
  bool boot_verdict_pending_{false};
  bool boot_verdict_success_{false};
  std::string boot_verdict_version_{};
  std::string boot_verdict_file_id_{};
  std::string boot_verdict_release_id_{};
  uint32_t boot_verdict_file_size_{0};
  uint32_t boot_verdict_next_try_ms_{0};
  std::string last_result_{"never ran"};
};

extern AutoUpdateComponent *global_auto_update;

}  // namespace auto_update
}  // namespace esphome

#endif  // USE_ESP32
