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
      // Format 2 ma vlastny kluc. Stary (99991112) sa necha nedotknuty, aby sa
      // z neho dal prevziat nazov a kanal - a aby sa dalo vratit na starsi
      // firmvér bez toho, aby miestnost prisla o zaznam.
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
        this->adoptLegacyMetadata();
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

    /// Prevezme, co po sebe nechal format 1.
    ///
    /// Nazov, popis a kanal sa nesu dalej zamerne: miestnost, ktora pred
    /// nahratim firmveru fungovala, ma fungovat aj po nom. `format` ostava `0`,
    /// takze kus vie, ze tomuto zaznamu neveri a ma si vypytat novy - master
    /// z cloudu, clen od mastera na spolocnej adrese.
    void DataRegion::adoptLegacyMetadata()
    {
#ifdef GSMART_FEATURE_REGION
      LegacyRegionMetadata legacy{};
      this->metadata = RegionMetadata{};
      if (!this->legacy_metadata_pref.load(&legacy))
      {
        ESP_LOGD(TAG, "No saved region metadata found");
        return;
      }

      copyString(this->metadata.name, sizeof(this->metadata.name), legacy.name);
      copyString(this->metadata.description, sizeof(this->metadata.description), legacy.description);
      this->metadata.legacyChannel = legacy.udpChannel;
      this->metadata.regionVersion = clampRegionVersion(legacy.configVersion);
      this->metadata.format = 0;
      this->metadata.regionPort = 0;
      ESP_LOGI(TAG, "Adopted region metadata from format 1: name=%s channel=%u version=%u; waiting for format %u",
               this->metadata.name, this->metadata.legacyChannel, this->metadata.regionVersion, kRegionFormat);
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
