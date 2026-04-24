#pragma once
// #include "ota_component.h"
#include "esphome/components/socket/socket.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace http_update {

enum OTAResponseTypes {
  OTA_RESPONSE_OK = 0,
  OTA_RESPONSE_REQUEST_AUTH = 1,

  OTA_RESPONSE_HEADER_OK = 64,
  OTA_RESPONSE_AUTH_OK = 65,
  OTA_RESPONSE_UPDATE_PREPARE_OK = 66,
  OTA_RESPONSE_BIN_MD5_OK = 67,
  OTA_RESPONSE_RECEIVE_OK = 68,
  OTA_RESPONSE_UPDATE_END_OK = 69,
  OTA_RESPONSE_SUPPORTS_COMPRESSION = 70,

  OTA_RESPONSE_ERROR_MAGIC = 128,
  OTA_RESPONSE_ERROR_UPDATE_PREPARE = 129,
  OTA_RESPONSE_ERROR_AUTH_INVALID = 130,
  OTA_RESPONSE_ERROR_WRITING_FLASH = 131,
  OTA_RESPONSE_ERROR_UPDATE_END = 132,
  OTA_RESPONSE_ERROR_INVALID_BOOTSTRAPPING = 133,
  OTA_RESPONSE_ERROR_WRONG_CURRENT_FLASH_CONFIG = 134,
  OTA_RESPONSE_ERROR_WRONG_NEW_FLASH_CONFIG = 135,
  OTA_RESPONSE_ERROR_ESP8266_NOT_ENOUGH_SPACE = 136,
  OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE = 137,
  OTA_RESPONSE_ERROR_NO_UPDATE_PARTITION = 138,
  OTA_RESPONSE_ERROR_UNKNOWN = 255,
};

enum OTAState { OTA_COMPLETED = 0, OTA_STARTED, OTA_IN_PROGRESS, OTA_ERROR };

class OTABackend {
 public:
  virtual ~OTABackend() = default;
  virtual OTAResponseTypes begin(size_t image_size) = 0;
  virtual void set_update_md5(const char *md5) = 0;
  virtual OTAResponseTypes write(uint8_t *data, size_t len) = 0;
  virtual OTAResponseTypes end() = 0;
  virtual void abort() = 0;
  virtual bool supports_compression() = 0;
};

}  // namespace ota
}  // namespace esphome
