#pragma once

#include <Arduino.h>
#include <string>
#include "esphome/components/json/json_util.h"
#include "global.h"
namespace esphome
{
  namespace storage
  {
    std::string convertModelToStr(uint8_t modelNum);

    uint8_t convertModelToNum(const std::string &model);

    std::string convertRegionSerialtoStr(uint64_t serial);

    uint64_t convertRegionSerialtoNum(std::string serial);

    void convertMacToArray(std::string macStr, uint8_t *mac);

    std::string convertMacToStr(uint8_t *mac);

    storage::RadiationMode convertRadiationStrToMode(std::string modeStr);

    std::string convertRadiationModeToStr(storage::RadiationMode mode);

    uint8_t convertWeekDayFromSkToUsa(uint8_t day_sk);
    uint8_t convertWeekDayFromUsaToSk(uint8_t day_usa);

  }
}
