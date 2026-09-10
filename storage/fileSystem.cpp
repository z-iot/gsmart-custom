#include "fileSystem.h"
#include "esphome/core/preferences.h"

#ifdef GSMART_FEATURE_FILESYSTEM

#ifdef ESP32
#include <esp32-hal.h>
#include <errno.h>
#include "esp_littlefs.h"
#include <dirent.h>
#include <sys/stat.h>
#include <esp_partition.h>

#endif

namespace esphome
{
  namespace storage
  {
    FileSystem *fileSystem = nullptr;

    FileSystem::FileSystem()
    {
      fileSystem = this;
    };

    void FileSystem::setup()
    {
#ifdef ESP32
      esp_vfs_littlefs_conf_t conf = {
        .base_path = "/fs",
        .partition_label = "spiffs", 
        .format_if_mount_failed = false,
        .dont_mount = false,
      };

      esp_err_t ret = esp_vfs_littlefs_register(&conf);
      if (ret != ESP_OK) {
        // Initial blank partition is safe to initialize. A damaged established
        // filesystem must retain its unacknowledged journal for recovery.
        const auto *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "spiffs");
        bool blank = partition != nullptr;
        uint8_t block[256];
        for (size_t offset = 0; blank && offset < partition->size; offset += sizeof(block)) {
          if (esp_partition_read(partition, offset, block, sizeof(block)) != ESP_OK) { blank = false; break; }
          for (const auto byte : block) if (byte != 0xFF) { blank = false; break; }
          yield();
        }
        if (blank && esp_littlefs_format("spiffs") == ESP_OK) ret = esp_vfs_littlefs_register(&conf);
      }
      if (ret == ESP_OK) {
        this->ready = true;
        ESP_LOGI("storage", "Storage initialized on /fs (Label: spiffs)");
      } else {
        ESP_LOGE("storage", "Storage init failed: %s", esp_err_to_name(ret));
      }
#elif defined(ESP8266)
      LittleFS.setConfig(LittleFSConfig(false));
      this->ready = LittleFS.begin();
      auto marker = global_preferences->make_preference<uint32_t>(0x47484A31, true);
      uint32_t initialized = 0;
      const bool known = marker.load(&initialized) && initialized == 0x47484A31;
      if (!this->ready && !known) {
        // REX previously had no filesystem. Its new 32 KiB range can contain
        // obsolete OTA staging bytes. One-time migration initializes only this
        // newly reserved range; EEPROM/RF sectors keep their original addresses.
        this->ready = LittleFS.format() && LittleFS.begin();
      }
      if (this->ready && !known) {
        initialized = 0x47484A31;
        // No journal writes until the migration marker is durable.
        this->ready = marker.save(&initialized) && global_preferences->sync();
      }
      if (this->ready) {
        ESP_LOGI("storage", "Storage initialized (LittleFS)");
      } else {
        ESP_LOGE("storage", "Storage init failed!");
      }
#endif
    }

    bool FileSystem::exists(const char *filePath)
    {
      if (!ready) return false;
#ifdef ESP32
      std::string fullPath = "/fs";
      if (filePath[0] != '/') fullPath += "/";
      fullPath += filePath;
      struct stat st;
      return (stat(fullPath.c_str(), &st) == 0);
#else
      return LittleFS.exists(filePath);
#endif
    }

    bool FileSystem::remove(const char *filePath)
    {
      if (!ready) return false;
#ifdef ESP32
      std::string fullPath = "/fs";
      if (filePath[0] != '/') fullPath += "/";
      fullPath += filePath;
      return (unlink(fullPath.c_str()) == 0);
#else
      return LittleFS.remove(filePath);
#endif
    }

    bool FileSystem::clearAll()
    {
      if (!ready) return false;
#ifdef ESP32
      return esp_littlefs_format("spiffs") == ESP_OK;
#elif defined(ESP8266)
      return LittleFS.format();
#else
      return false;
#endif
    }

    void FileSystem::listAllFiles()
    {
      if (!ready) return;
      ESP_LOGI("storage", "Listing files:");
#ifdef ESP32
      DIR *dir = opendir("/fs");
      if (!dir) return;
      struct dirent *ent;
      while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type == DT_REG) {
          ESP_LOGI("storage", "  - %s", ent->d_name);
        }
      }
      closedir(dir);
#elif defined(ESP8266)
      auto dir = LittleFS.openDir("/");
      while (dir.next()) {
        ESP_LOGI("storage", "  - %s (%d bytes)", dir.fileName().c_str(), (int)dir.fileSize());
      }
#endif
    }

    void FileSystem::listDir(JsonObject &root)
    {
      if (!ready) return;
#ifdef ESP32
      DIR *dir = opendir("/fs");
      if (!dir) return;
      struct dirent *ent;
      while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type == DT_REG) {
          std::string path = "/fs/";
          path += ent->d_name;
          struct stat st;
          if (stat(path.c_str(), &st) == 0) {
            root[ent->d_name] = (int)st.st_size;
          } else {
            root[ent->d_name] = 0;
          }
        }
      }
      closedir(dir);
#elif defined(ESP8266)
      auto dir = LittleFS.openDir("/");
      while (dir.next()) {
        root[dir.fileName()] = (int)dir.fileSize();
      }
#endif
    }
  }
}

#endif  // GSMART_FEATURE_FILESYSTEM
