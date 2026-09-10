#pragma once

#if defined(ESP32) || defined(ESP8266)

#include "esphome/core/component.h"
#include "esphome/components/api_core_v1/api_core_v1.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/storage/data_global.h"
#include "history_files.h"

#include <WebSocketsClient.h>
#include <functional>
#include <string>

namespace esphome {
namespace api_adapter_glink {

class ApiAdapterGLink : public Component {
 public:
  explicit ApiAdapterGLink(api_core_v1::ApiCoreV1 *core) : core_(core) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  void suspend_for_ota();
  void resume_after_ota_error();
  float get_setup_priority() const override { return setup_priority::LATE; }

  void set_url(const std::string &url) { this->url_ = url; }
  void set_promoss_secret(const std::string &promoss_secret) { this->promoss_secret_ = promoss_secret; }
  void set_tls_ca_cert(const std::string &tls_ca_cert) { this->tls_ca_cert_ = tls_ca_cert; }
  void set_heartbeat_interval(uint32_t heartbeat_interval_ms) { this->heartbeat_interval_ms_ = heartbeat_interval_ms; }
  void set_full_heartbeat_interval(uint32_t full_heartbeat_interval_ms) { this->full_heartbeat_interval_ms_ = full_heartbeat_interval_ms; }

 protected:
  struct ParsedUrl {
    bool secure{false};
    std::string host;
    uint16_t port{0};
    std::string path{"/"};
  };

  void connect_();
  void stop_(const char *reason);
  bool probe_gateway_();
  bool parse_url_(ParsedUrl *parsed) const;
  void on_websocket_event_(WStype_t type, uint8_t *payload, size_t length);
  void handle_text_(const uint8_t *text, size_t length);
  void handle_challenge_(JsonObject payload);
  void handle_command_(const std::string &ref_id, JsonObject payload);
  std::string handle_gnode_command_(const std::string &name, JsonObject body, JsonObject response);

  bool send_hello_();
  bool send_auth_();
  void send_heartbeat_(const char *mode);
  void send_session_event_(const char *phase, const char *reason, bool include_status);
  void send_radiation_event_(storage::RadiationMode mode, storage::RadiationSource source);
  bool send_firmware_event_(const char *phase, JsonObject body);
  bool send_frame_(const char *type, const char *peer, const std::string &id, std::function<void(JsonObject)> builder);
  void build_full_status_(JsonObject body);
  void build_diagnostics_(JsonObject root) const;
  void set_state_(const char *state);
  void set_error_(const char *error);
  void history_setup_();
  void history_poll_();
  void history_send_();
  void history_ack_(JsonObject body);
  void history_report_error_(uint16_t code, bool active);
  void history_report_errors_();
  bool history_record_(uint8_t kind, storage::RadiationMode mode, uint16_t code = 0);

  std::string device_serial_() const;
  std::string device_mac_() const;
  std::string derived_device_secret_() const;
  std::string device_model_() const;
  std::string device_display_name_() const;
  std::string next_frame_id_(const char *prefix);
  std::string random_hex_(size_t bytes) const;
  std::string hmac_sha256_hex_(const std::string &key, const std::string &input) const;

  api_core_v1::ApiCoreV1 *core_{nullptr};
  WebSocketsClient websocket_{};
  ParsedUrl parsed_{};
  std::string url_{};
  std::string promoss_secret_{};
  std::string tls_ca_cert_{};
  std::string client_nonce_{};
  std::string session_id_{};
  std::string server_nonce_{};
  std::string event_level_{"basic"};
  uint32_t heartbeat_interval_ms_{20000};
  uint32_t full_heartbeat_interval_ms_{300000};
  uint32_t last_heartbeat_ms_{0};
  uint32_t last_full_heartbeat_ms_{0};
  uint32_t event_level_expires_ms_{0};
  uint32_t next_connect_ms_{0};
  uint32_t connect_started_ms_{0};
  uint32_t frame_seq_{0};
  uint32_t reconnect_interval_ms_{60000};
  uint32_t probe_attempts_{0};
  uint32_t connect_attempts_{0};
  uint32_t last_probe_ms_{0};
  uint32_t last_connect_ms_{0};
  uint32_t last_connected_ms_{0};
  uint32_t last_rx_ms_{0};
  uint32_t last_tx_ms_{0};
  uint32_t last_auth_ms_{0};
  bool parsed_url_{false};
  bool started_{false};
  bool connected_{false};
  bool authenticated_{false};
  bool ota_suspended_{false};
  // Send outside the receive callback, after WebSockets has released its buffer.
  enum class Handshake : uint8_t { NONE, HELLO, AUTH, SESSION };
  Handshake handshake_{Handshake::NONE};
  bool last_probe_ok_{false};
  std::string state_{"init"};
  std::string last_error_{};
  std::string last_rx_type_{};
  std::string last_tx_type_{};
#ifdef GSMART_FEATURE_FILESYSTEM
  gsmart_history::FlashFiles history_files_{};
#ifdef ESP8266
  gsmart_history::Journal<3> history_{history_files_};
#elif defined(GSMART_MODEL_PANEL)
  gsmart_history::Journal<gsmart_history::PANEL_SLOTS> history_{history_files_};
#else
  gsmart_history::Journal<64> history_{history_files_};
#endif
  uint64_t history_uptime_{0}, history_sent_boot_{0}, history_region_{0};
  uint64_t history_wall_anchor_{0}, history_uptime_anchor_{0};
  uint32_t history_millis_{0}, history_poll_ms_{0}, history_send_ms_{0}, history_sent_sequence_{0};
  uint32_t history_window_ms_{0};
  uint32_t history_day_ms_{0};
  uint16_t history_day_records_{0};
  uint16_t history_send_delay_{5000};
  uint16_t history_version_{0}, history_window_records_{0};
  bool history_clock_{false}, history_boot_{false}, history_config_{false}, history_rate_gap_{false};
#endif
};

}  // namespace api_adapter_glink
}  // namespace esphome

#endif  // defined(ESP32) || defined(ESP8266)
