#pragma once

#include <Arduino.h>
#include <string>
namespace esphome
{
  namespace storage
  {

    enum RadiationMode
    {
      NONE = 0,
      MIN = 1,
      STD = 2,
      MAX = 3,
    };

    enum class ScheduleMode
    {
      MIN = 'm',
      STD = 's',
      MAX = 'M',
    };

    enum class FactoryMode
    {
      SILLY = 1,
      PHOENIX = 2,
      CLOUD = 3,
    };

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
    };

    uint32_t convertFromEspTimeToSituationSec(time_t nowTime);
    uint32_t convertFromScheduleTimeToSituationSec(uint8_t day, ScheduleTime time);
    RadiationMode convertScheduleModeToRadiationMode(ScheduleMode mode);

    std::string convertSituationSecToTimeStr(uint32_t sec);
    std::string convertSituationSecToDurationStr(uint32_t sec, bool slow = false);

  }
}
