#pragma once

#include <Arduino.h>
#include <string>
#include <vector>
#include "esphome/components/json/json_util.h"
#include "util.h"

#include "esphome/core/preferences.h"
#include "esphome/core/log.h"

namespace esphome
{
  namespace storage
  {

    struct RegionMember
    {
      uint8_t modelNum = 0;
      uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
    };

    struct RegionLayout
    {
      uint64_t serial = 0;
      uint8_t masterIndex = 0;
      uint8_t memberCount = 0;
      RegionMember members[16];
    };

    class DataRegion
    {

    public:
      // DataRegion()
      // {
      // }
      void reloadFromJson(JsonObject &root)
      {
        loadFromJson(root); // TODO porovnat stary a novy region config
        ESP_LOGW("region", "reloadFromJson: %s / %d", convertRegionSerialtoStr(layout.serial).c_str(), layout.memberCount);
        save();
      }

      void loadFromJson(JsonObject &root)
      {
        uint64_t serial = this->layout.serial;
        this->layout.memberCount = 0;
        this->layout.masterIndex = 0;
        this->layout.serial = serial;

        // this->layout.serial = convertRegionSerialtoNum(root["serial"].as<std::string>());
        if (root["mem"].isNull())
        {
        }
        else
        {
          JsonArray memArray = root["mem"].as<JsonArray>();
          if (memArray.size() <= 16)
          {
            this->layout.masterIndex = root["mst"].as<uint8_t>();
            this->layout.memberCount = memArray.size();
            for (int i = 0; i < this->layout.memberCount; i++)
            {
              this->layout.members[i].modelNum = convertModelToNum(memArray[i]["b"].as<std::string>());
              convertMacToArray(memArray[i]["m"].as<std::string>(), this->layout.members[i].mac);
            }
          }
          else
          {
            // TODO osetrit chybu
          }
        }
        recalculateLayout();
      }

      void recalculateLayout()
      {
        // pozriet na ktorom mieste je vlastna mac adresa = selfIndex
        this->selfIndex = -1;
        uint8_t selfMac[6];
        get_mac_address_raw(selfMac);
        for (int i = 0; i < this->layout.memberCount; i++)
        {
          if (memcmp(this->layout.members[i].mac, selfMac, 6) == 0)
          {
            this->selfIndex = i;
            break;
          }
        }
      }

      void saveToJson(JsonObject &root)
      {
        root["serial"] = convertRegionSerialtoStr(this->layout.serial); // TODO
        root["mst"] = this->layout.masterIndex;
        JsonArray memArray = root.createNestedArray("mem");
        for (int i = 0; i < this->layout.memberCount; i++)
        {
          JsonObject memObj = memArray.createNestedObject();
          memObj["b"] = convertModelToStr(this->layout.members[i].modelNum);
          memObj["m"] = convertMacToStr(this->layout.members[i].mac);
        }
      }

      bool isMaster()
      {
        return isRegionActive() && this->selfIndex == this->layout.masterIndex;
      }

      bool isRegionActive()
      {
        return this->layout.serial != 0;
      }

      void setup();
      void save();

      RegionLayout layout;
      int16_t selfIndex = -1;
      ESPPreferenceObject pref{};
    };

  }
}
