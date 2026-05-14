#pragma once

#ifdef GSMART_FEATURE_FILESYSTEM
#include "fileSystem.h"
#endif

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
        if (!saveToFile()) {
          ESP_LOGE("storage", "Failed to auto-save after reload from JSON");
        }
      }

      bool loadFromFile()
      {
#ifdef GSMART_FEATURE_FILESYSTEM
        if (fileSystem == nullptr) {
           ESP_LOGE("storage", "FileSystem not initialized, cannot load %s", fileName.c_str());
           return false;
        }
        JsonDocument doc;
        if (!fileSystem->readFromFS(fileName.c_str(), doc))
          return false;
        auto root = doc.as<JsonObject>();
        fromJson(root);
        return true;
#else
        return false;
#endif
      }

      bool saveToFile()
      {
#ifdef GSMART_FEATURE_FILESYSTEM
        if (fileSystem == nullptr) {
           ESP_LOGE("storage", "FileSystem not initialized, cannot save %s", fileName.c_str());
           return false;
        }
        JsonDocument jsonDocument;
        auto root = jsonDocument.to<JsonObject>();
        toJson(root);
        return fileSystem->writeToFS(fileName.c_str(), root);
#else
        return false;
#endif
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
