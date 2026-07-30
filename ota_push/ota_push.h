#pragma once

#ifdef USE_ESP32

#include "esphome/components/http_update/http_update.h"
#include "esphome/components/md5/md5.h"
#include "esphome/components/sha256/sha256.h"
#include "esphome/core/component.h"
#include <WiFiClient.h>
#include <functional>
#include <string>

namespace esphome {
namespace ota_push {

struct OtaPushRequest {
  std::string url;
  std::string target_ip;
  uint16_t target_port{3232};
  std::string md5;
  std::string ota_password;
};

class OtaPushBackend : public http_update::OTABackend {
 public:
  void configure(const OtaPushRequest &request);
  const std::string &last_error() const { return this->last_error_; }

  http_update::OTAResponseTypes begin(size_t image_size) override;
  void set_update_md5(const char *md5) override;
  http_update::OTAResponseTypes write(uint8_t *data, size_t len) override;
  http_update::OTAResponseTypes end() override;
  void abort() override;
  bool supports_compression() override { return false; }

 protected:
  bool readall_(uint8_t *buf, size_t len, uint32_t timeout_ms = 5000);
  bool writeall_(const uint8_t *buf, size_t len, uint32_t timeout_ms = 5000);
  bool expect_(uint8_t expected, const char *stage, uint32_t timeout_ms = 5000);
  bool authenticate_(uint8_t *buf);
  bool authenticate_sha256_();
  bool drain_chunk_acks_(bool final);
  void set_error_(const char *error);

  WiFiClient client_{};
  OtaPushRequest request_{};
  std::string bin_md5_{};
  /// Whichever protocol the target acked with, 1 or 2. Decides the auth digest
  /// and whether the data stream is acknowledged block by block.
  uint8_t protocol_version_{1};
  size_t written_{0};
  size_t acked_{0};
  std::string last_error_{};
  bool started_{false};
};

class OtaPushComponent : public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_ota_password(const std::string &password) { this->ota_password_ = password; }
  const std::string &ota_password() const { return this->ota_password_; }

  bool push_url(const OtaPushRequest &request, std::function<void(size_t, size_t)> progress);
  const std::string &last_error() const { return this->last_error_; }

  /// True while a delegated OTA is streaming. Other components use it to stay off
  /// the air: the relay already holds a TLS download and the push socket open, and
  /// anything else talking at the same time competes for the same memory.
  bool busy() const { return this->busy_; }

 protected:
  std::string ota_password_{"promoss1"};
  std::string last_error_{};
  bool busy_{false};
};

extern OtaPushComponent *global_ota_push;

}  // namespace ota_push
}  // namespace esphome

#endif  // USE_ESP32
