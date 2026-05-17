#pragma once

// #ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include <cstring>
#include <vector>
#include "esphome/core/helpers.h"
#ifdef GSMART_FEATURE_SCHEDULE
#include "settings_schedule.h"
#endif

#ifdef GSMART_FEATURE_FILESYSTEM
#include "fileSystem.h"
#endif

#include "data_global.h"
#include "data_usage.h"
#include "data_region.h"
#include "global.h"
#ifdef GSMART_FEATURE_FILESYSTEM
#include "settings_mode.h"
#include "settings_device.h"
#endif
#include <esphome/components/logger/logger.h>

#include "esphome/components/wifi/wifi_component.h"
#ifdef USE_MQTT
#include "esphome/components/mqtt/mqtt_component.h"
#endif
#ifdef USE_ESP32
#include <esp_wifi.h>
#endif
#include "esphome/components/gsmart_wifi_manager/gsmart_wifi_manager.h"



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
    struct FactoryResetResult
    {
      bool preferencesCleared{false};
      bool filesystemCleared{false};
      bool rebootScheduled{false};
      uint32_t delayMs{0};
    };

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

      void set_firmware_version(const std::string &firmware_version)
      {
        this->_firmware_version = firmware_version;
      }

      void getBuildNumber(uint8_t &hi, uint8_t &lo) const;

      const std::string get_model() { return this->_model; }
      uint8_t get_model_num() { return this->_model_num; }
      const std::string get_serial() { return esphome::get_mac_address().substr(6); }
      FactoryResetResult factory_reset(uint32_t reboot_delay_ms = 750);

      void setRadiationCause(RadiationCauseKind kind, const std::string &detail = "")
      {
        this->pending_radiation_cause_ = this->makeLocalRadiationCause_(kind, detail);
      }

      void setRadiationCauseFromRemote(RadiationCause cause, RadiationCauseKind fallback_kind,
                                       const uint8_t origin_mac[6] = nullptr)
      {
        if (cause.kind == RadiationCauseKind::UNKNOWN)
          cause.kind = fallback_kind;
        if (cause.detail[0] == 0)
          this->copyString_(cause.detail, sizeof(cause.detail), radiationCauseKindToApi(cause.kind));
        if (this->macIsEmpty_(cause.originMac) && origin_mac != nullptr)
          memcpy(cause.originMac, origin_mac, sizeof(cause.originMac));
        this->pending_radiation_cause_ = cause;
      }

      const RadiationCause &getLastRadiationCause() const { return this->global->radiation.lastCause; }

      bool isScheduleAuthority() const
      {
#ifdef GSMART_FEATURE_REGION
        if (this->region != nullptr && this->region->isRegionActive())
          return this->region->isMaster();
#endif
        return true;
      }

      void notifySituationChange()
      {
        this->situation_change_callback_.call();
      }
      
      void set_wifi_ap_active(bool active) {
        if (gsmart_wifi_manager::global_gsmart_wifi_manager != nullptr) {
          gsmart_wifi_manager::global_gsmart_wifi_manager->set_service_ap_runtime(active);
        }
      }

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

      std::string getSituationStatus(int egmode, int setup_mode, bool is_service_hotspot = false)
      {
        std::string res = "";
        if (!this->global->isWorking)
          res += "Z";

        if (egmode == 1)
          res += "D"; // dodo
        if (egmode == 2)
          res += "O"; // offline

#ifdef USE_MQTT
        if (mqtt::global_mqtt_client != nullptr && mqtt::global_mqtt_client->is_connected())
          // mqtt server
          res += "Q";
        else
#endif
        if (wifi::global_wifi_component->is_connected())
        {
          // wifi
          if (is_service_hotspot)
            res += "H"; // Hotspot service icon/mode
          else
            res += "W";
        }

        bool ap_active = esphome::wifi::global_wifi_component->is_ap_active();
        if (gsmart_wifi_manager::global_gsmart_wifi_manager != nullptr)
          ap_active = gsmart_wifi_manager::global_gsmart_wifi_manager->is_ap_active();
        if (ap_active)
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
        {
          std::string begin_time = convertSituationSecToTimeStr(situation.CurrentBeginTime);
          if (situation.CurrentMode == RadiationMode::ON)
            return begin_time + " - ";
          return begin_time + " - " + convertSituationSecToTimeStr(situation.CurrentEndTime);
        }
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
        if (!this->isScheduleAuthority())
          return false;

        int curPos = this->schedule->getCurrentScheduleItemPosition(now);
        if (curPos == -1)
        {
          if (situation.SchBeginTime != 0 || situation.SchEndTime != 0 || situation.SchTotalSec != 0 || situation.SchMode != RadiationMode::OFF)
            change = false;
          situation.SchBeginTime = 0;
          situation.SchEndTime = 0;
          situation.SchTotalSec = 0;
          situation.SchMode = RadiationMode::OFF;
        }
        else
        {
          auto schBeginTime = convertFromScheduleTimeToSituationSec(this->schedule->schedule[curPos].day, this->schedule->schedule[curPos].from);
          auto schEndTime = convertFromScheduleTimeToSituationSec(this->schedule->schedule[curPos].day, this->schedule->schedule[curPos].to);
          auto schMode = convertScheduleModeToRadiationMode(this->schedule->schedule[curPos].mode);
          uint16_t schTotalSec = 0;
          if (this->schedule->schedule[curPos].radiateMinutes > 0) {
            schTotalSec = this->schedule->schedule[curPos].radiateMinutes * 60;
          } else if (schMode == RadiationMode::ON) {
            schTotalSec = schEndTime - schBeginTime;
          } else {
            schTotalSec = getDurationForScheduleMode(schMode);
          }
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
          if (situation.NextBeginTime != 0 || situation.NextEndTime != 0 || situation.NextTotalSec != 0 || situation.NextMode != RadiationMode::OFF)
            change = false;
          situation.NextBeginTime = 0;
          situation.NextEndTime = 0;
          situation.NextTotalSec = 0;
          situation.NextMode = RadiationMode::OFF;
        }
        else
        {
          auto nextBeginTime = convertFromScheduleTimeToSituationSec(this->schedule->schedule[nextPos].day, this->schedule->schedule[nextPos].from);
          auto nextEndTime = convertFromScheduleTimeToSituationSec(this->schedule->schedule[nextPos].day, this->schedule->schedule[nextPos].to);
          auto nextMode = convertScheduleModeToRadiationMode(this->schedule->schedule[nextPos].mode);
          uint16_t nextTotalSec = 0;
          if (this->schedule->schedule[nextPos].radiateMinutes > 0) {
            nextTotalSec = this->schedule->schedule[nextPos].radiateMinutes * 60;
          } else if (nextMode == RadiationMode::ON) {
            nextTotalSec = nextEndTime - nextBeginTime;
          } else {
            nextTotalSec = getDurationForScheduleMode(nextMode);
          }
          if (situation.NextBeginTime != nextBeginTime || situation.NextEndTime != nextEndTime || situation.NextTotalSec != nextTotalSec || situation.NextMode != nextMode)
            change = true;
          situation.NextBeginTime = nextBeginTime;
          situation.NextEndTime = nextEndTime;
          situation.NextTotalSec = nextTotalSec;
          situation.NextMode = nextMode;
        }
#else
          if (situation.SchBeginTime != 0 || situation.SchEndTime != 0 || situation.SchTotalSec != 0 || situation.SchMode != RadiationMode::OFF)
            change = false;
          situation.SchBeginTime = 0;
          situation.SchEndTime = 0;
          situation.SchTotalSec = 0;
          situation.SchMode = RadiationMode::OFF;
          if (situation.NextBeginTime != 0 || situation.NextEndTime != 0 || situation.NextTotalSec != 0 || situation.NextMode != RadiationMode::OFF)
            change = false;
          situation.NextBeginTime = 0;
          situation.NextEndTime = 0;
          situation.NextTotalSec = 0;
          situation.NextMode = RadiationMode::OFF;          
#endif

        if (change)
          situation.SchIsAborted = false;
        return change;
      }

      void setActiveRadiationMode(time_t now, RadiationMode mode, RadiationSource source)
      {
        SituationInfo &situation = this->global->situation;
        RadiationCause cause = this->consumeRadiationCause_(source);
        if (this->global->radiation.activeMode == mode)
        {
          this->global->radiation.lastCause = cause;
          return;
        }

        uint32_t nowMs = millis() / 1000;
        if (mode != RadiationMode::OFF)
        {
          // radiation start
          this->global->radiation.lastStart = nowMs;
          situation.CurrentIsActive = true;
          situation.SchIsAborted = false;
          situation.CurrentIsExternal = source == RadiationSource::EXT || source == RadiationSource::REGION;
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
        this->global->radiation.lastCause = cause;

#ifdef GSMART_FEATURE_SCHEDULE
        situation.SchedulerActive = this->schedule->enabled;
        situation.SchedulerItemsCount = this->schedule->schedule.size();
#else
        situation.SchedulerActive = false;
        situation.SchedulerItemsCount = 0;
#endif
        situation.source = source;

        this->situation_change_callback_.call();
        this->radiation_applied_callback_.call(mode, source);

        ESP_LOGW("store", "radiation: total: %d, now: %d, beamBegin: %d, beamed: %d", situation.CurrentTotalSec, convertFromEspTimeToSituationSec(now), situation.BeamBeginTime, situation.CurrentBeamedSec);
      }

      RadiationMode getCurrentScheduleRadiation(time_t now)
      {
        SituationInfo &situation = this->global->situation;

        // check max guardDuration
        if (this->global->isGuardDurationOverflow())
          return RadiationMode::OFF;

        if (situation.CurrentIsActive && situation.CurrentMode != RadiationMode::ON && this->getTimerDurationSec(now) == 0)
        {
          situation.SchIsAborted = true;
          return RadiationMode::OFF;
        }

#ifdef GSMART_FEATURE_SCHEDULE
        // check schedule
        if (this->schedule->enabled && !situation.SchIsAborted &&
            this->isScheduleAuthority() &&
            (this->global->radiation.activeMode == RadiationMode::OFF || (this->global->radiation.activeMode != RadiationMode::OFF && this->global->radiation.lastSource == RadiationSource::SCH)))
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

        auto m = this->getCurrentScheduleRadiation(now);
        if (m != this->global->radiation.activeMode)
        {
          ESP_LOGI("radiation", "Change mode from scheduler to %s", convertRadiationModeToStr(m).c_str());
          this->setRadiationCause(RadiationCauseKind::SCHEDULER, "scheduler");
          this->change_radiation_mode_callback_.call(m);
        }

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
      void add_on_radiation_applied(std::function<void(RadiationMode, RadiationSource)> &&callback) { this->radiation_applied_callback_.add(std::move(callback)); }

#ifdef GSMART_FEATURE_FILESYSTEM
      FileSystem *file_system_ = nullptr;
#endif
#ifdef GSMART_FEATURE_SCHEDULE
      SettingsSchedule *schedule = nullptr;
#endif
#ifdef GSMART_FEATURE_REGION
      DataRegion *region = new DataRegion();
#endif
#ifdef GSMART_FEATURE_USAGE
      DataUsage *usage = new DataUsage();
#endif
      DataGlobal *global = new DataGlobal();
#ifdef GSMART_FEATURE_FILESYSTEM
      SettingsMode *settingsMode = new SettingsMode();
      SettingsDevice *settingsDevice = new SettingsDevice();
#endif

    protected:
      std::string _model;
      std::string _firmware_version;
      uint8_t _model_num;
      std::string lastSituationDuration = "";
      RadiationCause pending_radiation_cause_{};

      CallbackManager<void()> situation_change_callback_{};
      CallbackManager<void()> situation_duration_change_callback_{};
      CallbackManager<void(RadiationMode)> change_radiation_mode_callback_{};
      CallbackManager<void(RadiationMode, RadiationSource)> radiation_applied_callback_{};

      static void copyString_(char *target, size_t target_size, const std::string &value)
      {
        if (target_size == 0)
          return;
        strncpy(target, value.c_str(), target_size - 1);
        target[target_size - 1] = 0;
      }

      static bool macIsEmpty_(const uint8_t mac[6])
      {
        for (int i = 0; i < 6; i++)
          if (mac[i] != 0)
            return false;
        return true;
      }

      RadiationCause makeLocalRadiationCause_(RadiationCauseKind kind, const std::string &detail)
      {
        RadiationCause cause{};
        cause.kind = kind;
        this->copyString_(cause.detail, sizeof(cause.detail), detail.empty() ? radiationCauseKindToApi(kind) : detail);
        get_mac_address_raw(cause.originMac);
        this->copyString_(cause.originSerial, sizeof(cause.originSerial), this->get_serial());
        this->copyString_(cause.originModel, sizeof(cause.originModel), this->get_model());
        return cause;
      }

      RadiationCause makeDefaultRadiationCause_(RadiationSource source)
      {
        switch (source)
        {
        case RadiationSource::SCH:
          return this->makeLocalRadiationCause_(RadiationCauseKind::SCHEDULER, "scheduler");
        case RadiationSource::REGION:
          return this->makeLocalRadiationCause_(RadiationCauseKind::UDP_CONTROL, "region");
        case RadiationSource::EXT:
          return this->makeLocalRadiationCause_(RadiationCauseKind::MOBILE_API, "mobile_api");
        case RadiationSource::INT:
        default:
          return this->makeLocalRadiationCause_(RadiationCauseKind::BUTTON, "local");
        }
      }

      RadiationCause consumeRadiationCause_(RadiationSource source)
      {
        RadiationCause cause = this->pending_radiation_cause_;
        this->pending_radiation_cause_ = RadiationCause{};
        if (cause.kind == RadiationCauseKind::UNKNOWN)
          cause = this->makeDefaultRadiationCause_(source);
        return cause;
      }
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

    class RadiationAppliedTrigger : public Trigger<RadiationMode, RadiationSource>
    {
    public:
      explicit RadiationAppliedTrigger(Store *parent)
      {
        parent->add_on_radiation_applied([this](RadiationMode mode, RadiationSource source)
                                         { this->trigger(mode, source); });
      }
    };

    extern Store *store; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
  }
}

// #endif  // USE_ARDUINO
