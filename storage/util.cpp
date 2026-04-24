#include "util.h"

namespace esphome
{
  namespace storage
  {

    std::string convertModelToStr(uint8_t modelNum)
    {
      if (modelNum == 11)
        return "sibra";
      if (modelNum == 21)
        return "opera";
      if (modelNum == 22)
        return "aqua";
      if (modelNum == 23)
        return "mobi";
      if (modelNum == 51)
        return "rex";
      if (modelNum == 52)
        return "panel";
      return "unknown";
    }

    uint8_t convertModelToNum(const std::string &model)
    {
      if (model == "sibra")
        return 11;
      if (model == "opera")
        return 21;
      if (model == "aqua")
        return 22;
      if (model == "mobi")
        return 32;
      if (model == "rex")
        return 51;
      if (model == "panel")
        return 52;
      return 0;
    }

    std::string convertRegionSerialtoStr(uint64_t serial)
    {
      if (serial == 0)
        return "";
      char buffer[20];
      sprintf(buffer, "%014llX", serial);
      auto str = std::string(buffer);
      std::transform(str.begin(), str.end(), str.begin(), ::tolower);
      return str;
    }

    uint64_t convertRegionSerialtoNum(std::string serial)
    {
      if (serial.length() != 14)
        return 0;
      return strtoull(serial.c_str(), NULL, 16);
    }

    void convertMacToArray(std::string macStr, uint8_t *mac)
    {
      for (int i = 0; i < 6; i++)
      {
        if (macStr.length() == 12)
          mac[i] = strtoul(macStr.substr(i * 2, 2).c_str(), NULL, 16);
        else
          mac[i] = 0;
      }
    }

    std::string convertMacToStr(uint8_t *mac)
    {
      char buffer[20];
      sprintf(buffer, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      auto str = std::string(buffer);
      std::transform(str.begin(), str.end(), str.begin(), ::tolower);
      return str;
    }

    storage::RadiationMode convertRadiationStrToMode(std::string modeStr)
    {
      if (modeStr == "Min")
        return storage::RadiationMode::MIN;
      if (modeStr == "Std")
        return storage::RadiationMode::STD;
      if (modeStr == "Max")
        return storage::RadiationMode::MAX;
      return storage::RadiationMode::NONE;
    }

    std::string convertRadiationModeToStr(storage::RadiationMode mode)
    {
      switch (mode)
      {
      case storage::RadiationMode::MIN:
        return "Min";
      case storage::RadiationMode::STD:
        return "Std";
      case storage::RadiationMode::MAX:
        return "Max";
      default:
        return "None";
      }
    }

    uint8_t convertWeekDayFromSkToUsa(uint8_t day_sk)
    {
      if (day_sk == 6)
        return 0;
      return day_sk + 1;
    }

    uint8_t convertWeekDayFromUsaToSk(uint8_t day_usa)
    {
      if (day_usa == 0)
        return 6; 
      return day_usa - 1;
    }



  }
}
