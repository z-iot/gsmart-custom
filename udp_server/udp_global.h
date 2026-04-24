#pragma once

#include "esphome/components/json/json_util.h"
#include "esphome/components/storage/global.h"

#define MESSAGE_SYSINFO_REPEAT_SEC 3600*3
#define MESSAGE_STATUSINFO_REPEAT_SEC 60
#define MESSAGE_IDENTITYINFO_REPEAT_SEC 5*60

#define UDP_PROTOCOL_VERSION 1
#define DEVICE_MODEL_UNKNOWN 0

namespace esphome
{
  namespace udp_server
  {

    // - control (mode=min/std/max/off source=actuator/emitter/scheduller)
    // - status (event=interval/radiation/lamp/motion radiation=min/std/max/off radiationSource=unknown/ext/actuator/emitter/scheduller/regionMaster lamp=on-start/on/off-start/off motion=on/off motionSource=unknown/external/radar/pir/door))
    // - identity (identification=serial/region/uptime..., consuption=..., error=...)
    // - reconfig (regionChannel=, regionDevices=)
    // - regionStatus

    static const char *const TAG = "UdpServer";

    static uint8_t const UdpPacket_Mark[2] PROGMEM = {0x55, 0x75};

    std::string macToStr(const uint8_t mac[6]);
    std::string ipToStr(const uint8_t ip[4]);

    union ui32_to_ui8
    {
      uint32_t ui32;
      uint8_t ui8[4];
    };

    enum class PacketKind
    {
      UNKNOWN = 0,
      SYS_INFO = 11,
      
      PING_REQ = 21,
      PING_RES = 22,
      
      CONTROL = 51,
      STATUS = 52,
      IDENTITY = 53,
      RECONFIG = 54,
    };

    std::string packetKindToStr(PacketKind item);

    std::string radiationModeToStr(storage::RadiationMode item);

    enum KindRadiationSource
    {
      SOURCE_UNKNOWN = 0,
      SOURCE_EXT = 11,
      ACTUATOR = 21,
      EMITTER = 22,
      SCHEDULLER = 23,
      // REGION_MASTER = 31,
    };

    std::string kindRadiationSourceToStr(KindRadiationSource item);

    enum KindStatusEvent
    {
      INTERVAL = 11,
      RADIATION = 21,
      LAMP = 22,
      MOTION = 23,
    };

    std::string kindStatusEventToStr(KindStatusEvent item);

    enum KindLampMode
    {
      ON_START = 21,
      ON = 1,
      OFF_START = 22,
      OFF = 2,
    };

    std::string kindLampModeToStr(KindLampMode item);

    enum KindMotionSource
    {
      UNKNOWN = 0,
      EXT = 11,
      RADAR = 21,
      PIR = 22,
      DOOR = 23,
    };

    std::string kindMotionSourceToStr(KindMotionSource item);

    struct PacketHeader
    {
      uint8_t mark[2];
      uint8_t protocol_ver;
      PacketKind packetKind;
    };

    struct PacketSysInfo
    {
      uint8_t mac[6];
      uint8_t ip[4];
      uint8_t channel;
      uint8_t model;
      uint8_t build[2];
      uint32_t time;
      char name[25];
    };

    struct PacketUdpServer
    {
      PacketHeader header;
      std::vector<uint8_t> body;
    };

    struct PacketControl
    {
      uint8_t mac[6];
      storage::RadiationMode mode;
      KindRadiationSource source;
    };

    std::string packetControlToJsonStr(PacketControl packet);

    struct PacketStatus
    {
      uint8_t mac[6];
      KindStatusEvent event;
      storage::RadiationMode radiation;
      KindRadiationSource radiationSource;
      KindLampMode lamp;
      bool motion;
      KindMotionSource motionSource;
    };

    std::string packetStatusToJsonStr(PacketStatus packet);

    struct ConsumableLamp
    {
      uint16_t hours_max;
      uint16_t last_day_change;
      uint16_t start_count;
      uint32_t usage_sec;
    };

    struct PacketIdentity
    {
      uint8_t mac[6];
      uint8_t ip[4];
      uint8_t model;
      // uint8_t build[2];
      uint32_t time;
      char name[25];
      uint8_t consumable_lamp_count;
      ConsumableLamp consumable[3];
      uint32_t cleaned_total;
      uint32_t cleaned_today;
      uint32_t cleaned_yesterday;
      uint16_t total_start_count;
      uint32_t total_usage_sec;
      uint16_t current_day;
    };

    std::string packetIdentityToJsonStr(PacketIdentity packet);

    struct PacketReconfig
    {
      uint8_t mac[6];
    };

    struct PacketPing
    {
      uint8_t mac[6];
    };
  }
}
