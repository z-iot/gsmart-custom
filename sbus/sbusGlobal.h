#pragma once

static const int COMMAND_DELAY = 10;
static const int RECEIVE_TIMEOUT = 300;

enum class SbusDatapointKind : uint8_t
{
  DPKIND_VERSION = 1,
  DPKIND_SERIALNUM = 2,

  DPKIND_MCU = 5,

  DPKIND_EEPROM = 11,

  DPKIND_DATETIME = 21,
  DPKIND_LED,

  DPKIND_VOLTAGE_MCU = 51,
  DPKIND_VOLTAGE_BATT,
  DPKIND_VOLTAGE_SUPPLY,
  DPKIND_TEMP_MCU,

  DPKIND_FANA_PWM = 101,
  DPKIND_FANB_PWM,
  DPKIND_FANA_TACHO,
  DPKIND_FANB_TACHO,
  DPKIND_RELAYA,
  DPKIND_RELAYB,
  DPKIND_TRIACA,
  DPKIND_TRIACB,

  DPKIND_FAN_PWM = 121,
};

enum class SbusDatapointType : uint8_t
{
  RAW = 0x00,     // variable length
  BOOLEAN = 0x01, // 1 byte (0/1)
  INTEGER = 0x02, // 4 byte
  STRING = 0x03,  // variable length
  // ENUM = 0x04,    // 1 byte
  // BITMASK = 0x05, // 1/2/4 bytes
};

struct SbusDatapoint
{
  uint8_t id;
  SbusDatapointType type;
  size_t len;
  union
  {
    bool value_bool;
    int value_int;
    uint32_t value_uint;
  };
  std::string value_string;
  std::vector<uint8_t> value_raw;
};

enum class SbusCommandType : uint8_t
{
  HEARTBEAT = 0,
  DATAPOINT_DELIVER,
  DATAPOINT_REPORT,
  DATAPOINT_QUERY,
  STATUS,
  ORDER,
};

struct SbusCommand
{
  SbusCommandType cmd;
  std::vector<uint8_t> payload;
};
