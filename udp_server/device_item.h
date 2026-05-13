#pragma once

#include "udp_global.h"
#include "esphome/components/storage/util.h"
// #include "esphome/components/json/json_util.h"

namespace esphome
{
  namespace udp_server
  {
    class DeviceItem
    {
    public:
      // DeviceItem();

      uint8_t mac[6];
      uint64_t region_id;
      uint8_t ip[4];
      uint16_t channel;
      uint8_t model;
      uint8_t build[2];
      uint32_t time;
      char name[25];

      uint32_t last_update;

      bool isMac(uint8_t macParam[6])
      {
        return mac[5] == macParam[5] &&
               mac[4] == macParam[4] &&
               mac[3] == macParam[3] &&
               mac[2] == macParam[2] &&
               mac[1] == macParam[1] &&
               mac[0] == macParam[0];
      }

      bool updateFromSysInfo(PacketSysInfo *packet)
      {
        memcpy(mac, packet->mac, sizeof(mac));
        region_id = packet->region_id;
        memcpy(ip, packet->ip, sizeof(ip));
        channel = packet->channel;
        model = packet->model;
        memcpy(build, packet->build, sizeof(build));
        time = packet->time;
        memcpy(name, packet->name, sizeof(name));
        last_update = millis();
        return true;
      }

      std::string toEventMessage()
      {
        return json::build_json([this](JsonObject root)
                                { this->toJson(root); });
      }

      void toJson(JsonObject &root)
      {
        root["mac"] = macToStr(this->mac);
        root["regionId"] = storage::convertRegionSerialtoStr(this->region_id);
        root["ip"] = ipToStr(this->ip);
        root["channel"] = this->channel;
        root["model"] = storage::convertModelToStr(this->model);
        root["modelNum"] = this->model;
        root["build"] = str_sprintf("%d.%d", this->build[0], this->build[1]);
        root["time"] = this->time;
        root["lastSeenMs"] = this->last_update;
        root["name"] = String(this->name);
      }

      bool fromJson(JsonObject &root)
      {
        // TODO
        return true;
      }

    protected:
    };

  }
}
