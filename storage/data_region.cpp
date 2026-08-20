#include "data_region.h"
#include "esphome/core/log.h"

namespace esphome
{
  namespace storage
  {

    static const char *const TAG = "storage";

    void DataRegion::setup()
    {
#ifdef GSMART_FEATURE_REGION
      this->pref = global_preferences->make_preference<RegionLayout>(99991111UL, true);
      // Format 2 ma vlastny kluc (99991113). Stary (99991112) sa otvara uz len
      // preto, aby sa dal vymazat: zaznam formatu 1 sa neprekonvertuje, zahadzuje sa.
      this->metadata_pref = global_preferences->make_preference<RegionMetadata>(99991113UL, true);
      this->legacy_metadata_pref = global_preferences->make_preference<LegacyRegionMetadata>(99991112UL, true);

      if (this->pref.load(&this->layout))
      {
        ESP_LOGD(TAG, "Loaded saved region settings: %s", convertRegionSerialtoStr(this->layout.serial).c_str());
      }
      else
      {
        ESP_LOGD(TAG, "No saved region settings found");
      }

      if (this->metadata_pref.load(&this->metadata) && this->metadata.format == kRegionFormat)
      {
        ESP_LOGD(TAG, "Loaded region metadata format %u: name=%s port=%u version=%u",
                 this->metadata.format, this->metadata.name, this->metadata.regionPort, this->metadata.regionVersion);
      }
      else
      {
        this->discardPreFormat2Region();
      }
      recalculateLayout();
#endif
    }

    void DataRegion::save()
    {
#ifdef GSMART_FEATURE_REGION
      this->pref.save(&this->layout);
      this->metadata_pref.save(&this->metadata);
      global_preferences->sync();
#endif
    }

    void DataRegion::saveMetadata()
    {
#ifdef GSMART_FEATURE_REGION
      this->metadata_pref.save(&this->metadata);
      global_preferences->sync();
#endif
    }

    /// Zahodi vsetko, co po sebe nechal format 1.
    ///
    /// Tento firmvér format 1 nevie a nebude sa tvarit, ze ano. Miesto prekladu
    /// zacne kus tak, ako keby miestnost nikdy nemal: prazdny layout, prazdna
    /// metadata a prazdny aj stary zaznam - v NVS po miestnosti nezostane nic.
    ///
    /// Netyka sa to nicoho ineho. Wi-Fi, cloud, servisny AP, nacitane trubice,
    /// nocny update aj nastavenia v subore maju vlastne kluce a tie sa necitaju
    /// ani nezapisuju.
    ///
    /// Spat sa miestnost dostane z cloudu: kus si na spolocnej adrese vypyta
    /// layout, master ho podla MAC spozna a posle mu ho uz vo formate 2. Kym sa
    /// tak stane, stoji ako single a nesvieti so ziadnou miestnostou - to je ta
    /// cena, ktoru to stoji, a je zamerna.
    void DataRegion::discardPreFormat2Region()
    {
#ifdef GSMART_FEATURE_REGION
      LegacyRegionMetadata legacy{};
      const bool legacy_loaded = this->legacy_metadata_pref.load(&legacy);
      const bool legacy_has_content =
          legacy_loaded && (legacy.udpChannel != 0 || legacy.name[0] != 0 || legacy.configVersion != 0);
      const bool had_region = legacy_has_content || this->layout.serial != 0 || this->layout.memberCount != 0 ||
                              this->metadata.format != 0 || this->metadata.regionPort != 0 ||
                              this->metadata.legacyChannel != 0 || this->metadata.name[0] != 0;

      const uint8_t had_format = this->metadata.format;
      const uint8_t had_members = this->layout.memberCount;
      const uint16_t had_channel = legacy_has_content ? legacy.udpChannel : this->metadata.legacyChannel;

      this->layout = RegionLayout{};
      this->metadata = RegionMetadata{};
      this->selfIndex = -1;

      if (!had_region)
      {
        // Kus, ktory ziadnu miestnost nemal, nema co zahadzovat - a nema preco
        // pri kazdom starte prepisovat tri kluce v NVS.
        ESP_LOGD(TAG, "No saved region settings found");
        return;
      }

      // Maze sa aj stary kluc (99991112). Nechat ho tam znamena, ze po navrate
      // na starsi firmvér miestnost ozije v zlozeni, o ktorom uz nikto nevie.
      const LegacyRegionMetadata cleared_legacy{};
      this->pref.save(&this->layout);
      this->metadata_pref.save(&this->metadata);
      this->legacy_metadata_pref.save(&cleared_legacy);
      global_preferences->sync();

      ESP_LOGW(TAG,
               "Region record is not format %u (format %u, %u members, legacy channel %u) - discarded. "
               "This device now stands alone and waits to be adopted again.",
               kRegionFormat, had_format, had_members, had_channel);
#endif
    }

    void DataRegion::clear()
    {
#ifdef GSMART_FEATURE_REGION
      this->layout = RegionLayout{};
      this->metadata = RegionMetadata{};
      this->selfIndex = -1;
      this->save();
#endif
    }

  }
}
