#pragma once

#include <vector>
#include "esphome/core/helpers.h"
#include "settings_base.h"
#include <ctime>
#include "global.h"
#include "util.h"
#include "esphome/core/log.h"

#define DEVICE_SETTINGS_FILE "/settings/device.json"

namespace esphome
{
  namespace storage
  {
    class SettingsDevice : public SettingsBase
    {
    public:
      DeviceSettings device;

      SettingsDevice()
      {
        fileName = DEVICE_SETTINGS_FILE;
      }

      void toJson(JsonObject &root) override
      {
      }

      StateUpdateResult fromJson(JsonObject &root) override
      {
        return StateUpdateResult::CHANGED;
      }
    };
  }
}
