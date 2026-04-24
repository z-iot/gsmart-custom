#include "sbus.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"
#include "esphome/core/util.h"

namespace esphome {
namespace sbus {

static const char *const TAG = "sbus";


void Sbus::setup() {
  this->set_interval("heartbeat", 15000, [this] { this->send_empty_command_(SbusCommandType::HEARTBEAT); });
}

void Sbus::loop() {
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);
    this->handle_char_(c);
  }
  process_command_queue_();
}

void Sbus::dump_config() {
  ESP_LOGCONFIG(TAG, "Sbus:");
  if (this->state_ != SbusState::SBUS_STATE_NORMAL) {
    ESP_LOGCONFIG(TAG, "  Configuration will be reported when setup is complete. Current state: %u",
                  static_cast<uint8_t>(this->state_));
    ESP_LOGCONFIG(TAG, "  If no further output is received, confirm that this is a supported Sbus device.");
    return;
  }
  for (auto &info : this->datapoints_) {
    if (info.type == SbusDatapointType::RAW) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: raw (value: %s)", info.id, format_hex_pretty(info.value_raw).c_str());
    } else if (info.type == SbusDatapointType::BOOLEAN) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: switch (value: %s)", info.id, ONOFF(info.value_bool));
    } else if (info.type == SbusDatapointType::INTEGER) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: int value (value: %d)", info.id, info.value_int);
    } else if (info.type == SbusDatapointType::STRING) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: string value (value: %s)", info.id, info.value_string.c_str());
    } else {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: unknown", info.id);
    }
  }
  this->check_uart_settings(115200);
}

bool Sbus::validate_message_() {
  uint32_t at = this->rx_message_.size() - 1;
  auto *data = &this->rx_message_[0];
  uint8_t new_byte = data[at];

  // Byte 0: HEADER1 (always 0xFF)
  if (at == 0)
    return new_byte == 0xFF;
  // Byte 1: HEADER2 (always 0xF1) (mozno ver 2: F2...)
  if (at == 1)
    return new_byte == 0xF1;

  // Byte 2: COMMAND
  uint8_t command = data[2];
  if (at == 2)
    return true;

  // Byte 3: LENGTH1
  // Byte 4: LENGTH2
  if (at <= 4) {
    // no validation for these fields
    return true;
  }

  uint16_t length = (uint16_t(data[3]) << 8) | (uint16_t(data[4]));

  // wait until all data is read
  if (at - 5 < length)
    return true;

  // Byte 5+LEN: CHECKSUM - sum of all bytes (including header) modulo 256
  uint8_t rx_checksum = new_byte;
  uint8_t calc_checksum = 0;
  for (uint32_t i = 0; i < 5 + length; i++)
    calc_checksum += data[i];

  if (rx_checksum != calc_checksum) {
    ESP_LOGW(TAG, "Sbus Received invalid message checksum %02X!=%02X", rx_checksum, calc_checksum);
    return false;
  }

  // valid message
  const uint8_t *message_data = data + 5;
  ESP_LOGV(TAG, "Received Sbus: CMD=0x%02X DATA=[%s] STATE=%u", command, 
           format_hex_pretty(message_data, length).c_str(), static_cast<uint8_t>(this->state_));
  this->handle_command_(command, message_data, length);

  // return false to reset rx buffer
  return false;
}

void Sbus::handle_char_(uint8_t c) {
  this->rx_message_.push_back(c);
  if (!this->validate_message_()) {
    this->rx_message_.clear();
  } else {
    this->last_rx_char_timestamp_ = millis();
  }
}

void Sbus::handle_command_(uint8_t command, const uint8_t *buffer, size_t len) {
  SbusCommandType command_type = (SbusCommandType) command;

  switch (command_type) {
    case SbusCommandType::HEARTBEAT:
      ESP_LOGV(TAG, "MCU Heartbeat (0x%02X)", buffer[0]);
      if (buffer[0] == 0) {
        ESP_LOGI(TAG, "MCU restarted");
        this->state_ = SbusState::SBUS_STATE_NORMAL;
        this->set_timeout("datapoint_dump", 1000, [this] { this->dump_config(); });
        this->initialized_callback_.call();
      }
      break;
    case SbusCommandType::DATAPOINT_DELIVER:
      break;
    case SbusCommandType::DATAPOINT_REPORT:
      this->handle_datapoints_(buffer, len);
      break;
    case SbusCommandType::DATAPOINT_QUERY:
      break;
    default:
      ESP_LOGE(TAG, "Invalid command (0x%02X) received", command);
  }
}

void Sbus::handle_datapoints_(const uint8_t *buffer, size_t len) {
  if (len >= 2) {
    SbusDatapoint datapoint{};
    datapoint.id = buffer[0];
    datapoint.type = (SbusDatapointType) buffer[1];
    datapoint.value_uint = 0;

    const uint8_t *data = buffer + 2;
    size_t data_len = len - 2;

    datapoint.len = data_len;

    switch (datapoint.type) {
      case SbusDatapointType::RAW:
        datapoint.value_raw = std::vector<uint8_t>(data, data + data_len);
        ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, format_hex_pretty(datapoint.value_raw).c_str());
        break;
      case SbusDatapointType::BOOLEAN:
        if (data_len != 1) {
          ESP_LOGW(TAG, "Datapoint %u has bad boolean len %zu", datapoint.id, data_len);
          return;
        }
        datapoint.value_bool = data[0];
        ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, ONOFF(datapoint.value_bool));
        break;
      case SbusDatapointType::INTEGER:
        if (data_len != 4) {
          ESP_LOGW(TAG, "Datapoint %u has bad integer len %zu", datapoint.id, data_len);
          return;
        }
        datapoint.value_uint = encode_uint32(data[0], data[1], data[2], data[3]);
        ESP_LOGD(TAG, "Datapoint %u update to %d", datapoint.id, datapoint.value_int);
        break;
      case SbusDatapointType::STRING:
        datapoint.value_string = std::string(reinterpret_cast<const char *>(data), data_len);
        ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, datapoint.value_string.c_str());
        break;
      default:
        ESP_LOGW(TAG, "Datapoint %u has unknown type %#02hhX", datapoint.id, static_cast<uint8_t>(datapoint.type));
        return;
    }

    // Update internal datapoints
    bool found = false;
    for (auto &other : this->datapoints_) {
      if (other.id == datapoint.id) {
        other = datapoint;
        found = true;
      }
    }
    if (!found) {
      this->datapoints_.push_back(datapoint);
    }

    // Run through listeners
    for (auto &listener : this->listeners_) {
      if (listener.datapoint_id == datapoint.id)
        listener.on_datapoint(datapoint);
    }
  }
}

void Sbus::send_raw_command_(SbusCommand command) {
  uint8_t len_hi = (uint8_t)(command.payload.size() >> 8);
  uint8_t len_lo = (uint8_t)(command.payload.size() & 0xFF);

  this->last_command_timestamp_ = millis();

  ESP_LOGV(TAG, "Sending Sbus: CMD=0x%02X DATA=[%s] STATE=%u", static_cast<uint8_t>(command.cmd),
           format_hex_pretty(command.payload).c_str(), static_cast<uint8_t>(this->state_));

  this->write_array({0xFF, 0xF1, (uint8_t) command.cmd, len_hi, len_lo});
  if (!command.payload.empty())
    this->write_array(command.payload.data(), command.payload.size());

  uint8_t checksum = 0xFF + 0xF1 + (uint8_t) command.cmd + len_hi + len_lo;
  for (auto &data : command.payload)
    checksum += data;
  this->write_byte(checksum);
}

void Sbus::process_command_queue_() {
  uint32_t now = millis();
  uint32_t delay = now - this->last_command_timestamp_;

  if (now - this->last_rx_char_timestamp_ > RECEIVE_TIMEOUT) {
    this->rx_message_.clear();
  }

    // Left check of delay since last command in case there's ever a command sent by calling send_raw_command_ directly
  if (delay > COMMAND_DELAY && !this->command_queue_.empty() && this->rx_message_.empty()) {
    this->send_raw_command_(command_queue_.front());
    this->command_queue_.erase(command_queue_.begin());
  }
}

void Sbus::send_command_(const SbusCommand &command) {
  command_queue_.push_back(command);
  process_command_queue_();
}

void Sbus::send_empty_command_(SbusCommandType command) {
  send_command_(SbusCommand{.cmd = command, .payload = std::vector<uint8_t>{}});
}

void Sbus::set_raw_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value) {
  this->set_raw_datapoint_value_(datapoint_id, value, false);
}

void Sbus::set_boolean_datapoint_value(uint8_t datapoint_id, bool value) {
  this->set_numeric_datapoint_value_(datapoint_id, SbusDatapointType::BOOLEAN, value, 1, false);
}

void Sbus::set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value) {
  this->set_numeric_datapoint_value_(datapoint_id, SbusDatapointType::INTEGER, value, 4, false);
}

void Sbus::set_string_datapoint_value(uint8_t datapoint_id, const std::string &value) {
  this->set_string_datapoint_value_(datapoint_id, value, false);
}

optional<SbusDatapoint> Sbus::get_datapoint_(uint8_t datapoint_id) {
  for (auto &datapoint : this->datapoints_) {
    if (datapoint.id == datapoint_id)
      return datapoint;
  }
  return {};
}

void Sbus::set_numeric_datapoint_value_(uint8_t datapoint_id, SbusDatapointType datapoint_type, const uint32_t value,
                                        uint8_t length, bool forced) {
  ESP_LOGD(TAG, "Setting datapoint %u to %u", datapoint_id, value);
  // optional<SbusDatapoint> datapoint = this->get_datapoint_(datapoint_id);
  // if (!datapoint.has_value()) {
  //   ESP_LOGW(TAG, "Setting unknown datapoint %u", datapoint_id);
  // } else if (datapoint->type != datapoint_type) {
  //   ESP_LOGE(TAG, "Attempt to set datapoint %u with incorrect type", datapoint_id);
  //   return;
  // } else if (!forced && datapoint->value_uint == value) {
  //   ESP_LOGV(TAG, "Not sending unchanged value");
  //   return;
  // }

  std::vector<uint8_t> data;
  switch (length) {
    case 4:
      data.push_back(value >> 24);
      data.push_back(value >> 16);
    case 2:
      data.push_back(value >> 8);
    case 1:
      data.push_back(value >> 0);
      break;
    default:
      ESP_LOGE(TAG, "Unexpected datapoint length %u", length);
      return;
  }
  this->send_datapoint_command_(datapoint_id, datapoint_type, data);
}

void Sbus::set_raw_datapoint_value_(uint8_t datapoint_id, const std::vector<uint8_t> &value, bool forced) {
  ESP_LOGD(TAG, "Setting datapoint %u to %s", datapoint_id, format_hex_pretty(value).c_str());
  // optional<SbusDatapoint> datapoint = this->get_datapoint_(datapoint_id);
  // if (!datapoint.has_value()) {
  //   ESP_LOGW(TAG, "Setting unknown datapoint %u", datapoint_id);
  // } else if (datapoint->type != SbusDatapointType::RAW) {
  //   ESP_LOGE(TAG, "Attempt to set datapoint %u with incorrect type", datapoint_id);
  //   return;
  // } else if (!forced && datapoint->value_raw == value) {
  //   ESP_LOGV(TAG, "Not sending unchanged value");
  //   return;
  // }
  this->send_datapoint_command_(datapoint_id, SbusDatapointType::RAW, value);
}

void Sbus::set_string_datapoint_value_(uint8_t datapoint_id, const std::string &value, bool forced) {
  ESP_LOGD(TAG, "Setting datapoint %u to %s", datapoint_id, value.c_str());
  // optional<SbusDatapoint> datapoint = this->get_datapoint_(datapoint_id);
  // if (!datapoint.has_value()) {
  //   ESP_LOGW(TAG, "Setting unknown datapoint %u", datapoint_id);
  // } else if (datapoint->type != SbusDatapointType::STRING) {
  //   ESP_LOGE(TAG, "Attempt to set datapoint %u with incorrect type", datapoint_id);
  //   return;
  // } else if (!forced && datapoint->value_string == value) {
  //   ESP_LOGV(TAG, "Not sending unchanged value");
  //   return;
  // }
  std::vector<uint8_t> data;
  for (char const &c : value) {
    data.push_back(c);
  }
  this->send_datapoint_command_(datapoint_id, SbusDatapointType::STRING, data);
}

void Sbus::send_datapoint_command_(uint8_t datapoint_id, SbusDatapointType datapoint_type, std::vector<uint8_t> data) {
  std::vector<uint8_t> buffer;
  buffer.push_back(datapoint_id);
  buffer.push_back(static_cast<uint8_t>(datapoint_type));
  buffer.insert(buffer.end(), data.begin(), data.end());

  this->send_command_(SbusCommand{.cmd = SbusCommandType::DATAPOINT_DELIVER, .payload = buffer});
}

void Sbus::query_datapoint(uint8_t datapoint_id){
  std::vector<uint8_t> buffer;
  buffer.push_back(datapoint_id);

  this->send_command_(SbusCommand{.cmd = SbusCommandType::DATAPOINT_QUERY, .payload = buffer});
}

void Sbus::register_listener(uint8_t datapoint_id, const std::function<void(SbusDatapoint)> &func) {
  auto listener = SbusDatapointListener{
      .datapoint_id = datapoint_id,
      .on_datapoint = func,
  };
  this->listeners_.push_back(listener);

  // Run through existing datapoints
  for (auto &datapoint : this->datapoints_) {
    if (datapoint.id == datapoint_id)
      func(datapoint);
  }
}

SbusState Sbus::get_state() { return this->state_; }

}  // namespace sbus
}  // namespace esphome
