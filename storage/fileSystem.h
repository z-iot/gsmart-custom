#pragma once

#ifdef GSMART_FEATURE_FILESYSTEM

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/json/json_util.h"

#ifdef ESP32
#include <FS.h>
#include <LittleFS.h>
#include <stdio.h>
#include <unistd.h>
#include "esp_littlefs.h"
#include <dirent.h>
#include <sys/stat.h>
#elif defined(ESP8266)
#include <LittleFS.h>
#endif

#include <string>

#define SCHEDULE_SETTINGS_FILE "/schedule.json"

namespace esphome
{
  namespace storage
  {
    class FileSystem
    {
    public:
      FileSystem();
      void setup();
      bool isReady() { return ready; }

      // Generic JSON operations
      bool readFromFS(const char *filePath, JsonDocument &doc);
      bool writeToFS(const char *filePath, JsonObject &root);

      // Utility operations
      bool exists(const char *filePath);
      bool remove(const char *filePath);
      bool clearAll();
      void listAllFiles();
      
      // Helper for UI/Payloader
      void listDir(JsonObject &root);

      size_t GetTotalBytes();
      size_t GetUsedBytes();

    protected:
      bool ready{false};

#ifdef ESP8266
      void ensureDirectory(const char *path)
      {
        std::string s = path;
        size_t pos = s.find_last_of('/');
        if (pos != std::string::npos && pos > 0)
        {
          std::string dir = s.substr(0, pos);
          if (!LittleFS.exists(dir.c_str()))
          {
            LittleFS.mkdir(dir.c_str());
          }
        }
      }
#endif
    };

    extern FileSystem *fileSystem;

    // Implementation of inline methods
    inline bool FileSystem::readFromFS(const char *filePath, JsonDocument &doc)
    {
      if (!ready) return false;

#ifdef ESP32
      std::string fullPath = "/fs";
      if (filePath[0] != '/') fullPath += "/";
      fullPath += filePath;

      FILE *f = fopen(fullPath.c_str(), "r");
      if (!f) return false;

      fseek(f, 0, SEEK_END);
      long fsize = ftell(f);
      fseek(f, 0, SEEK_SET);

      std::string content;
      content.resize(fsize);
      fread(&content[0], 1, fsize, f);
      fclose(f);

      DeserializationError error = deserializeJson(doc, content);
#else
      File settingsFile = LittleFS.open(filePath, "r");
      if (!settingsFile) return false;
      DeserializationError error = deserializeJson(doc, settingsFile);
      settingsFile.close();
#endif

      if (error != DeserializationError::Ok) {
        ESP_LOGE("storage", "JSON error in %s: %s", filePath, error.c_str());
        return false;
      }
      return doc.is<JsonObject>();
    }

    inline bool FileSystem::writeToFS(const char *filePath, JsonObject &root)
    {
      if (!ready) return false;

#ifdef ESP32
      std::string fullPath = "/fs";
      if (filePath[0] != '/') fullPath += "/";
      fullPath += filePath;

      FILE *f = fopen(fullPath.c_str(), "w");
      if (!f) return false;

      std::string json;
      serializeJson(root, json);
      size_t bytesWritten = fwrite(json.c_str(), 1, json.length(), f);
      fflush(f);
      fsync(fileno(f));
      fclose(f);
#else
      ensureDirectory(filePath);
      File settingsFile = LittleFS.open(filePath, "w");
      if (!settingsFile) return false;
      size_t bytesWritten = serializeJson(root, settingsFile);
      settingsFile.flush();
      settingsFile.close();
#endif

      if (bytesWritten > 0) {
        ESP_LOGI("storage", "Saved %s (%d bytes)", filePath, (int)bytesWritten);
        return true;
      }
      return false;
    }

    inline size_t FileSystem::GetTotalBytes()
    {
#ifdef ESP32
      size_t total = 0, used = 0;
      esp_littlefs_info("spiffs", &total, &used); 
      return total;
#elif defined(ESP8266)
      FSInfo fs_info;
      LittleFS.info(fs_info);
      return fs_info.totalBytes;
#else
      return 0;
#endif
    }

    inline size_t FileSystem::GetUsedBytes()
    {
#ifdef ESP32
      size_t total = 0, used = 0;
      esp_littlefs_info("spiffs", &total, &used);
      return used;
#elif defined(ESP8266)
      FSInfo fs_info;
      LittleFS.info(fs_info);
      return fs_info.usedBytes;
#else
      return 0;
#endif
    }
  }
}

#endif  // GSMART_FEATURE_FILESYSTEM
