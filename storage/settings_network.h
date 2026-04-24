#pragma once

#include <vector>
#include "esphome/core/helpers.h"
#include "settings_base.h"
#include <ctime>
#include "global.h"
#include "util.h"
#include "esphome/core/log.h"

#define NETWORK_SETTINGS_FILE "/settings/network.json"

namespace esphome
{
  namespace storage
  {
    class SettingsNetwork : public SettingsBase
    {
    public:
      NetworkSettings network;

      SettingsNetwork()
      {
        fileName = NETWORK_SETTINGS_FILE;
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
