#pragma once

#ifdef ESP32

#include "esphome/core/component.h"
#include "esphome/components/api_core_v1/api_core_v1.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/storage/data_global.h"

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

  void set_url(const std::string &url) { this->url_ = url; }
  void set_key_id(const std::string &key_id) { this->key_id_ = key_id; }
  void set_secret(const std::string &secret) { this->secret_ = secret; }
  void set_heartbeat_interval(uint32_t heartbeat_interval_ms) { this->heartbeat_interval_ms_ = heartbeat_interval_ms; }

 protected:
  struct ParsedUrl {
    bool secure{false};
    std::string host;
    uint16_t port{0};
    std::string path{"/"};
  };

  void connect_();
  bool parse_url_(ParsedUrl *parsed) const;
  void on_websocket_event_(WStype_t type, uint8_t *payload, size_t length);
  void handle_text_(const std::string &text);
  void handle_challenge_(JsonObject payload);
  void handle_command_(const std::string &ref_id, JsonObject payload);
  std::string handle_gnode_command_(const std::string &name, JsonObject body, JsonObject response);

  void send_hello_();
  void send_auth_();
  void send_heartbeat_();
  void send_ack_(const std::string &ref_id, const std::string &command_id, const char *status, const std::string &error = "");
  void send_response_(const std::string &ref_id, const std::string &command_id, const char *status, JsonObject body,
                      const std::string &error = "");
  void send_radiation_event_(storage::RadiationMode mode, storage::RadiationSource source);
  bool send_frame_(const char *type, const char *peer, const std::string &id, std::function<void(JsonObject)> builder);

  std::string device_serial_() const;
  std::string device_mac_() const;
  std::string device_model_() const;
  std::string next_frame_id_(const char *prefix);
  std::string random_hex_(size_t bytes) const;
  std::string hmac_sha256_hex_(const std::string &input) const;

  api_core_v1::ApiCoreV1 *core_{nullptr};
  WebSocketsClient websocket_{};
  std::string url_{};
  std::string key_id_{};
  std::string secret_{};
  std::string client_nonce_{};
  std::string session_id_{};
  std::string server_nonce_{};
  uint32_t heartbeat_interval_ms_{20000};
  uint32_t last_heartbeat_ms_{0};
  uint32_t frame_seq_{0};
  bool started_{false};
  bool connected_{false};
  bool authenticated_{false};
};

}  // namespace api_adapter_glink
}  // namespace esphome

#endif  // ESP32
