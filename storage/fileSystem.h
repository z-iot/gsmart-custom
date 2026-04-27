#pragma once

#include "esphome/components/json/json_util.h"

#ifdef ESP32
#include <FS.h>
#include <SPIFFS.h>
#define ESPFS SPIFFS
#elif defined(ESP8266)
#include <LittleFS.h>
#define ESPFS LittleFS
#endif

#define DEFAULT_BUFFER_SIZE 4096

namespace esphome
{
  namespace storage
  {

    class FileSystem
    {
    public:
      FileSystem();

      bool readFromFS(const char *filePath, DynamicJsonDocument &doc)
      {
        File settingsFile = ESPFS.open(filePath, "r");

        if (!settingsFile)
          return false;
        DeserializationError error = deserializeJson(doc, settingsFile);
        settingsFile.close();
        if (error != DeserializationError::Ok || !doc.is<JsonObject>())
          return false;
        return true;

        // If we reach here we have not been successful in loading the config and hard-coded defaults are now applied.
        // The settings are then written back to the file system so the defaults persist between resets. This last step is
        // required as in some cases defaults contain randomly generated values which would otherwise be modified on reset.
        // applyDefaults();
        // writeToFS();
      }

      bool writeToFS(const char *filePath, JsonObject &root)
      {
        File settingsFile = ESPFS.open(filePath, "w");
        if (!settingsFile)
          return false;
        serializeJson(root, settingsFile);
        settingsFile.close();
        return true;
      }

      size_t GetTotalBytes()
      {
#ifdef ESP32
        return SPIFFS.totalBytes();
#elif defined(ESP8266)
        FSInfo fs_info;
        ESPFS.info(fs_info);
        return fs_info.totalBytes;
#endif
      }

      size_t GetUsedBytes()
      {
#ifdef ESP32
        return SPIFFS.usedBytes();
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
        JsonArray arr = root.createNestedArray("files");
        while (file)
        {
          JsonObject arrItem = arr.createNestedObject();
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

        JsonArray arr = root.createNestedArray("files");
        while (dir.next())
        {
          JsonObject arrItem = arr.createNestedObject();
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
