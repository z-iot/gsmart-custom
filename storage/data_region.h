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

    struct RegionMetadata
    {
      char name[48] = {0};
      char description[128] = {0};
      uint16_t udpChannel = 0;
      uint32_t configVersion = 0;
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

      void loadMetadataFromJson(JsonObject &root)
      {
        if (!root["regionName"].isNull())
          copyString(this->metadata.name, sizeof(this->metadata.name), root["regionName"].as<std::string>());
        if (!root["name"].isNull())
          copyString(this->metadata.name, sizeof(this->metadata.name), root["name"].as<std::string>());
        if (!root["regionDescription"].isNull())
          copyString(this->metadata.description, sizeof(this->metadata.description), root["regionDescription"].as<std::string>());
        if (!root["description"].isNull())
          copyString(this->metadata.description, sizeof(this->metadata.description), root["description"].as<std::string>());
        if (!root["udpChannel"].isNull())
          this->metadata.udpChannel = root["udpChannel"].as<uint16_t>();
        if (!root["regionNum"].isNull())
          this->metadata.udpChannel = root["regionNum"].as<uint16_t>();
        if (!root["configVersion"].isNull())
          this->metadata.configVersion = root["configVersion"].as<uint32_t>();
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

      int16_t memberIndexForMac(const uint8_t mac[6]) const
      {
        for (int i = 0; i < this->layout.memberCount; i++)
        {
          if (memcmp(this->layout.members[i].mac, mac, 6) == 0)
            return i;
        }
        return -1;
      }

      bool isMemberMac(const uint8_t mac[6]) const
      {
        return this->memberIndexForMac(mac) >= 0;
      }

      bool isMasterMac(const uint8_t mac[6]) const
      {
        if (!this->isRegionActive() || this->layout.masterIndex >= this->layout.memberCount)
          return false;
        return memcmp(this->layout.members[this->layout.masterIndex].mac, mac, 6) == 0;
      }

      bool hasMembers() const
      {
        return this->layout.memberCount > 0;
      }

      void bumpConfigVersion()
      {
        this->metadata.configVersion++;
        if (this->metadata.configVersion == 0)
          this->metadata.configVersion = 1;
      }

      void saveToJson(JsonObject &root)
      {
        root["serial"] = convertRegionSerialtoStr(this->layout.serial); // TODO
        root["mst"] = this->layout.masterIndex;
        root["regionName"] = this->metadata.name;
        root["regionDescription"] = this->metadata.description;
        root["udpChannel"] = this->metadata.udpChannel;
        root["regionNum"] = this->metadata.udpChannel;
        root["configVersion"] = this->metadata.configVersion;
        JsonArray memArray = root["mem"].to<JsonArray>();
        for (int i = 0; i < this->layout.memberCount; i++)
        {
          JsonObject memObj = memArray.add<JsonObject>();
          memObj["b"] = convertModelToStr(this->layout.members[i].modelNum);
          memObj["m"] = convertMacToStr(this->layout.members[i].mac);
        }
      }

      bool isMaster() const
      {
        return isRegionActive() && this->selfIndex == this->layout.masterIndex;
      }

      bool isRegionActive() const
      {
        return this->layout.serial != 0;
      }

      void setup();
      void save();
      void saveMetadata();
      void clear();

      RegionLayout layout;
      RegionMetadata metadata;
      int16_t selfIndex = -1;
      ESPPreferenceObject pref{};
      ESPPreferenceObject metadata_pref{};

    protected:
      void copyString(char *target, size_t target_size, const std::string &value)
      {
        if (target_size == 0)
          return;
        strncpy(target, value.c_str(), target_size - 1);
        target[target_size - 1] = 0;
      }
    };

  }
}
