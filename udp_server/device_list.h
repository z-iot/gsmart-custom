#pragma once

#include "device_item.h"
#include <string>

namespace esphome
{
  namespace udp_server
  {
    class DeviceList
    {
    public:
      // DeviceList();

      DeviceItem* findByMac(uint8_t mac[6])
      {
        for (int i = 0; i < Items.size(); i++)
          if (Items[i]->isMac(mac))
            return Items[i];
        
        return nullptr;
      }

      DeviceItem* updateFromSysInfo(PacketSysInfo* packet)
      {
        DeviceItem* item = findByMac(packet->mac);
        if (item == nullptr)
          {
            ESP_LOGD(TAG, "UDP DeviceList new device %s %s", ipToStr(packet->ip).c_str(), macToStr(packet->mac).c_str());
            item = new DeviceItem();
            this->Items.push_back(item);
          }
        item->updateFromSysInfo(packet);
        return item;
      };

      void toJson(JsonObject &root)
      {
        JsonArray arr = root.createNestedArray("devices");
        for (auto &item : this->Items)
        {
          JsonObject arrItem = arr.createNestedObject();
          item->toJson(arrItem);
        }
      }

      bool fromJson(JsonObject &root)
      {
        //TODO - implement
        return true;
      }

      std::vector<DeviceItem*> Items;

    protected:
    };

  }
}
