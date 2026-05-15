#pragma once

#include <Arduino.h>
#include "global.h"
#include "esphome/components/json/json_util.h"


namespace esphome
{
  namespace storage
  {

    enum RadiationSource
    {
      INT = 1,
      EXT = 2,
      SCH = 3,
      REGION = 4,
    };

    struct RadiationSettings
    {
      uint32_t guardDuration = 3 * 60 * 60; // 3hod
      RadiationMode activeMode = RadiationMode::OFF;
      RadiationMode nextModeSelector = RadiationMode::STD;
      RadiationSource lastSource = RadiationSource::INT;
      uint32_t lastStart = 0;
      uint32_t lastStop = 0;
    };

    struct ConSettings
    {
      uint16_t disconnectCount = 0;
      uint16_t disconnectSecTotal = 0;
      uint16_t disconnectSecLast = 0;
      uint32_t lastConnect = 0;
      uint32_t lastDisconnect = 0;
    };

    struct SituationInfo
    {
      bool SchedulerActive = false;
      uint16_t SchedulerItemsCount = 0;
      RadiationSource source = RadiationSource::INT;

      uint32_t BeamBeginTime = 0;
      uint32_t BeamEndTime = 0;

      uint32_t CurrentBeginTime = 0;
      uint32_t CurrentEndTime = 0;
      uint16_t CurrentBeamedSec = 0;
      uint16_t CurrentTotalSec = 0;
      RadiationMode CurrentMode = RadiationMode::OFF;
      bool CurrentIsActive = false; // active/inactive
      bool CurrentIsSchedule = false; // manual/schedule
      bool CurrentIsExternal = false; // internal/external

      uint32_t PrevBeginTime = 0;
      uint32_t PrevEndTime = 0;
      uint16_t PrevBeamedSec = 0;
      uint16_t PrevTotalSec = 0;
      RadiationMode PrevMode = RadiationMode::OFF;
      
      uint32_t SchBeginTime = 0;
      uint32_t SchEndTime = 0;
      uint16_t SchTotalSec = 0;
      RadiationMode SchMode = RadiationMode::OFF;
      bool SchIsAborted = false;

      uint32_t NextBeginTime = 0;
      uint32_t NextEndTime = 0;
      uint16_t NextTotalSec = 0;
      RadiationMode NextMode = RadiationMode::OFF;
    };

    class DataGlobal
    {

    public:
      // DataGlobal()
      // {
      // }

      bool isGuardDurationOverflow()
      {
        if (this->radiation.activeMode == RadiationMode::OFF)
          return false;

        uint32_t now = millis() / 1000;
        uint32_t diff = now - this->radiation.lastStart;
        return diff > this->radiation.guardDuration;
      }

      std::string situationToJsonStr()
      {
        return json::build_json([this](JsonObject root)
                                {
                                  root["SchedulerActive"] = this->situation.SchedulerActive;
                                  root["SchedulerItemsCount"] = this->situation.SchedulerItemsCount;
                                  root["source"] = this->situation.source;
                                  root["BeamBeginTime"] = this->situation.BeamBeginTime;
                                  root["BeamEndTime"] = this->situation.BeamEndTime;

                                  root["CurrentBeginTime"] = this->situation.CurrentBeginTime;
                                  root["CurrentEndTime"] = this->situation.CurrentEndTime;
                                  root["CurrentBeamedSec"] = this->situation.CurrentBeamedSec;
                                  root["CurrentTotalSec"] = this->situation.CurrentTotalSec;
                                  root["CurrentMode"] = this->situation.CurrentMode;
                                  root["CurrentIsSchedule"] = this->situation.CurrentIsSchedule;
                                  root["CurrentIsExternal"] = this->situation.CurrentIsExternal;
                                  root["CurrentIsActive"] = this->situation.CurrentIsActive;

                                  root["PrevBeginTime"] = this->situation.PrevBeginTime;
                                  root["PrevEndTime"] = this->situation.PrevEndTime;
                                  root["PrevBeamedSec"] = this->situation.PrevBeamedSec;
                                  root["PrevTotalSec"] = this->situation.PrevTotalSec;
                                  root["PrevMode"] = this->situation.PrevMode;

                                  root["SchBeginTime"] = this->situation.SchBeginTime;
                                  root["SchEndTime"] = this->situation.SchEndTime;
                                  root["SchTotalSec"] = this->situation.SchTotalSec;
                                  root["SchMode"] = this->situation.SchMode;
                                  root["SchIsAborted"] = this->situation.SchIsAborted;

                                  root["NextBeginTime"] = this->situation.NextBeginTime;
                                  root["NextEndTime"] = this->situation.NextEndTime;
                                  root["NextTotalSec"] = this->situation.NextTotalSec;
                                  root["NextMode"] = this->situation.NextMode;
                                });
      }

      FactoryMode factoryMode = FactoryMode::CLOUD;
      ConSettings con;
      RadiationSettings radiation;
      ModeSettings mode;
      SituationInfo situation;
      bool isWorking = false;
    };

  }
}
