#include "udp_global.h"
#include "esphome/components/storage/util.h"

namespace esphome
{
  namespace udp_server
  {

    std::string macToStr(const uint8_t mac[6])
    {
      return str_sprintf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    std::string ipToStr(const uint8_t ip[4])
    {
      return str_sprintf("%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    };

    std::string packetKindToStr(PacketKind item)
    {
      switch (item)
      {
      case PacketKind::UNKNOWN:
        return "UNKNOWN";
      case PacketKind::SYS_INFO:
        return "SYS_INFO";
      case PacketKind::CONTROL:
        return "CONTROL";
      case PacketKind::STATUS:
        return "STATUS";
      case PacketKind::IDENTITY:
        return "IDENTITY";
      case PacketKind::RECONFIG:
        return "RECONFIG";
      case PacketKind::MANAGEMENT:
        return "MANAGEMENT";
      case PacketKind::SITUATION:
        return "SITUATION";
      case PacketKind::REGION_LAYOUT:
        return "REGION_LAYOUT";
      case PacketKind::REGION_INTENT:
        return "REGION_INTENT";
      default:
        return "--unknown--";
      }
    };

    std::string radiationModeToStr(storage::RadiationMode item)
    {
      switch (item)
      {
      case storage::RadiationMode::OFF:
        return "OFF";
      case storage::RadiationMode::MIN:
        return "MIN";
      case storage::RadiationMode::STD:
        return "STD";
      case storage::RadiationMode::MAX:
        return "MAX";
      case storage::RadiationMode::ON:
        return "ON";
      default:
        return "--unknown--";
      }
    };

    std::string kindRadiationSourceToStr(KindRadiationSource item)
    {
      switch (item)
      {
      case KindRadiationSource::SOURCE_UNKNOWN:
        return "SOURCE_UNKNOWN";
      case KindRadiationSource::SOURCE_EXT:
        return "SOURCE_EXT";
      case KindRadiationSource::ACTUATOR:
        return "ACTUATOR";
      case KindRadiationSource::EMITTER:
        return "EMITTER";
      case KindRadiationSource::SCHEDULLER:
        return "SCHEDULLER";
      case KindRadiationSource::SWITCH:
        return "SWITCH";
      case KindRadiationSource::REGION_MASTER:
        return "REGION_MASTER";
      default:
        return "--unknown--";
      }
    };

    std::string kindStatusEventToStr(KindStatusEvent item)
    {
      switch (item)
      {
      case KindStatusEvent::INTERVAL:
        return "INTERVAL";
      case KindStatusEvent::RADIATION:
        return "RADIATION";
      case KindStatusEvent::LAMP:
        return "LAMP";
      case KindStatusEvent::MOTION:
        return "MOTION";
      default:
        return "--unknown--";
      }
    };

    std::string kindLampModeToStr(KindLampMode item)
    {
      switch (item)
      {
      case KindLampMode::ON_START:
        return "ON_START";
      case KindLampMode::ON:
        return "ON";
      case KindLampMode::OFF_START:
        return "OFF_START";
      case KindLampMode::OFF:
        return "OFF";
      default:
        return "--unknown--";
      }
    };

    std::string kindMotionSourceToStr(KindMotionSource item)
    {
      switch (item)
      {
      case KindMotionSource::UNKNOWN:
        return "UNKNOWN";
      case KindMotionSource::EXT:
        return "EXT";
      case KindMotionSource::RADAR:
        return "RADAR";
      case KindMotionSource::PIR:
        return "PIR";
      case KindMotionSource::DOOR:
        return "DOOR";
      default:
        return "--unknown--";
      }
    };

    std::string packetControlToJsonStr(PacketControl packet)
    {
      return json::build_json([packet](JsonObject root)
                              { 
                                root["mac"] = macToStr(packet.mac);
                                root["regionId"] = storage::convertRegionSerialtoStr(packet.region_id);
                                root["mode"] = radiationModeToStr(packet.mode);
                                root["source"] = kindRadiationSourceToStr(packet.source); });
    }

    std::string packetStatusToJsonStr(PacketStatus packet)
    {
      return json::build_json([packet](JsonObject root)
                              { 
                                root["mac"] = macToStr(packet.mac);
                                root["regionId"] = storage::convertRegionSerialtoStr(packet.region_id);
                                root["event"] = kindStatusEventToStr(packet.event);
                                root["radiation"] = radiationModeToStr(packet.radiation);
                                root["radiationSource"] = kindRadiationSourceToStr(packet.radiationSource);
                                root["lamp"] = kindLampModeToStr(packet.lamp);
                                root["motion"] = packet.motion;
                                root["motionSource"] = kindMotionSourceToStr(packet.motionSource); });
    };

    std::string packetIdentityToJsonStr(PacketIdentity packet)
    {
      return json::build_json([packet](JsonObject root)
                              { 
                                root["mac"] = macToStr(packet.mac);
                                root["regionId"] = storage::convertRegionSerialtoStr(packet.region_id);
                                root["ip"] = ipToStr(packet.ip);
                                root["model"] = storage::convertModelToStr(packet.model);
                                root["time"] = packet.time;
                                root["name"] = packet.name;
                                root["consumable_lamp_count"] = packet.consumable_lamp_count;
                                // root["consumable"] = json::build_json([packet](JsonArray root)
                                //                                       {
                                //                                         for (int i = 0; i < packet.consumable_lamp_count; i++)
                                //                                         {
                                //                                           root[i] = json::build_json([packet, i](JsonObject root)
                                //                                                                      {
                                //                                                                        root["hours_max"] = packet.consumable[i].hours_max;
                                //                                                                        root["last_day_change"] = packet.consumable[i].last_day_change;
                                //                                                                        root["start_count"] = packet.consumable[i].start_count;
                                //                                                                        root["usage_sec"] = packet.consumable[i].usage_sec;
                                //                                                                      });
                                //                                         }
                                //                                       });
                                root["cleaned_total"] = packet.cleaned_total;
                                root["cleaned_today"] = packet.cleaned_today;
                                root["cleaned_yesterday"] = packet.cleaned_yesterday;
                                root["total_start_count"] = packet.total_start_count;
                                root["total_usage_sec"] = packet.total_usage_sec;
                                root["current_day"] = packet.current_day; });
    }

    std::string packetSituationToJsonStr(PacketSituation packet)
    {
      return json::build_json([packet](JsonObject root)
                              {
                                root["mac"] = macToStr(packet.mac);
                                root["regionId"] = storage::convertRegionSerialtoStr(packet.region_id);
                                root["source"] = kindRadiationSourceToStr(packet.source);
                                root["activeMode"] = radiationModeToStr(packet.active_mode);
                                root["schedulerActive"] = packet.scheduler_active;
                                root["schedulerItemsCount"] = packet.scheduler_items_count;
                                root["currentIsActive"] = packet.current_is_active;
                                root["currentIsSchedule"] = packet.current_is_schedule;
                                root["currentIsExternal"] = packet.current_is_external;
                                root["currentMode"] = radiationModeToStr(packet.current_mode);
                                root["currentBeginTime"] = packet.current_begin_time;
                                root["currentEndTime"] = packet.current_end_time;
                                root["currentBeamedSec"] = packet.current_beamed_sec;
                                root["currentTotalSec"] = packet.current_total_sec;
                                root["prevMode"] = radiationModeToStr(packet.prev_mode);
                                root["prevBeginTime"] = packet.prev_begin_time;
                                root["prevEndTime"] = packet.prev_end_time;
                                root["prevBeamedSec"] = packet.prev_beamed_sec;
                                root["prevTotalSec"] = packet.prev_total_sec;
                                root["scheduleMode"] = radiationModeToStr(packet.schedule_mode);
                                root["scheduleBeginTime"] = packet.schedule_begin_time;
                                root["scheduleEndTime"] = packet.schedule_end_time;
                                root["scheduleTotalSec"] = packet.schedule_total_sec;
                                root["scheduleIsAborted"] = packet.schedule_is_aborted;
                                root["nextMode"] = radiationModeToStr(packet.next_mode);
                                root["nextBeginTime"] = packet.next_begin_time;
                                root["nextEndTime"] = packet.next_end_time;
                                root["nextTotalSec"] = packet.next_total_sec; });
    }

    std::string regionLayoutActionToStr(RegionLayoutAction item)
    {
      switch (item)
      {
      case RegionLayoutAction::REQUEST:
        return "REQUEST";
      case RegionLayoutAction::PUSH:
        return "PUSH";
      case RegionLayoutAction::RESPONSE:
        return "RESPONSE";
      default:
        return "--unknown--";
      }
    }

    std::string packetRegionLayoutToJsonStr(PacketRegionLayout packet)
    {
      return json::build_json([packet](JsonObject root)
                              {
                                root["mac"] = macToStr(packet.mac);
                                root["regionId"] = storage::convertRegionSerialtoStr(packet.region_id);
                                root["action"] = regionLayoutActionToStr(packet.action);
                                root["udpChannel"] = packet.udp_channel;
                                root["configVersion"] = packet.config_version;
                                root["masterIndex"] = packet.master_index;
                                root["memberCount"] = packet.member_count;
                                JsonArray members = root["members"].to<JsonArray>();
                                for (int i = 0; i < packet.member_count && i < 16; i++)
                                {
                                  JsonObject member = members.add<JsonObject>();
                                  member["index"] = i;
                                  member["model"] = storage::convertModelToStr(packet.members[i].modelNum);
                                  member["modelNum"] = packet.members[i].modelNum;
                                  member["mac"] = macToStr(packet.members[i].mac);
                                  member["master"] = i == packet.master_index;
                                } });
    }

    std::string packetRegionIntentToJsonStr(PacketRegionIntent packet)
    {
      return json::build_json([packet](JsonObject root)
                              {
                                root["originMac"] = macToStr(packet.origin_mac);
                                root["regionId"] = storage::convertRegionSerialtoStr(packet.region_id);
                                root["sequence"] = packet.sequence;
                                root["mode"] = radiationModeToStr(packet.mode);
                                root["source"] = kindRadiationSourceToStr(packet.source);
                                JsonObject cause = root["cause"].to<JsonObject>();
                                cause["kind"] = storage::radiationCauseKindToApi(packet.cause.kind);
                                cause["detail"] = packet.cause.detail;
                                cause["originMac"] = macToStr(packet.cause.originMac);
                                cause["originSerial"] = packet.cause.originSerial;
                                cause["originModel"] = packet.cause.originModel; });
    }
  }
}
