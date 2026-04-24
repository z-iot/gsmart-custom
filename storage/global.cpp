#include "global.h"

#include <ctime>

namespace esphome
{
  namespace storage
  {

    uint32_t convertFromEspTimeToSituationSec(time_t nowTime)
    {
      tm *now = localtime(&nowTime);
      return now->tm_wday * 24 * 60 * 60 + now->tm_hour * 60 * 60 + now->tm_min * 60 + now->tm_sec;
    }

    uint32_t convertFromScheduleTimeToSituationSec(uint8_t day, ScheduleTime time)
    {
      return (day * 24 * 60 * 60) + (time.hour * 60 * 60) + (time.minute * 60);
    }

    RadiationMode convertScheduleModeToRadiationMode(ScheduleMode mode)
    {
      switch (mode)
      {
      case ScheduleMode::MIN:
        return RadiationMode::MIN;
      case ScheduleMode::MAX:
        return RadiationMode::MAX;
      default:
        return RadiationMode::STD;
      }
    }

    // uint32_t convertScheduleItemToSituationSec(ScheduleItem *item)
    // {
    //   return item->day*24*36000 + item->from.hour * 3600 + item->from.minute * 60;
    // }

    std::string convertSituationSecToTimeStr(uint32_t sec)
    {
      uint8_t day = sec / (24 * 3600);
      sec = sec % (24 * 3600);
      uint8_t hour = sec / 3600;
      sec %= 3600;
      uint8_t minutes = sec / 60;
      sec %= 60;
      uint8_t seconds = sec;

      char buffer[20];
      sprintf(buffer, "%02d:%02d", hour, minutes);
      return std::string(buffer);
    }

    std::string convertSituationSecToDurationStr(uint32_t sec, bool slow)
    {
      uint8_t day = sec / (24 * 3600);
      sec = sec % (24 * 3600);
      uint8_t hour = sec / 3600;
      sec %= 3600;
      uint8_t minutes = sec / 60;
      sec %= 60;
      uint8_t seconds = sec;

      char buffer[20];
      if (day > 0)
        sprintf(buffer, "%dd %02dh", day, hour);
      else if (hour > 0)
        sprintf(buffer, "%02dh %02dm", hour, minutes);
      else if (slow)
      {
        // pomaly display epaper
        if (minutes > 0)
          sprintf(buffer, "%02dm", minutes);
        else if (seconds > 30)
          sprintf(buffer, "<1m");
        else if (seconds > 15)
          sprintf(buffer, "<30s");
        else if (seconds > 5)
          sprintf(buffer, "<15s");
        else if (seconds > 0)
          sprintf(buffer, "<5s");
      }
      else if (minutes > 0)
        sprintf(buffer, "%02dm %02ds", minutes, seconds);
      else
        sprintf(buffer, "%02ds", seconds);
      return std::string(buffer);
    }

  }
}
