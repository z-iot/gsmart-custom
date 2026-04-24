#pragma once

#include <vector>
#include "esphome/core/helpers.h"
#include "settings_base.h"
#include <ctime>
#include "global.h"
#include "util.h"
#include "esphome/core/log.h"

// #define FACTORY_SCHEDULE_ENABLED false
#define SCHEDULE_SETTINGS_FILE "/settings/schedule.json"

namespace esphome
{
  namespace storage
  {
    // {"enabled"=false, "frames"=[
    //   {
    //     "f": "21:00",
    //     "t": "23:00",
    //     "d": 5,
    //   },
    //   {
    //     "f": "20:30",
    //     "t": "22:00",
    //     "d": 6,
    //     "m": "M"
    //   }
    // ]}

    // {
    // 	"d": 2,
    // 	"m": "M",   // m - min, M - max, s - std
    // 	"f": "10:30",
    // 	"t": "11:00"
    // },

    class SettingsSchedule : public SettingsBase
    {
    public:
      bool enabled = false;
      std::vector<ScheduleItem> schedule = {};

      SettingsSchedule()
      {
        fileName = SCHEDULE_SETTINGS_FILE;
      }

      void toJson(JsonObject &root) override
      {
        if (!this->enabled)
          root["enabled"] = this->enabled;

        JsonArray arr = root.createNestedArray("frames");
        for (auto &item : this->schedule)
        {
          JsonObject arrItem = arr.createNestedObject();
          arrItem["d"] = convertWeekDayFromUsaToSk(item.day);
          if (item.mode == ScheduleMode::MIN)
            arrItem["m"] = "m";
          else if (item.mode == ScheduleMode::MAX)
            arrItem["m"] = "M";
          // else
          //   arrItem["m"] = "s";
          arrItem["f"] = convertTimeToString(item.from);
          arrItem["t"] = convertTimeToString(item.to);
        }
      }

      StateUpdateResult fromJson(JsonObject &root) override
      {
        if (root["enabled"] == false)
          this->enabled = false;
        else
          this->enabled = true;
        this->schedule.clear();

        if (root["frames"].is<JsonArray>())
        {
          for (JsonVariant item : root["frames"].as<JsonArray>())
          {
            ScheduleItem itemData;
            itemData.day = convertWeekDayFromSkToUsa(item["d"]);
            if (item["m"] == "m")
              itemData.mode = ScheduleMode::MIN;
            else if (item["m"] == "M")
              itemData.mode = ScheduleMode::MAX;
            else
              itemData.mode = ScheduleMode::STD;
            itemData.from = convertStringToTime(item["f"]);
            itemData.to = convertStringToTime(item["t"]);
            this->schedule.push_back(itemData);
          }
        }
        sort();
        return StateUpdateResult::CHANGED;
      }

      static std::string convertTimeToString(ScheduleTime time)
      {
        return str_snprintf("%02d:%02d", 5, time.hour, time.minute);
      }

      static ScheduleTime convertStringToTime(std::string strTime)
      {
        ScheduleTime time;
        if (strTime.length() == 5)
        {
          auto hour = parse_number<uint8_t>(strTime.substr(0, 2));
          auto minute = parse_number<uint8_t>(strTime.substr(3, 2));
          time.hour = *hour;
          time.minute = *minute;
        }
        else
        {
          time.hour = 0;
          time.minute = 0;
        }
        return time;
      }

      void sort()
      {
        std::sort(this->schedule.begin(), this->schedule.end(), [](ScheduleItem &a, ScheduleItem &b)
                  {
                    if (a.day < b.day)
                      return true;
                    if (a.day > b.day)
                      return false;
                    if (a.from.hour < b.from.hour)
                      return true;
                    if (a.from.hour > b.from.hour)
                      return false;
                    return a.from.minute < b.from.minute; });
      }

      // get current time
      tm *getTime(time_t now)
      {
        // time_t now = time(nullptr);
        return localtime(&now);
      }

      // Get the next scheduler item position
      int getNextScheduleItemPosition(time_t now_time)
      {
        tm *now = getTime(now_time);

        if (!this->enabled)
          return -1;

        // Find the next scheduler item
        for (int i = 0; i < this->schedule.size(); i++)
        {
          auto &item = this->schedule[i];
          if (item.day == now->tm_wday)
          {
            if (now->tm_hour < item.from.hour ||
                (now->tm_hour == item.from.hour && now->tm_min < item.from.minute))
              return i;
          }
          else if (item.day > now->tm_wday)
            return i;
        }
        return -1;
      }

      // Get the current scheduler item position
      int getCurrentScheduleItemPosition(time_t now_time)
      {
        tm *now = getTime(now_time);

        if (!this->enabled)
          return -1;

        // Find the next scheduler item
        for (int i = 0; i < this->schedule.size(); i++)
        {
          auto &item = this->schedule[i];
          // ESP_LOGI("radiation", "findPos curPos:%d, time=%d/%d:%d, item: d=%d from=%d:%d to=%d:%d", i, now->tm_wday, now->tm_hour, now->tm_min, item.day, item.from.hour, item.from.minute, item.to.hour, item.to.minute);
          if (item.day == now->tm_wday)
          {
            if ((now->tm_hour > item.from.hour || (now->tm_hour == item.from.hour && now->tm_min >= item.from.minute)) &&
                (now->tm_hour < item.to.hour || (now->tm_hour == item.to.hour && now->tm_min < item.to.minute)))
              return i;
          }
        }
        return -1;
      }

      // Calculate the time in sec end of event
      uint32_t timeUntilEndOfEvent(ScheduleItem *item, time_t now_time)
      {
        tm *now = getTime(now_time);

        if (item == nullptr)
          return -1; // No more scheduler events
        int days_until = item->day - now->tm_wday;
        if (days_until < 0)
        {
          days_until += 7;
        }
        int hours_until = item->to.hour - now->tm_hour;
        if (hours_until < 0)
        {
          days_until--;
          hours_until += 24;
        }
        int minutes_until = item->to.minute - now->tm_min;
        if (minutes_until < 0)
        {
          hours_until--;
          minutes_until += 60;
        }
        return days_until * 86400 + hours_until * 3600 + minutes_until * 60;
      }

      // Calculate the time in sec until event
      uint32_t timeUntilNextEvent(ScheduleItem *item, time_t now_time)
      {
        tm *now = getTime(now_time);

        if (item == nullptr)
          return -1; // No more scheduler events
        int days_until = item->day - now->tm_wday;
        if (days_until < 0)
        {
          days_until += 7;
        }
        int hours_until = item->from.hour - now->tm_hour;
        if (hours_until < 0)
        {
          days_until--;
          hours_until += 24;
        }
        int minutes_until = item->from.minute - now->tm_min;
        if (minutes_until < 0)
        {
          hours_until--;
          minutes_until += 60;
        }
        return days_until * 86400 + hours_until * 3600 + minutes_until * 60;
      }

      RadiationMode getCurrentRadiationMode(time_t now)
      {
        int curPos = getCurrentScheduleItemPosition(now);
        // ESP_LOGI("radiation", "getCurrentRadiationMode curPos:%d, schedule.size:%d, time=%d/%d:%d", curPos, this->schedule.size(), getTime(now)->tm_wday, getTime(now)->tm_hour, getTime(now)->tm_min);
        if (curPos == -1)
          return RadiationMode::NONE;
        return convertScheduleModeToRadiationMode(schedule[curPos].mode);
      };
    };
  }
}

// #include <ctime>

// int time_until_next_event(const std::vector<ScheduleItem>& items) {
//   // Get the current time
//   time_t current_time = time(nullptr);
//   tm* local_time = localtime(&current_time);

//   // Find the next scheduler item
//   ScheduleItem* next_item = nullptr;
//   for (const auto& item : items) {
//     if (item.day == local_time->tm_wday) {
//       if (local_time->tm_hour < item.from.hour ||
//           (local_time->tm_hour == item.from.hour && local_time->tm_min < item.from.minute)) {
//         next_item = &item;
//         break;
//       }
//     } else if (item.day > local_time->tm_wday) {
//       next_item = &item;
//       break;
//     }
//   }

//   // Calculate the time until the next scheduler event
//   if (next_item == nullptr) {
//     return -1;  // No more scheduler events
//   } else {
//     int days_until = next_item->day - local_time->tm_wday;
//     if (days_until < 0) {
//       days_until += 7;
//     }
//     int hours_until = next_item->from.hour - local_time->tm_hour;
//     if (hours_until < 0) {
//       days_until--;
//       hours_until += 24;
//     }
//     int minutes_until = next_item->from.minute - local_time->tm_min;
//     if (minutes_until < 0) {
//       hours_until--;
//       minutes_until += 60;
//     }
//     return days_until * 86400 + hours_until * 3600 + minutes_until * 60;
//   }
// }