#pragma once

#include "esphome/components/json/json_util.h"
#include "esphome/components/storage/global.h"
#include "esphome/components/storage/data_global.h"
#include "esphome/components/storage/data_region.h"

#define MESSAGE_SYSINFO_REPEAT_SEC 3600*3
#define MESSAGE_STATUSINFO_REPEAT_SEC 60
#define MESSAGE_IDENTITYINFO_REPEAT_SEC 5*60

#define UDP_PROTOCOL_VERSION 2
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
      MANAGEMENT = 55,
      SITUATION = 56,
      REGION_LAYOUT = 57,
      REGION_INTENT = 58,
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
      SWITCH = 24,
      REGION_MASTER = 31,
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
      uint64_t region_id;
      uint8_t ip[4];
      uint16_t channel;
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
      uint64_t region_id;
      storage::RadiationMode mode;
      KindRadiationSource source;
    };

    std::string packetControlToJsonStr(PacketControl packet);

    struct PacketStatus
    {
      uint8_t mac[6];
      uint64_t region_id;
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
      uint64_t region_id;
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
      uint64_t region_id;
    };

    struct PacketSituation
    {
      uint8_t mac[6];
      uint64_t region_id;
      KindRadiationSource source;
      storage::RadiationMode active_mode;
      bool scheduler_active;
      uint16_t scheduler_items_count;
      bool current_is_active;
      bool current_is_schedule;
      bool current_is_external;
      storage::RadiationMode current_mode;
      uint32_t current_begin_time;
      uint32_t current_end_time;
      uint16_t current_beamed_sec;
      uint16_t current_total_sec;
      storage::RadiationMode prev_mode;
      uint32_t prev_begin_time;
      uint32_t prev_end_time;
      uint16_t prev_beamed_sec;
      uint16_t prev_total_sec;
      storage::RadiationMode schedule_mode;
      uint32_t schedule_begin_time;
      uint32_t schedule_end_time;
      uint16_t schedule_total_sec;
      bool schedule_is_aborted;
      storage::RadiationMode next_mode;
      uint32_t next_begin_time;
      uint32_t next_end_time;
      uint16_t next_total_sec;
    };

    std::string packetSituationToJsonStr(PacketSituation packet);

    enum class RegionLayoutAction : uint8_t
    {
      REQUEST = 1,
      PUSH = 2,
      RESPONSE = 3,
    };

    std::string regionLayoutActionToStr(RegionLayoutAction item);

    struct PacketRegionLayout
    {
      uint8_t mac[6];
      uint64_t region_id;
      RegionLayoutAction action;
      uint16_t udp_channel;
      uint32_t config_version;
      uint8_t master_index;
      uint8_t member_count;
      storage::RegionMember members[16];
    };

    std::string packetRegionLayoutToJsonStr(PacketRegionLayout packet);

    struct PacketRegionIntent
    {
      uint8_t origin_mac[6];
      uint64_t region_id;
      uint32_t sequence;
      storage::RadiationMode mode;
      KindRadiationSource source;
      storage::RadiationCause cause;
    };

    std::string packetRegionIntentToJsonStr(PacketRegionIntent packet);

    enum class ManagementAction : uint8_t
    {
      NONE = 0,
      SET_REGION = 1,
      SET_WIFI_PRIMARY = 2,
      SET_WIFI_SECONDARY = 3,
      SET_REGION_AP = 4,
      REBOOT = 5,
      OPEN_SERVICE_AP = 6,
      PING = 7,
    };

    struct PacketManagement
    {
      uint8_t target_mac[6];
      uint8_t sender_mac[6];
      uint64_t region_id;
      ManagementAction action;
      uint16_t udp_channel;
      uint64_t new_region_id;
      uint8_t sta_mode;
      uint8_t ap_mode;
      uint8_t ap_channel;
      char ssid[33];
      char password[65];
      char region_name[48];
    };

    struct PacketPing
    {
      uint8_t mac[6];
      uint64_t region_id;
    };
  }
}
