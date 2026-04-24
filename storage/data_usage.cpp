#include "data_usage.h"

namespace esphome
{
  namespace storage
  {

    static const char *const TAG = "storage";

#ifdef GSMART_EMITTER
    void DataUsage::updateLamp(uint8_t lampNum, bool on)
    {
      ESP_LOGD(TAG, "updateLamp n%d to %d", lampNum, on);
      if (lampNum >= beam.pref.lampCount)
        return;

      auto nowSec = millis() / 1000;
      if (on)
      {
        if (lampOnCount() == 0)
        {
          beam.pref.startCount++;
          beam.lastStart = nowSec;
        }
        lamp[lampNum].pref.startCount++;
        lamp[lampNum].lastStart = nowSec;
      }
      else
      {
        if (lampOnCount() == 1)
        {
          beam.pref.stopCount++;
          if (beam.lastStart > beam.lastStop)
            beam.pref.onSec += nowSec - beam.lastStart;
          beam.lastStop = nowSec;
          beam.prefObj.save(&beam.pref);
        }
        lamp[lampNum].pref.stopCount++;
        if (lamp[lampNum].lastStart > lamp[lampNum].lastStop)
          lamp[lampNum].pref.onSec += nowSec - lamp[lampNum].lastStart;
        lamp[lampNum].lastStop = nowSec;
        lamp[lampNum].prefObj.save(&lamp[lampNum].pref);
      }
      lastCheck = nowSec;
      lastChange = nowSec;
    };
#endif

    void DataUsage::setup()
    {
#ifdef GSMART_EMITTER
      beam.prefObj = global_preferences->make_preference<UsageBeamPref>(99991121UL, true);
      if (!beam.prefObj.load(&beam.pref))
      {
        ESP_LOGD(TAG, "No saved usageBeam settings found");
        if (beam.pref.lampCount == -1 || beam.pref.fanCount == -1)
        {
#if defined(GSMART_MODEL_SIBRA)
          beam.pref.lampCount = 2;
          beam.pref.fanCount = 2;
#elif defined(GSMART_MODEL_OPERA) || defined(GSMART_MODEL_AQUA) || defined(GSMART_MODEL_MOBI)
          beam.pref.lampCount = 1;
          beam.pref.fanCount = 0;
#else
          beam.pref.lampCount = 0;
          beam.pref.fanCount = 0;
#endif
          beam.prefObj.save(&beam.pref);
        }
      }
        for (int i = 0; i < beam.pref.lampCount; i++)
        {
          lamp[i].prefObj = global_preferences->make_preference<UsageLampPref>(99991131UL + i, true);
          if (!lamp[i].prefObj.load(&lamp[i].pref))
            ESP_LOGD(TAG, "No saved usageLamp settings found");
        }
#endif
    }

    void DataUsage::save()
    {
#ifdef GSMART_EMITTER
      beam.prefObj.save(&beam.pref);
      for (int i = 0; i < beam.pref.lampCount; i++)
        lamp[i].prefObj.save(&lamp[i].pref);
      // global_preferences->sync();
#endif
    }

  }
}
