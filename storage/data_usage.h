#pragma once

#include <Arduino.h>
#include <string>
#include "esphome/components/json/json_util.h"

#include "esphome/core/preferences.h"
#include "esphome/core/log.h"
#include "esphome/core/defines.h"

#define DEVICE_MAX_LAMP 3
#define DEVICE_MAX_FAN 2

namespace esphome
{
  namespace storage
  {

    // enum KindMotionSource
    // {
    //   UNKNOWN = 0,
    //   EXT = 11,
    // };

    struct UsageBeamPref
    {
      int8_t lampCount = -1; // pocet lamp
      int8_t fanCount = -1;  // pocet ventilatorov

      uint32_t onSec = 0;
      uint16_t startCount = 0;
      uint16_t stopCount = 0;
    };

    struct UsageBeam
    {
      UsageBeamPref pref;
      ESPPreferenceObject prefObj{};
      uint32_t lastStart = 0;
      uint32_t lastStop = 0;
    };

    struct UsageLampPref
    {
      uint32_t onSec = 0;
      uint16_t startCount = 0;
      uint16_t stopCount = 0;
      uint32_t lastExchangeDate = 0;
      uint32_t lastExchangeLiveHour = 8000;
    };

    struct UsageLamp
    {
      UsageLampPref pref;
      ESPPreferenceObject prefObj{};
      uint32_t lastStart = 0;
      uint32_t lastStop = 0;
    };

    struct UsageFan
    {
      uint32_t rotationCount = 0;
      uint32_t onSec = 0;
      uint16_t startCount = 0;
      uint16_t stopCount = 0;
      uint32_t lastStart = 0;
      uint32_t lastStop = 0;
      uint32_t lastExchangeDate = 0;
    };

    struct UsageMotion
    {
      uint32_t onSec = 0;
      uint32_t offSec = 0;
      uint16_t startCount = 0;
      uint16_t stopCount = 0;
      uint32_t lastStart = 0;
      uint32_t lastStop = 0;
    };

    struct UsageError
    {
      uint32_t totalCount = 0;
      std::string lastDesc = "";
      uint16_t lastCode = 0;
    };

    class DataUsage
    {
    public:

    void reloadFromJson(JsonObject &root)
      {
#ifdef GSMART_EMITTER
        loadFromJson(root);
        ESP_LOGW("usage", "reloadFromJson");
        save();
#endif        
      }

#ifdef GSMART_EMITTER
      uint8_t lampOnCount()
      {
        uint8_t count = 0;
        for (int i = 0; i < beam.pref.lampCount; i++)
          if (lamp[i].lastStart > lamp[i].lastStop)
            count++;
        return count;
      };

      void updateMotion(bool on)
      {
        auto nowSec = millis() / 1000;
        if (on)
        {
          motion.startCount++;
          if (motion.lastStop > motion.lastStart)
            motion.offSec += nowSec - motion.lastStop;
          motion.lastStart = nowSec;
        }
        else
        {
          motion.stopCount++;
          if (motion.lastStart > motion.lastStop)
            motion.onSec += nowSec - motion.lastStart;
          motion.lastStop = nowSec;
        }
        lastCheck = nowSec;
        lastChange = nowSec;
      };

      void loadFromJson(JsonObject &root)
      {
        if (!root["beam"].isNull())
        {
          auto b = root["beam"].as<JsonObject>();
          if (!b["lampCount"].isNull())
            beam.pref.lampCount = b["lampCount"].as<uint32_t>();
          if (!b["fanCount"].isNull())
            beam.pref.fanCount = b["fanCount"].as<uint32_t>();
          if (!b["onSec"].isNull())
            beam.pref.onSec = b["onSec"].as<uint32_t>();
          if (!b["beam.startCount"].isNull())
            beam.pref.startCount = b["startCount"].as<uint32_t>();
          if (!b["beam.stopCount"].isNull())
            beam.pref.startCount = b["stopCount"].as<uint32_t>();
        }
        for (int i = 0; i < DEVICE_MAX_LAMP; i++)
        {
          std::string lampName = ("l" + String(i)).c_str();
          if (!root[lampName].isNull())
          {
            auto lmp = root[lampName].as<JsonObject>();
            if (!lmp["onSec"].isNull())
              lamp[i].pref.onSec = root["onSec"].as<uint32_t>();
            if (!lmp["startCount"].isNull())
              lamp[i].pref.startCount = root["startCount"].as<uint32_t>();
            if (!lmp["stopCount"].isNull())
              lamp[i].pref.onSec = root["stopCount"].as<uint32_t>();
            if (!lmp["lastExchangeDate"].isNull())
              lamp[i].pref.lastExchangeDate = root["lastExchangeDate"].as<uint32_t>();
            if (!lmp["lastExchangeLiveHour"].isNull())
              lamp[i].pref.lastExchangeLiveHour = root["lastExchangeLiveHour"].as<uint32_t>();
          }
        }
      }
#endif

      void fillAdvertise(JsonObject &root)
      {
        auto nowSec = millis() / 1000;
#ifdef GSMART_EMITTER
        root["beamOnSec"] = beam.pref.onSec;
        root["beamStCnt"] = beam.pref.startCount;
#ifdef GSMART_MODEL_SIBRA
        root["fanOnSec"] = fan[0].onSec;
        root["fanRot"] = fan[0].rotationCount;
#endif
        root["motOnSec"] = motion.onSec;
        // root["motOffSec"] = motion.offSec;
        root["motStCnt"] = motion.startCount;
#endif
        root["errMsg"] = error.lastDesc;
        root["errCnt"] = error.totalCount;
      };

      std::string formatOnSec(uint32_t sec)
      {
        uint32_t hours = sec / 3600;
        int minutes = (sec % 3600) / 60;
        char buffer[30];
        sprintf(buffer, "%dh %dm", hours, minutes);
        return std::string(buffer);
      }

#ifdef GSMART_EMITTER
      void updateLamp(uint8_t lampNum, bool on);
#endif

      void setup();
      void save();

#ifdef GSMART_EMITTER
      UsageBeam beam;
      UsageLamp lamp[DEVICE_MAX_LAMP];
      UsageFan fan[DEVICE_MAX_FAN];
      UsageMotion motion;
#endif
      UsageError error;

      uint32_t lastCheck = 0;       // naposledy kontrolovane v ms od startu
      uint32_t lastChange = 0;      // naposledy zmene udaje v ms od startu
      uint32_t lastStorage = 0;     // naposledy ulozene udaje v ms od startu
      uint32_t manufactureDate = 0; // datum vyroby v sec od 1.1.2000
      uint32_t firstRunDate = 0;    // datum prveho spustenia v sec od 1.1.2000
      std::string catalog = "";     // katalogovy kod

      uint32_t motionOnSec = 0;
      uint16_t motionStartCount = 0;
    };

  }
}
