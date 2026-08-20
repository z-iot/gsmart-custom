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

    /// Ako je region ulozeny a ako sa o nom hovori na sieti.
    ///
    /// Format 1 je vsetko, co bolo pred tymto cislom: kanal 10-250, adresa
    /// 230.x, verzia ako 32-bit pecialka. Format 2 nesie regionPort, ktory je
    /// zaroven identita aj UDP port, adresu v 239.192.0.0/16 a 16-bitovu verziu.
    ///
    /// Tento firmvér drzi len format 2. Zaznam, ktory nim nie je, sa zahadzuje
    /// - pri starte aj pri zapise. Kus potom stoji ako single: bez miestnosti,
    /// bez cisla, bez clenov, a caka, kym ho jeho master alebo cloud prevezme
    /// nanovo. Prekladat medzi formatmi sa neoplatilo: kazdy taky preklad bol
    /// tvrdenie o stave, ktory nikto neoveril.
    static constexpr uint8_t kRegionFormat = 2;

    /// Rozsah portov, z ktoreho cloud prideluje regionPort. Firmvér ho
    /// nevynucuje, len sa podla neho rozhoduje, ci je hodnota vobec pouzitelna.
    static constexpr uint16_t kRegionPortMin = 30101;
    static constexpr uint16_t kRegionPortMax = 49151;

    /// Zaklad, na ktorom stal stary kanal.
    ///
    /// Format 1 hovoril na `30100 + kanal` a novy firmvér na tom cisle nic
    /// nezmenil - `activeRegionPort()` ho dopocitaval rovnako. Preto sa z neho
    /// da spravit `regionPort` bez toho, aby sa miestnost pohla: je to ten isty
    /// port aj ta ista skupina, na ktorej uz je. Cloud ho neskor moze prepisat
    /// na svoj pridelovany - vtedy sa miestnost presunie cela naraz.
    static constexpr uint16_t kLegacyPortBase = 30100;

    /// Port stareho kanala. `0`, ked ziadny kanal ulozeny nie je.
    inline uint16_t regionPortFromLegacyChannel(uint16_t channel)
    {
      return channel == 0 ? 0 : static_cast<uint16_t>(kLegacyPortBase + channel);
    }

    struct RegionMetadata
    {
      /// `0` znamena zaznam spred formatu 2 - vtedy plati `legacyChannel`.
      uint8_t format = 0;
      /// Identita miestnosti na sieti **a** port, na ktorom si jej kusy hovoria.
      uint16_t regionPort = 0;
      /// Pocitadlo zmien obsahu. Zvysuje ho vylucne cloud a nepretaca sa.
      uint16_t regionVersion = 0;
      /// Stary kanal 10-250. Drzi sa preto, aby miestnost bezala aj po nahrati
      /// firmveru dovtedy, kym jej cloud posle regionPort - a aby sa da nou
      /// rozpravat s kusmi, ktore na format 2 este neprešli.
      uint16_t legacyChannel = 0;
      char name[48] = {0};
      char description[128] = {0};
    };

    /// Zaznam, ktory na kuse zostal z formatu 1. Cita sa raz, pri prvom starte
    /// noveho firmveru, aby miestnost nezostala bez mena a bez kanala.
    struct LegacyRegionMetadata
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
        // Format 1 posielal cislo kanala. Berie sa uz len ako zaloha: kym cloud
        // neposle regionPort, miestnost bezi po starom a rozprava sa aj s kusmi,
        // ktore na format 2 este neprešli.
        if (!root["udpChannel"].isNull())
          this->metadata.legacyChannel = root["udpChannel"].as<uint16_t>();
        if (!root["regionNum"].isNull())
          this->metadata.legacyChannel = root["regionNum"].as<uint16_t>();
        if (!root["configVersion"].isNull())
          this->metadata.regionVersion = clampRegionVersion(root["configVersion"].as<uint32_t>());

        if (!root["regionPort"].isNull())
          this->metadata.regionPort = root["regionPort"].as<uint16_t>();
        if (!root["regionVersion"].isNull())
          this->metadata.regionVersion = clampRegionVersion(root["regionVersion"].as<uint32_t>());
        // Format sa uz len zvysuje, nikdy neklesa.
        //
        // Cloud posiela `regionFormat: 1` dovtedy, kym o kazdom clenovi miestnosti
        // nevie, ze ma novy firmvér - a kus, ktory sa pri starte preformatoval, by
        // sa kazdym takym zapisom vratil spat. Port si zo zapisu vezme, ten vlastni
        // cloud a je to jedine cislo, ktore rozhoduje, kde sa miestnost stretne.
        // Format nie. Zapis bez formatu, ale s portom, je zapis formatu 2.
        if (!root["regionFormat"].isNull())
        {
          const uint8_t incomingFormat = root["regionFormat"].as<uint8_t>();
          if (incomingFormat > this->metadata.format)
            this->metadata.format = incomingFormat;
        }
        else if (!root["regionPort"].isNull())
          this->metadata.format = kRegionFormat;
      }

      /// Verzia sa nepretaca. Cloud ju nad stropom uz nezvysi a povie, ze
      /// miestnost treba zalozit nanovo; firmvér preto len oreze, co pride.
      static uint16_t clampRegionVersion(uint32_t value)
      {
        return value > 0xFFFF ? 0xFFFF : static_cast<uint16_t>(value);
      }

      /// Bezi tento kus uz podla formatu 2?
      bool usesRegionFormat2() const
      {
        return this->metadata.format == kRegionFormat && this->metadata.regionPort != 0;
      }

      /// Port, na ktorom sa tato miestnost prave rozprava. `0` znamena, ze kus
      /// nema kde - vtedy pocuva len na spolocnej adrese a pyta si konfiguraciu.
      uint16_t activeRegionPort() const
      {
        if (this->usesRegionFormat2())
          return this->metadata.regionPort;
        return regionPortFromLegacyChannel(this->metadata.legacyChannel);
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
        this->normalizeMasterIndex();
      }

      int16_t firstEmitterMemberIndex() const
      {
        for (int i = 0; i < this->layout.memberCount; i++)
          if (isEmitterModel(this->layout.members[i].modelNum))
            return i;
        return -1;
      }

      bool isEmitterMember(uint8_t index) const
      {
        return index < this->layout.memberCount && isEmitterModel(this->layout.members[index].modelNum);
      }

      void normalizeMasterIndex()
      {
        if (this->layout.memberCount == 0)
        {
          this->layout.masterIndex = 0;
          return;
        }
        if (this->isEmitterMember(this->layout.masterIndex))
          return;
        const int16_t first_emitter_index = this->firstEmitterMemberIndex();
        this->layout.masterIndex = first_emitter_index >= 0 ? static_cast<uint8_t>(first_emitter_index) : this->layout.memberCount;
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
        if (!this->isRegionActive() || !this->isEmitterMember(this->layout.masterIndex))
          return false;
        return memcmp(this->layout.members[this->layout.masterIndex].mac, mac, 6) == 0;
      }

      bool hasMembers() const
      {
        return this->layout.memberCount > 0;
      }

      /// Zvysenie verzie po zapise, ktory ziadnu nepriniesol.
      ///
      /// Verziu vlastni ten, kto zapisuje - cloud aj appka ju posielaju
      /// v zapise, a vtedy sa berie presne to, co prislo (`apply_region`).
      /// Toto je poistka pre pisatela, ktory ju neposle (stara appka, servisny
      /// zasah po LAN-e): zmena obsahu sa nesmie schovat za nezmenene cislo.
      /// Na strope sa zastavi - pretocit ju na nulu by znamenalo, ze kazdy
      /// dalsi push bude vyzerat starsie nez to, co uz kus ma, a miestnost by
      /// sa prestala aktualizovat.
      void bumpConfigVersion()
      {
        if (this->metadata.regionVersion == 0xFFFF)
          return;
        this->metadata.regionVersion++;
      }

      void saveToJson(JsonObject &root)
      {
        root["serial"] = convertRegionSerialtoStr(this->layout.serial); // TODO
        root["mst"] = this->layout.masterIndex;
        root["regionName"] = this->metadata.name;
        root["regionDescription"] = this->metadata.description;
        root["regionFormat"] = this->metadata.format;
        root["regionPort"] = this->metadata.regionPort;
        root["regionVersion"] = this->metadata.regionVersion;
        // Stare kluce sa este vypisuju, aby appka a cloud, ktore format 2 zatial
        // neposielaju, nedostali prazdno tam, kde doteraz cakali cislo.
        root["udpChannel"] = this->metadata.legacyChannel;
        root["regionNum"] = this->metadata.legacyChannel;
        root["configVersion"] = this->metadata.regionVersion;
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
        return isRegionActive() && this->selfIndex >= 0 && this->selfIndex == this->layout.masterIndex && this->isEmitterMember(this->selfIndex);
      }

      bool isRegionActive() const
      {
        return this->layout.serial != 0;
      }

      void setup();
      void save();
      void saveMetadata();
      void discardPreFormat2Region();
      void clear();

      RegionLayout layout;
      RegionMetadata metadata;
      int16_t selfIndex = -1;
      ESPPreferenceObject pref{};
      ESPPreferenceObject metadata_pref{};
      ESPPreferenceObject legacy_metadata_pref{};

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
