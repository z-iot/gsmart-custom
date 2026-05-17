#pragma once

#include "device_item.h"
#include <cstddef>
#include <string>

namespace esphome
{
  namespace udp_server
  {
    static constexpr size_t DEVICE_LIST_MAX_ITEMS = 16;

    class DeviceList
    {
    public:
      DeviceItem* findByMac(uint8_t mac[6])
      {
        for (size_t i = 0; i < ItemsCount; i++)
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
            if (this->ItemsCount < DEVICE_LIST_MAX_ITEMS)
            {
              item = &this->pool_[this->ItemsCount];
              this->Items[this->ItemsCount] = item;
              this->ItemsCount++;
            }
            else
            {
              item = this->oldestItem_();
              ESP_LOGW(TAG, "UDP DeviceList full (%u); replacing oldest device with %s", static_cast<unsigned>(DEVICE_LIST_MAX_ITEMS), macToStr(packet->mac).c_str());
            }
          }
        item->updateFromSysInfo(packet);
        return item;
      };

      void toJson(JsonObject &root)
      {
        JsonArray arr = root["devices"].to<JsonArray>();
        for (size_t i = 0; i < this->ItemsCount; i++)
        {
          auto *item = this->Items[i];
          if (item == nullptr)
            continue;
          JsonObject arrItem = arr.add<JsonObject>();
          item->toJson(arrItem);
        }
      }

      bool fromJson(JsonObject &root)
      {
        //TODO - implement
        return true;
      }

      DeviceItem *Items[DEVICE_LIST_MAX_ITEMS]{};
      size_t ItemsCount = 0;

    protected:
      DeviceItem pool_[DEVICE_LIST_MAX_ITEMS]{};

      DeviceItem *oldestItem_()
      {
        DeviceItem *oldest = this->Items[0];
        for (size_t i = 1; i < this->ItemsCount; i++)
        {
          if (this->Items[i] != nullptr && this->Items[i]->last_update < oldest->last_update)
            oldest = this->Items[i];
        }
        return oldest;
      }
    };

  }
}
