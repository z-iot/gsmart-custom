#pragma once

#include "esphome/components/json/json_util.h"

#ifdef ESP32
#include <FS.h>
#include <LittleFS.h>
#define ESPFS LittleFS
#elif defined(ESP8266)
#include <LittleFS.h>
#define ESPFS LittleFS
#endif

#define DEFAULT_BUFFER_SIZE 4096
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
      void listAllFiles();

      bool readFromFS(const char *filePath, JsonDocument &doc)
      {
        if (!ready) {
          ESP_LOGE("storage", "FS not ready, cannot read %s", filePath);
          return false;
        }
        File settingsFile = ESPFS.open(filePath, "r");

        if (!settingsFile) {
          ESP_LOGD("storage", "File not found: %s", filePath);
          return false;
        }
        DeserializationError error = deserializeJson(doc, settingsFile);
        settingsFile.close();
        if (error != DeserializationError::Ok) {
          ESP_LOGE("storage", "Failed to deserialize %s: %s", filePath, error.c_str());
          return false;
        }
        if (!doc.is<JsonObject>()) {
          ESP_LOGE("storage", "File %s is not a JSON object", filePath);
          return false;
        }
        return true;
      }

      bool writeToFS(const char *filePath, JsonObject &root)
      {
        if (!ready) {
          ESP_LOGE("storage", "FS not ready, cannot write %s", filePath);
          return false;
        }
        
        ensureDirectory(filePath);

        File settingsFile = ESPFS.open(filePath, "w");
        if (!settingsFile) {
          ESP_LOGE("storage", "Failed to open file for writing: %s", filePath);
          return false;
        }
        size_t bytesWritten = serializeJson(root, settingsFile);
        settingsFile.flush();
        settingsFile.close();
        if (bytesWritten == 0) {
          ESP_LOGE("storage", "Failed to write any data to %s (serializeJson returned 0)", filePath);
          return false;
        }
        ESP_LOGI("storage", "Saved %s (%d bytes)", filePath, (int)bytesWritten);
        return true;
      }

      void ensureDirectory(const char *filePath)
      {
        std::string path = filePath;
        // Find the last slash to separate directory from filename
        size_t lastSlash = path.find_last_of('/');
        if (lastSlash != std::string::npos && lastSlash > 0) {
          std::string dir = path.substr(0, lastSlash);
          if (!ESPFS.exists(dir.c_str())) {
            ESP_LOGD("storage", "Creating directory: %s", dir.c_str());
            if (!ESPFS.mkdir(dir.c_str())) {
              ESP_LOGE("storage", "Failed to create directory: %s", dir.c_str());
            }
          }
        }
      }

      bool ready = false;

      size_t GetTotalBytes()
      {
#ifdef ESP32
        return ESPFS.totalBytes();
#elif defined(ESP8266)
        FSInfo fs_info;
        ESPFS.info(fs_info);
        return fs_info.totalBytes;
#endif
      }

      size_t GetUsedBytes()
      {
#ifdef ESP32
        return ESPFS.usedBytes();
#elif defined(ESP8266)
        FSInfo fs_info;
        ESPFS.info(fs_info);
        return fs_info.usedBytes;
#endif
      }

#ifdef ESP32
      void listDir(JsonObject &root, std::string dirPath = "/")
      {
        root["dir"] = dirPath;
        File dir = ESPFS.open(dirPath.c_str());
        if (!dir)
        {
          root["error"] = "Failed to open dir";
          return;
        }
        if (!dir.isDirectory())
        {
          root["error"] = "Not directory";
          dir.close();
          return;
        }

        File file = dir.openNextFile();
        JsonArray arr = root["files"].to<JsonArray>();
        while (file)
        {
          JsonObject arrItem = arr.add<JsonObject>();
          arrItem["filename"] = (String(file.name()).startsWith("/") ? String(file.name()).substring(1) : file.name());
          arrItem["type"] = (file.isDirectory() ? "Dir" : "File");
          arrItem["size"] = FileSystem::convertSizeToStr(file.size());
          file = dir.openNextFile();
        }
        dir.close();
      }
#elif defined(ESP8266)
      void listDir(JsonObject &root, std::string dirPath = "/")
      {
        root["dir"] = dirPath;

        auto dir = ESPFS.openDir(dirPath.c_str());
        if (!dir.isDirectory())
        {
          root["error"] = "Not directory";
          return;
        }

        JsonArray arr = root["files"].to<JsonArray>();
        while (dir.next())
        {
          JsonObject arrItem = arr.add<JsonObject>();
          arrItem["filename"] = (String(dir.fileName()).startsWith("/") ? String(dir.fileName()).substring(1) : dir.fileName());
          arrItem["type"] = (dir.isDirectory() ? "Dir" : "File");
          arrItem["size"] = FileSystem::convertSizeToStr(dir.fileSize());
        }
      }
#endif

      //convert filesize to string with unit
      static  String convertSizeToStr(size_t bytes)
      {
        if (bytes < 1024)
        {
          return String(bytes) + " B";
        }
        else if (bytes < 1024 * 1024)
        {
          return String(bytes / 1024.0) + " KB";
        }
        else if (bytes < (1024 * 1024 * 1024))
        {
          return String(bytes / 1024.0 / 1024.0) + " MB";
        }
        else
          return "";
      }

    protected:
    };

    extern FileSystem *fileSystem;

  }
}
