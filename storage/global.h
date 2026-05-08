#pragma once

#include <Arduino.h>
#include <string>
namespace esphome
{
  namespace storage
  {

    enum RadiationMode
    {
      OFF = 0,
      MIN = 1,
      STD = 2,
      MAX = 3,
      ON = 4,
    };

    enum class ScheduleMode
    {
      MIN = 'm',
      STD = 's',
      MAX = 'M',
      ON = 'o',
    };

    enum class FactoryMode
    {
      SILLY = 1,
      PHOENIX = 2,
      CLOUD = 3,
    };

    inline uint16_t getDurationForScheduleMode(RadiationMode mode)
    {
      switch (mode)
      {
      case RadiationMode::MIN:
        return 30 * 60;
      case RadiationMode::STD:
        return 60 * 60;
      case RadiationMode::MAX:
        return 90 * 60;
      case RadiationMode::ON:
        return 0xFFFF; // Approximately 18 hours or handle as infinite
      default:
        return 0;
      }
    }

    enum LampMode
    {
      Top = 1,
      Bottom = 2,
      Alternate = 90,
      All = 99,
    };

    enum MotionMode
    {
      None = 0,
      Active = 1,
      Inactive = 2,
    };

    struct ModeItem
    {
      LampMode lampMode;
      uint16_t fanSpeed;
      MotionMode motionMode;

      uint16_t motionDetectionDuration;
      uint16_t motionRadiateDuration;

      uint16_t maxDayDuration;
      uint16_t totalDuration;
      bool extendDurationByMotion;
    };

    struct ModeSettings
    {
      ModeItem items[3];
    };


    struct NetworkSettings
    {
      std::string wifiSSID;
      std::string wifiPassword;
      std::string apSSID;
      std::string apPassword;

      // apEnable
      // timezone
    };

    struct DeviceSettings
    {
      // pin
      // locked
      // sleep
      // brightness
      // dimmable
      // sounds / silence

      // catalog
      // batch
      // patchPos
      // lampCount
      // lampPower

    };

    struct ScheduleTime
    {
      uint8_t hour;
      uint8_t minute;
    };

    struct ScheduleItem
    {
      uint8_t day;
      ScheduleMode mode;
      ScheduleTime from;
      ScheduleTime to;
      uint16_t radiateMinutes;
    };

    uint32_t convertFromEspTimeToSituationSec(time_t nowTime);
    uint32_t convertFromScheduleTimeToSituationSec(uint8_t day, ScheduleTime time);
    RadiationMode convertScheduleModeToRadiationMode(ScheduleMode mode);

    std::string convertSituationSecToTimeStr(uint32_t sec);
    std::string convertSituationSecToDurationStr(uint32_t sec, bool slow = false);

  }
}
