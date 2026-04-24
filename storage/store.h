#pragma once

// #ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include <vector>
#include "esphome/core/helpers.h"
#include "settings_schedule.h"

#ifdef GSMART_FEATURE_FILESYSTEM
#include "fileSystem.h"
#endif

#include "data_global.h"
#include "data_usage.h"
#include "data_region.h"
#include "global.h"
#include <esphome/components/logger/logger.h>
#include <string>

#include "esphome/components/wifi/wifi_component.h"
#include "esphome/components/mqtt/mqtt_component.h"



#ifdef USE_ESP8266
// #include <esp_ota_get_partition_info.h>
#endif
#ifdef USE_ESP32
#include <esp_partition.h>
#endif

namespace esphome
{
  namespace storage
  {

    class Store : public Component
    {
    public:
      Store();

      void setup() override;
      void dump_config() override;
      float get_setup_priority() const override;
      void loop() override;

      void set_model(const std::string &model)
      {
        this->_model = model;
        this->_model_num = convertModelToNum(model);
      }

      void getBuildNumber(uint8_t &hi, uint8_t &lo)
      {
        // auto compileTime = App.get_compilation_time();

        hi = 4;
        lo = 5;
      }

      const std::string get_model() { return this->_model; }
      uint8_t get_model_num() { return this->_model_num; }
      const std::string get_serial() { return esphome::get_mac_address().substr(6); }

      void mqttConnect()
      {
        global->con.lastConnect = millis() / 1000;
        global->con.disconnectSecLast = global->con.lastConnect - global->con.lastDisconnect;
        global->con.disconnectSecTotal += global->con.disconnectSecLast;
      }

      void mqttDisconnect()
      {
        global->con.disconnectCount++;
        global->con.lastDisconnect = millis() / 1000;
      }

      uint16_t getDurationForScheduleMode(RadiationMode mode)
      {
        switch (mode)
        {
        case RadiationMode::MIN:
          return 15 * 60;
        case RadiationMode::STD:
          return 30 * 60;
        case RadiationMode::MAX:
          return 60 * 60;
        default:
          return 0;
        }
      }

      uint32_t getTimerDurationSec(time_t now)
      {
        SituationInfo &situation = this->global->situation;
        if (!situation.CurrentIsActive)
          return 0;
        int32_t durationBeamed = convertFromEspTimeToSituationSec(now) - situation.BeamBeginTime;
        int32_t duration = 0;
        if (situation.BeamEndTime >= situation.BeamBeginTime)
          duration = situation.CurrentTotalSec - situation.CurrentBeamedSec;
        else
          duration = situation.CurrentTotalSec - situation.CurrentBeamedSec - durationBeamed;
        if (duration < 0)
          duration = 0;
        return duration;
      }

      std::string getTimerDuration(time_t now)
      {
        auto sec = this->getTimerDurationSec(now);
        if (sec == 0)
          return ": : :";
        if (this->get_model_num() == 11 || this->get_model_num() == 52) // sibra alebo panel
          return convertSituationSecToDurationStr(sec, true);
        return convertSituationSecToDurationStr(sec);
      }

      std::string getSituationStatus(int egmode, int setup_mode)
      {
        std::string res = "";
        if (!this->global->isWorking)
          res += "Z";

        if (egmode == 1)
          res += "D"; // dodo
        if (egmode == 2)
          res += "O"; // offline

        if (mqtt::global_mqtt_client->is_connected())
          // mqtt server
          res += "Q";
        else if (wifi::global_wifi_component->is_connected())
          // wifi
          res += "W";

        if (wifi::global_wifi_component->isApActive())
          // AP
          res += "A";

#ifdef GSMART_FEATURE_REGION        
        // if (this->region->isRegionActive())
        // {
        //   if (this->region->isMaster())
        //     res += "M";
        //   else
        //     res += "S";
        // }
        // else
        // {
        //   res += "";
        // };
#endif        
        if (this->global->situation.CurrentIsActive)
        {
          if (this->global->situation.CurrentIsSchedule)
            res += "1";
          else
            res += "2";
        }
#ifdef GSMART_FEATURE_SCHEDULE
        if (this->schedule->enabled)
          res += "E";
        else
          res += "";
#endif

        if (setup_mode == 1)
          res += "P"; // production
        if (setup_mode == 2)
          res += "R"; // manufacture

        return res;
      }

      std::string getTimerInterval(time_t now)
      {
        SituationInfo &situation = this->global->situation;
        if (situation.CurrentIsActive)
          return convertSituationSecToTimeStr(situation.CurrentBeginTime) + " - " + convertSituationSecToTimeStr(situation.CurrentEndTime);
        return "";
      }

      std::string getTimerIntervalPrev(time_t now)
      {
        SituationInfo &situation = this->global->situation;
        if (situation.PrevBeginTime > 0 && situation.PrevEndTime > 0)
          return convertSituationSecToTimeStr(situation.PrevBeginTime) + " - " + convertSituationSecToTimeStr(situation.PrevEndTime) + " (" + convertSituationSecToDurationStr(situation.PrevBeamedSec) + ")";
        return "";
      }

      std::string getTimerIntervalNext(time_t now)
      {
        SituationInfo &situation = this->global->situation;
        if (situation.NextBeginTime > 0 && situation.NextEndTime > 0)
          return convertSituationSecToTimeStr(situation.NextBeginTime) + " - " + convertSituationSecToTimeStr(situation.NextEndTime) + " (" + convertSituationSecToDurationStr(situation.NextTotalSec) + ")";
        return "";
      }

#ifdef GSMART_EMITTER
      void updateLamp(time_t now, uint8_t lampNum, bool on)
      {
        uint8_t lampCountOld = this->usage->lampOnCount();
        this->usage->updateLamp(lampNum, on);
        uint8_t lampCountNew = this->usage->lampOnCount();

        SituationInfo &situation = this->global->situation;
        if (lampCountOld == 0 && lampCountNew > 0)
        {
          // turn on
          situation.BeamBeginTime = convertFromEspTimeToSituationSec(now);
        }
        else if (lampCountOld > 0 && lampCountNew == 0)
        {
          // turn off
          situation.BeamEndTime = convertFromEspTimeToSituationSec(now);
          situation.CurrentBeamedSec += situation.BeamEndTime - situation.BeamBeginTime;
        }
        this->situation_change_callback_.call();
      }
#endif

      bool fillSituationSchedule(time_t now)
      {
        SituationInfo &situation = this->global->situation;
        bool change = false;

#ifdef GSMART_FEATURE_SCHEDULE
        int curPos = this->schedule->getCurrentScheduleItemPosition(now);
        if (curPos == -1)
        {
          if (situation.SchBeginTime != 0 || situation.SchEndTime != 0 || situation.SchTotalSec != 0 || situation.SchMode != RadiationMode::NONE)
            change = false;
          situation.SchBeginTime = 0;
          situation.SchEndTime = 0;
          situation.SchTotalSec = 0;
          situation.SchMode = RadiationMode::NONE;
        }
        else
        {
          auto schBeginTime = convertFromScheduleTimeToSituationSec(this->schedule->schedule[curPos].day, this->schedule->schedule[curPos].from);
          auto schEndTime = convertFromScheduleTimeToSituationSec(this->schedule->schedule[curPos].day, this->schedule->schedule[curPos].to);
          auto schTotalSec = getDurationForScheduleMode(convertScheduleModeToRadiationMode(this->schedule->schedule[curPos].mode));
          auto schMode = convertScheduleModeToRadiationMode(this->schedule->schedule[curPos].mode);
          if (situation.SchBeginTime != schBeginTime || situation.SchEndTime != schEndTime || situation.SchTotalSec != schTotalSec || situation.SchMode != schMode)
            change = true;
          situation.SchBeginTime = schBeginTime;
          situation.SchEndTime = schEndTime;
          situation.SchTotalSec = schTotalSec;
          situation.SchMode = schMode;
        }

        int nextPos = this->schedule->getNextScheduleItemPosition(now);
        if (nextPos == -1)
        {
          if (situation.NextBeginTime != 0 || situation.NextEndTime != 0 || situation.NextTotalSec != 0 || situation.NextMode != RadiationMode::NONE)
            change = false;
          situation.NextBeginTime = 0;
          situation.NextEndTime = 0;
          situation.NextTotalSec = 0;
          situation.NextMode = RadiationMode::NONE;
        }
        else
        {
          auto nextBeginTime = convertFromScheduleTimeToSituationSec(this->schedule->schedule[nextPos].day, this->schedule->schedule[nextPos].from);
          auto nextEndTime = convertFromScheduleTimeToSituationSec(this->schedule->schedule[nextPos].day, this->schedule->schedule[nextPos].to);
          auto nextTotalSec = getDurationForScheduleMode(convertScheduleModeToRadiationMode(this->schedule->schedule[nextPos].mode));
          auto nextMode = convertScheduleModeToRadiationMode(this->schedule->schedule[nextPos].mode);
          if (situation.NextBeginTime != nextBeginTime || situation.NextEndTime != nextEndTime || situation.NextTotalSec != nextTotalSec || situation.NextMode != nextMode)
            change = true;
          situation.NextBeginTime = nextBeginTime;
          situation.NextEndTime = nextEndTime;
          situation.NextTotalSec = nextTotalSec;
          situation.NextMode = nextMode;
        }
#else
          if (situation.SchBeginTime != 0 || situation.SchEndTime != 0 || situation.SchTotalSec != 0 || situation.SchMode != RadiationMode::NONE)
            change = false;
          situation.SchBeginTime = 0;
          situation.SchEndTime = 0;
          situation.SchTotalSec = 0;
          situation.SchMode = RadiationMode::NONE;
          if (situation.NextBeginTime != 0 || situation.NextEndTime != 0 || situation.NextTotalSec != 0 || situation.NextMode != RadiationMode::NONE)
            change = false;
          situation.NextBeginTime = 0;
          situation.NextEndTime = 0;
          situation.NextTotalSec = 0;
          situation.NextMode = RadiationMode::NONE;          
#endif

        if (change)
          situation.SchIsAborted = false;
        return change;
      }

      void setActiveRadiationMode(time_t now, RadiationMode mode, RadiationSource source)
      {
        SituationInfo &situation = this->global->situation;
        if (this->global->radiation.activeMode == mode)
          return;

        uint32_t nowMs = millis() / 1000;
        if (mode != RadiationMode::NONE)
        {
          // radiation start
          this->global->radiation.lastStart = nowMs;
          situation.CurrentIsActive = true;
          situation.SchIsAborted = false;
          situation.CurrentIsExternal = source == RadiationSource::EXT;
          situation.BeamBeginTime = convertFromEspTimeToSituationSec(now);
          situation.BeamEndTime = convertFromEspTimeToSituationSec(now);
          situation.CurrentBeamedSec = 0;
          if (source != RadiationSource::SCH)
          {
            // radiation manual start
            situation.CurrentIsSchedule = false;
            situation.CurrentTotalSec = getDurationForScheduleMode(mode);
            situation.CurrentBeginTime = convertFromEspTimeToSituationSec(now);
            situation.CurrentEndTime = situation.CurrentBeginTime + situation.CurrentTotalSec;
            situation.CurrentMode = mode;
          }
          else
          {
            // radiation schedule start
            situation.CurrentIsSchedule = true;
            situation.CurrentTotalSec = situation.SchTotalSec;
            situation.CurrentBeginTime = situation.SchBeginTime;
            situation.CurrentEndTime = situation.SchEndTime;
            situation.CurrentMode = mode;
            ;
          }
        }
        else
        {
          // radiation end
          this->global->radiation.lastStop = nowMs;
          situation.PrevBeginTime = situation.CurrentBeginTime;
          situation.PrevEndTime = convertFromEspTimeToSituationSec(now);
          if (situation.BeamEndTime >= situation.BeamBeginTime)
            situation.PrevBeamedSec = situation.CurrentBeamedSec;
          else
            situation.PrevBeamedSec = situation.CurrentBeamedSec + convertFromEspTimeToSituationSec(now) - situation.BeamBeginTime;
          situation.PrevTotalSec = situation.CurrentTotalSec;
          situation.PrevMode = situation.CurrentMode;
          situation.CurrentIsActive = false;
          if (source != RadiationSource::SCH)
          {
            // radiation manual end
            situation.SchIsAborted = true;
          }
          else
          {
            // radiation schedule end
          }
        }

        this->global->radiation.lastSource = source;
        this->global->radiation.activeMode = mode;

#ifdef GSMART_FEATURE_SCHEDULE
        situation.SchedulerActive = this->schedule->enabled;
        situation.SchedulerItemsCount = this->schedule->schedule.size();
#else
        situation.SchedulerActive = false;
        situation.SchedulerItemsCount = 0;
#endif
        situation.source = source;

        this->situation_change_callback_.call();

        ESP_LOGW("store", "radiation: total: %d, now: %d, beamBegin: %d, beamed: %d", situation.CurrentTotalSec, convertFromEspTimeToSituationSec(now), situation.BeamBeginTime, situation.CurrentBeamedSec);
      }

      RadiationMode getCurrentScheduleRadiation(time_t now)
      {
        SituationInfo &situation = this->global->situation;

        // check max guardDuration
        if (this->global->isGuardDurationOverflow())
          return RadiationMode::NONE;

        if (situation.CurrentIsActive && this->getTimerDurationSec(now) == 0)
        {
          situation.SchIsAborted = true;
          return RadiationMode::NONE;
        }

#ifdef GSMART_FEATURE_SCHEDULE
        // check schedule
        if (this->schedule->enabled && !situation.SchIsAborted &&
            ((this->region->isRegionActive() && this->region->isMaster()) || (!this->region->isRegionActive())) &&
            (this->global->radiation.activeMode == RadiationMode::NONE || (this->global->radiation.activeMode != RadiationMode::NONE && this->global->radiation.lastSource == RadiationSource::SCH)))
          return schedule->getCurrentRadiationMode(now);
#endif
        return this->global->radiation.activeMode;
      }

      bool isSituationActive()
      {
        return this->global->isWorking && this->global->situation.CurrentIsActive;
      }

      void interval1sec(time_t now)
      {
        if (!this->global->isWorking)
          return;
        // auto radiation = getCurrentScheduleRadiation(now);
        // porovnat a aktualnym a zmenit ak treba (this->global->radiation.activeMode)

        if (this->fillSituationSchedule(now))
        {
          this->situation_change_callback_.call();
          // TODO change active schedule radiation
        }

#ifdef GSMART_EMITTER
        auto m = this->getCurrentScheduleRadiation(now);
        if (m != this->global->radiation.activeMode)
        {
          ESP_LOGI("radiation", "Change mode from scheduler to %s", convertRadiationModeToStr(m).c_str());
          this->change_radiation_mode_callback_.call(m);
        }
#endif

        if (this->isSituationActive())
        {
          auto durationStr = this->getTimerDuration(now);
          if (this->lastSituationDuration != durationStr)
          {
            this->situation_duration_change_callback_.call();
            this->lastSituationDuration = durationStr;
          }
        }
      }

      void interval1hour(time_t now)
      {
        if (!this->global->isWorking)
          return;
      }

      void logPartitionsInfo()
      {
#ifdef USE_ESP8266
        ESP_LOGI("store", "FreeSketchSpace: %d, SketchSize: %d, FlashChipRealSize: %d", ESP.getFreeSketchSpace(), ESP.getSketchSize(), ESP.getFlashChipRealSize());
        // esp_ota_get_partition_info_t p;
        // int i = 0;
        // while (esp_ota_get_partition_info(i++, &p) == ESP_OK)
        // {
        //   ESP_LOGI("store", "Partition %d: %s, %d, %d, %d, %d", i, p->label, p->type, p->subtype, p->address, p->size);
        // }
#endif
#ifdef USE_ESP32
        ESP_LOGI("store", "FreeSketchSpace: %d, SketchSize: %d", ESP.getFreeSketchSpace(), ESP.getSketchSize());
        ESP_LOGI("store", "Partitions info:");
        int i = 0;
        esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
        while (it != NULL)
        {
          auto p = esp_partition_get(it);
          ESP_LOGI("store", "Partition %d: %s, %d, %d, %d, %d", i++, p->label, p->type, p->subtype, p->address, p->size);
          it = esp_partition_next(it);
        }
#endif
      }

      void add_on_situation_change(std::function<void()> &&callback) { this->situation_change_callback_.add(std::move(callback)); }
      void add_on_situation_duration_change(std::function<void()> &&callback) { this->situation_duration_change_callback_.add(std::move(callback)); }
      void add_on_change_radiation_mode(std::function<void(RadiationMode)> &&callback) { this->change_radiation_mode_callback_.add(std::move(callback)); }

#ifdef GSMART_FEATURE_FILESYSTEM
      FileSystem *fileSystem;
#endif
#ifdef GSMART_FEATURE_SCHEDULE
      SettingsSchedule *schedule;
#endif
#ifdef GSMART_FEATURE_REGION
      DataRegion *region = new DataRegion();
#endif
#ifdef GSMART_FEATURE_USAGE
      DataUsage *usage = new DataUsage();
#endif
      DataGlobal *global = new DataGlobal();

    protected:
      std::string _model;
      uint8_t _model_num;
      std::string lastSituationDuration = "";

      CallbackManager<void()> situation_change_callback_{};
      CallbackManager<void()> situation_duration_change_callback_{};
      CallbackManager<void(RadiationMode)> change_radiation_mode_callback_{};
    };

    class SituationChangeTrigger : public Trigger<>
    {
    public:
      explicit SituationChangeTrigger(Store *parent)
      {
        parent->add_on_situation_change([this]()
                                        { this->trigger(); });
      }
    };

    class SituationDurationChangeTrigger : public Trigger<>
    {
    public:
      explicit SituationDurationChangeTrigger(Store *parent)
      {
        parent->add_on_situation_duration_change([this]()
                                                 { this->trigger(); });
      }
    };

    class ChangeRadiationModeTrigger : public Trigger<RadiationMode>
    {
    public:
      explicit ChangeRadiationModeTrigger(Store *parent)
      {
        parent->add_on_change_radiation_mode([this](RadiationMode mode)
                                             { this->trigger(mode); });
      }
    };

    extern Store *store; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
  }
}

// #endif  // USE_ARDUINO
