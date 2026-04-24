#pragma once

#include "fileSystem.h"

namespace esphome
{
  namespace storage
  {

    enum class StateUpdateResult
    {
      CHANGED = 0, // The update changed the state and propagation should take place if required
      UNCHANGED,   // The state was unchanged, propagation should not take place
      ERROR        // There was a problem updating the state, propagation should not take place
    };

    class SettingsBase
    {
    public:
      SettingsBase()
      {
        fileName = "";
      }
      
      void reloadFromJson(JsonObject &root)
      {
        fromJson(root); // TODO porovnat stary a novy
        // ESP_LOGW("schedule", "reloadFromJson: %d", schedule.size());
        saveToFile();
      }

      bool loadFromFile()
      {
        DynamicJsonDocument doc = DynamicJsonDocument(DEFAULT_BUFFER_SIZE);
        if (!fileSystem->readFromFS(fileName.c_str(), doc))
          return false;
        auto root = doc.as<JsonObject>();
        fromJson(root);
        return true;
      }

      bool saveToFile()
      {
        DynamicJsonDocument jsonDocument = DynamicJsonDocument(DEFAULT_BUFFER_SIZE);
        auto root = jsonDocument.to<JsonObject>();
        toJson(root);
        return fileSystem->writeToFS(fileName.c_str(), root);
      }

      virtual void toJson(JsonObject &root)
      {
      }

      virtual StateUpdateResult fromJson(JsonObject &root)
      {
        return StateUpdateResult::ERROR;
      }

      std::string fileName;
    };

  }
}
