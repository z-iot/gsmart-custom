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
      this->metadata_pref = global_preferences->make_preference<RegionMetadata>(99991112UL, true);
      if (this->pref.load(&this->layout))
      {
        ESP_LOGD(TAG, "Loaded saved region settings: %s", convertRegionSerialtoStr(this->layout.serial).c_str());
      }
      else
      {
        ESP_LOGD(TAG, "No saved region settings found");
      }
      if (this->metadata_pref.load(&this->metadata))
      {
        ESP_LOGD(TAG, "Loaded saved region metadata: name=%s channel=%u version=%u", this->metadata.name, this->metadata.udpChannel, this->metadata.configVersion);
      }
      else
      {
        ESP_LOGD(TAG, "No saved region metadata found");
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

  }
}
