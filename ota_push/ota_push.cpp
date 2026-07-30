#include "ota_push.h"

#ifdef USE_ESP32

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cstdio>
#include <cstring>

namespace esphome {
namespace ota_push {

static const char *const TAG = "ota_push";
static const uint8_t OTA_MAGIC[5] = {0x6C, 0x26, 0xF7, 0x5C, 0x45};
static const uint8_t OTA_VERSION_1_0 = 1;

// ESPHome's OTA server has spoken protocol 2 for a while - every model here builds
// with USE_OTA_VERSION 2 - so demanding version 1 made the handshake fail with
// magic_ack_failed against every device, and delegated OTA never worked at all.
static const uint8_t OTA_VERSION_2_0 = 2;

// Protocol 2 replaces the MD5 challenge with SHA256 and refuses a client that does
// not advertise it, so the feature byte is no longer just "no compression".
static const uint8_t CLIENT_FEATURE_SUPPORTS_SHA256_AUTH = 0x02;

// Not in the vendored ota_backend.h, which predates protocol 2.
static const uint8_t OTA_RESPONSE_REQUEST_SHA256_AUTH = 2;

static const size_t MD5_HEX_SIZE = 32;
static const size_t SHA256_HEX_SIZE = 64;

OtaPushComponent *global_ota_push = nullptr;

void OtaPushComponent::setup() {
  global_ota_push = this;
  this->dump_config();
}

void OtaPushComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "G-Smart delegated OTA push:");
  ESP_LOGCONFIG(TAG, "  Default OTA password: %s", this->ota_password_.empty() ? "disabled" : "configured");
}

bool OtaPushComponent::push_url(const OtaPushRequest &request, std::function<void(size_t, size_t)> progress) {
  if (http_update::global_http_update == nullptr) {
    this->last_error_ = "http_update_not_ready";
    return false;
  }

  OtaPushRequest effective = request;
  if (effective.ota_password.empty())
    effective.ota_password = this->ota_password_;

  OtaPushBackend backend;
  backend.configure(effective);
  http_update::global_http_update->set_url(effective.url);
  http_update::global_http_update->set_method("GET");
  const bool ok = http_update::global_http_update->flash_to_backend(&backend, progress);
  http_update::global_http_update->close();
  this->last_error_ = ok ? "" : backend.last_error();
  return ok;
}

void OtaPushBackend::configure(const OtaPushRequest &request) {
  this->request_ = request;
  this->bin_md5_ = request.md5;
  this->last_error_.clear();
  this->started_ = false;
}

http_update::OTAResponseTypes OtaPushBackend::begin(size_t image_size) {
  uint8_t buf[96];
  this->last_error_.clear();
  this->client_.setNoDelay(true);
  this->client_.setTimeout(5);

  if (!this->client_.connect(this->request_.target_ip.c_str(), this->request_.target_port)) {
    this->set_error_("target_connect_failed");
    return http_update::OTA_RESPONSE_ERROR_UNKNOWN;
  }

  if (!this->writeall_(OTA_MAGIC, sizeof(OTA_MAGIC))) {
    this->set_error_("magic_write_failed");
    return http_update::OTA_RESPONSE_ERROR_MAGIC;
  }
  if (!this->readall_(buf, 2) || buf[0] != http_update::OTA_RESPONSE_OK ||
      (buf[1] != OTA_VERSION_1_0 && buf[1] != OTA_VERSION_2_0)) {
    this->set_error_("magic_ack_failed");
    return http_update::OTA_RESPONSE_ERROR_MAGIC;
  }
  this->protocol_version_ = buf[1];

  // No compression either way - the wire MD5 belongs to the uncompressed image.
  // Extended protocol is deliberately not requested: it would add an OTA-type byte
  // to the data phase and buys nothing here.
  buf[0] = this->protocol_version_ >= OTA_VERSION_2_0 ? CLIENT_FEATURE_SUPPORTS_SHA256_AUTH : 0x00;
  if (!this->writeall_(buf, 1)) {
    this->set_error_("features_write_failed");
    return http_update::OTA_RESPONSE_ERROR_UNKNOWN;
  }
  if (!this->readall_(buf, 1)) {
    this->set_error_("header_ack_failed");
    return http_update::OTA_RESPONSE_ERROR_UNKNOWN;
  }
  if (buf[0] != http_update::OTA_RESPONSE_HEADER_OK && buf[0] != http_update::OTA_RESPONSE_SUPPORTS_COMPRESSION) {
    this->set_error_("header_rejected");
    return static_cast<http_update::OTAResponseTypes>(buf[0]);
  }

  if (!this->readall_(buf, 1)) {
    this->set_error_("auth_state_read_failed");
    return http_update::OTA_RESPONSE_ERROR_AUTH_INVALID;
  }
  if (buf[0] == OTA_RESPONSE_REQUEST_SHA256_AUTH) {
    if (!this->authenticate_sha256_())
      return http_update::OTA_RESPONSE_ERROR_AUTH_INVALID;
    if (!this->expect_(http_update::OTA_RESPONSE_AUTH_OK, "auth_ok"))
      return http_update::OTA_RESPONSE_ERROR_AUTH_INVALID;
  } else if (buf[0] == http_update::OTA_RESPONSE_REQUEST_AUTH) {
    if (!this->authenticate_(buf))
      return http_update::OTA_RESPONSE_ERROR_AUTH_INVALID;
    if (!this->expect_(http_update::OTA_RESPONSE_AUTH_OK, "auth_ok"))
      return http_update::OTA_RESPONSE_ERROR_AUTH_INVALID;
  } else if (buf[0] != http_update::OTA_RESPONSE_AUTH_OK) {
    this->set_error_("auth_rejected");
    return static_cast<http_update::OTAResponseTypes>(buf[0]);
  }

  buf[0] = static_cast<uint8_t>((image_size >> 24) & 0xff);
  buf[1] = static_cast<uint8_t>((image_size >> 16) & 0xff);
  buf[2] = static_cast<uint8_t>((image_size >> 8) & 0xff);
  buf[3] = static_cast<uint8_t>(image_size & 0xff);
  if (!this->writeall_(buf, 4)) {
    this->set_error_("size_write_failed");
    return http_update::OTA_RESPONSE_ERROR_UPDATE_PREPARE;
  }
  if (!this->expect_(http_update::OTA_RESPONSE_UPDATE_PREPARE_OK, "prepare_ok"))
    return http_update::OTA_RESPONSE_ERROR_UPDATE_PREPARE;

  if (this->bin_md5_.length() != 32) {
    this->set_error_("missing_wire_md5");
    return http_update::OTA_RESPONSE_ERROR_UNKNOWN;
  }
  if (!this->writeall_(reinterpret_cast<const uint8_t *>(this->bin_md5_.c_str()), 32)) {
    this->set_error_("md5_write_failed");
    return http_update::OTA_RESPONSE_ERROR_UNKNOWN;
  }
  if (!this->expect_(http_update::OTA_RESPONSE_BIN_MD5_OK, "md5_ok"))
    return http_update::OTA_RESPONSE_ERROR_UNKNOWN;

  this->started_ = true;
  ESP_LOGI(TAG, "Delegated OTA stream started: %s:%u size=%u", this->request_.target_ip.c_str(), this->request_.target_port,
           static_cast<unsigned>(image_size));
  return http_update::OTA_RESPONSE_OK;
}

void OtaPushBackend::set_update_md5(const char *md5) {
  if (md5 != nullptr && strlen(md5) == 32)
    this->bin_md5_ = md5;
}

http_update::OTAResponseTypes OtaPushBackend::write(uint8_t *data, size_t len) {
  if (!this->writeall_(data, len, 10000)) {
    this->set_error_("body_write_failed");
    return http_update::OTA_RESPONSE_ERROR_WRITING_FLASH;
  }
  return http_update::OTA_RESPONSE_OK;
}

http_update::OTAResponseTypes OtaPushBackend::end() {
  if (!this->expect_(http_update::OTA_RESPONSE_RECEIVE_OK, "receive_ok", 15000))
    return http_update::OTA_RESPONSE_ERROR_WRITING_FLASH;
  if (!this->expect_(http_update::OTA_RESPONSE_UPDATE_END_OK, "update_end_ok", 20000))
    return http_update::OTA_RESPONSE_ERROR_UPDATE_END;
  uint8_t ok = http_update::OTA_RESPONSE_OK;
  this->writeall_(&ok, 1);
  this->client_.stop();
  this->started_ = false;
  ESP_LOGI(TAG, "Delegated OTA stream completed: %s:%u", this->request_.target_ip.c_str(), this->request_.target_port);
  return http_update::OTA_RESPONSE_OK;
}

void OtaPushBackend::abort() {
  if (this->client_.connected()) {
    uint8_t error = http_update::OTA_RESPONSE_ERROR_UNKNOWN;
    this->writeall_(&error, 1, 500);
  }
  this->client_.stop();
  this->started_ = false;
}

bool OtaPushBackend::authenticate_sha256_() {
  // Protocol 2 challenge: the device sent auth_type followed by a 64-char hex nonce.
  // We answer with our own hex cnonce and SHA256(password || nonce || cnonce), the
  // same digest ESPHomeOTAComponent::handle_auth_read_ recomputes to compare.
  char nonce[SHA256_HEX_SIZE + 1];
  if (!this->readall_(reinterpret_cast<uint8_t *>(nonce), SHA256_HEX_SIZE)) {
    this->set_error_("auth_nonce_read_failed");
    return false;
  }
  nonce[SHA256_HEX_SIZE] = '\0';

  char cnonce[SHA256_HEX_SIZE + 1];
  char response[SHA256_HEX_SIZE + 1];
  {
    // Both hashes are built and consumed inside this scope on purpose: the ESP32-S3
    // SHA accelerator corrupts results when a hasher crosses a stack frame, which is
    // why the ESPHome side documents the same rule.
    char seed[40];
    snprintf(seed, sizeof(seed), "%08X%08X%08X", random_uint32(), random_uint32(), millis());
    sha256::SHA256 hasher;
    hasher.init();
    hasher.add(reinterpret_cast<const uint8_t *>(seed), strlen(seed));
    hasher.calculate();
    hasher.get_hex(cnonce);
    cnonce[SHA256_HEX_SIZE] = '\0';
  }
  {
    sha256::SHA256 hasher;
    hasher.init();
    hasher.add(reinterpret_cast<const uint8_t *>(this->request_.ota_password.c_str()),
               this->request_.ota_password.length());
    hasher.add(reinterpret_cast<const uint8_t *>(nonce), SHA256_HEX_SIZE);
    hasher.add(reinterpret_cast<const uint8_t *>(cnonce), SHA256_HEX_SIZE);
    hasher.calculate();
    hasher.get_hex(response);
    response[SHA256_HEX_SIZE] = '\0';
  }

  if (!this->writeall_(reinterpret_cast<uint8_t *>(cnonce), SHA256_HEX_SIZE)) {
    this->set_error_("auth_cnonce_write_failed");
    return false;
  }
  if (!this->writeall_(reinterpret_cast<uint8_t *>(response), SHA256_HEX_SIZE)) {
    this->set_error_("auth_response_write_failed");
    return false;
  }
  return true;
}

bool OtaPushBackend::authenticate_(uint8_t *buf) {
  if (!this->readall_(buf, 32)) {
    this->set_error_("auth_nonce_read_failed");
    return false;
  }
  char nonce[33];
  memcpy(nonce, buf, 32);
  nonce[32] = 0;

  char cnonce[33];
  char seed[32];
  snprintf(seed, sizeof(seed), "%08X%08X", random_uint32(), millis());
  md5::MD5Digest md5;
  md5.init();
  md5.add(seed, strlen(seed));
  md5.calculate();
  md5.get_hex(cnonce);

  md5.init();
  md5.add(this->request_.ota_password.c_str(), this->request_.ota_password.length());
  md5.add(nonce, 32);
  md5.add(cnonce, 32);
  md5.calculate();
  char result[33];
  md5.get_hex(result);

  if (!this->writeall_(reinterpret_cast<uint8_t *>(cnonce), 32)) {
    this->set_error_("auth_cnonce_write_failed");
    return false;
  }
  if (!this->writeall_(reinterpret_cast<uint8_t *>(result), 32)) {
    this->set_error_("auth_response_write_failed");
    return false;
  }
  return true;
}

bool OtaPushBackend::expect_(uint8_t expected, const char *stage, uint32_t timeout_ms) {
  uint8_t value = 0;
  if (!this->readall_(&value, 1, timeout_ms)) {
    this->set_error_(stage);
    return false;
  }
  if (value != expected) {
    this->last_error_ = std::string(stage) + "_unexpected_" + str_sprintf("%u", value);
    return false;
  }
  return true;
}

bool OtaPushBackend::readall_(uint8_t *buf, size_t len, uint32_t timeout_ms) {
  uint32_t start = millis();
  size_t at = 0;
  while (len - at > 0) {
    if (millis() - start > timeout_ms)
      return false;
    int read = this->client_.read(buf + at, len - at);
    if (read < 0)
      return false;
    if (read == 0) {
      App.feed_wdt();
      delay(1);
      continue;
    }
    at += static_cast<size_t>(read);
    App.feed_wdt();
    delay(1);
  }
  return true;
}

bool OtaPushBackend::writeall_(const uint8_t *buf, size_t len, uint32_t timeout_ms) {
  uint32_t start = millis();
  size_t at = 0;
  while (len - at > 0) {
    if (millis() - start > timeout_ms)
      return false;
    size_t written = this->client_.write(buf + at, len - at);
    if (written == 0) {
      App.feed_wdt();
      delay(1);
      continue;
    }
    at += written;
    App.feed_wdt();
    delay(1);
  }
  return true;
}

void OtaPushBackend::set_error_(const char *error) { this->last_error_ = error != nullptr ? error : "ota_push_failed"; }

}  // namespace ota_push
}  // namespace esphome

#endif  // USE_ESP32
