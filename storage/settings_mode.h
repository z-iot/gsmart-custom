#pragma once

#include <vector>
#include "esphome/core/helpers.h"
#include "settings_base.h"
#include <ctime>
#include "global.h"
#include "util.h"
#include "esphome/core/log.h"

#define MODE_SETTINGS_FILE "/settings/mode.json"

namespace esphome
{
  namespace storage
  {
    class SettingsMode : public SettingsBase
    {
    public:
      ModeSettings modes;

      SettingsMode()
      {
        fileName = MODE_SETTINGS_FILE;
      }

      void toJsonOneMode(JsonObject &root, ModeItem &mode)
      {
        root["lampMode"] = mode.lampMode;
        root["fanSpeed"] = mode.fanSpeed;
        root["motionMode"] = mode.motionMode;
        root["motionDetectionDuration"] = mode.motionDetectionDuration;
        root["motionRadiateDuration"] = mode.motionRadiateDuration;
        root["maxDayDuration"] = mode.maxDayDuration;
        root["totalDuration"] = mode.totalDuration;
        root["extendDurationByMotion"] = mode.extendDurationByMotion;
      }

      void toJson(JsonObject &root) override
      {
        JsonObject modeItem = root.createNestedObject("min");
        toJsonOneMode(modeItem, modes.items[0]);
        modeItem = root.createNestedObject("std");
        toJsonOneMode(modeItem, modes.items[1]);
        modeItem = root.createNestedObject("max");
        toJsonOneMode(modeItem, modes.items[2]);
      }

      void fromJsonOneMode(JsonObject &root, ModeItem &mode)
      {
        mode.lampMode = root["lampMode"];
        mode.fanSpeed = root["fanSpeed"];
        mode.motionMode = root["motionMode"];
        mode.motionDetectionDuration = root["motionDetectionDuration"];
        mode.motionRadiateDuration = root["motionRadiateDuration"];
        mode.maxDayDuration = root["maxDayDuration"];
        mode.totalDuration = root["totalDuration"];
        mode.extendDurationByMotion = root["extendDurationByMotion"];
      }

      StateUpdateResult fromJson(JsonObject &root) override
      {
        auto modeItem = root["min"].as<JsonObject>();
        fromJsonOneMode(modeItem, modes.items[0]);
        modeItem = root["std"].as<JsonObject>();
        fromJsonOneMode(modeItem, modes.items[1]);
        modeItem = root["max"].as<JsonObject>();
        fromJsonOneMode(modeItem, modes.items[2]);
        return StateUpdateResult::CHANGED;
      }
    };
  }
}
