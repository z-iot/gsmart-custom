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
      default:
        return "--unknown--";
      }
    };

    std::string radiationModeToStr(storage::RadiationMode item)
    {
      switch (item)
      {
      case storage::RadiationMode::NONE:
        return "NONE";
      case storage::RadiationMode::MIN:
        return "MIN";
      case storage::RadiationMode::STD:
        return "STD";
      case storage::RadiationMode::MAX:
        return "MAX";
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
                                root["mode"] = radiationModeToStr(packet.mode);
                                root["source"] = kindRadiationSourceToStr(packet.source); });
    }

    std::string packetStatusToJsonStr(PacketStatus packet)
    {
      return json::build_json([packet](JsonObject root)
                              { 
                                root["mac"] = macToStr(packet.mac);
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
  }
}
