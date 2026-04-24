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
      if (this->pref.load(&this->layout))
      {
        ESP_LOGD(TAG, "Loaded saved region settings: %s", convertRegionSerialtoStr(this->layout.serial).c_str());
      }
      else
      {
        ESP_LOGD(TAG, "No saved region settings found");
      }
      recalculateLayout();
#endif
    }

    void DataRegion::save()
    {
#ifdef GSMART_FEATURE_REGION
      this->pref.save(&this->layout);
      global_preferences->sync();
#endif
    }

  }
}
