#include "fileSystem.h"

#ifdef ESP32
#include <esp32-hal.h>
#include <errno.h>

extern "C" int __attribute__((weak)) rmdir(const char *path) {
  errno = ENOSYS;
  return -1;
}
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
      ESP_LOGE("storage", ">>> STARTING LITTLEFS DIAGNOSTICS <<<");
#ifdef ESP32
      // 1. Check partition table via ESP-IDF API
      const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)0x83, "littlefs");
      if (part == nullptr) {
        ESP_LOGE("storage", "DIAG: Partition 'littlefs' (subtype 0x83) NOT FOUND!");
        part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)0x82, "littlefs");
        if (part != nullptr) ESP_LOGE("storage", "DIAG: Found 'littlefs' with SPIFFS subtype (0x82)");
        else {
          ESP_LOGE("storage", "DIAG: No partition with label 'littlefs' found at all!");
        }
      } else {
        ESP_LOGE("storage", "DIAG: Partition 'littlefs' found at 0x%08X, size 0x%08X", (int)part->address, (int)part->size);
      }

      // 2. Attempt mount using /fs as mount point (to avoid /littlefs conflict)
      ESP_LOGE("storage", "DIAG: Mounting LittleFS at /fs...");
      this->ready = ESPFS.begin(false, "/fs", 10, "littlefs"); 

      if (!this->ready) {
        ESP_LOGE("storage", "DIAG: Mount FAILED. Forcing FORMAT now...");
        uint32_t start_fmt = millis();
        bool fmt_res = ESPFS.format();
        uint32_t dur_fmt = millis() - start_fmt;
        ESP_LOGE("storage", "DIAG: Format took %u ms. Result: %s", (unsigned int)dur_fmt, fmt_res ? "SUCCESS" : "FAIL");
        
        ESP_LOGE("storage", "DIAG: Remounting after format...");
        this->ready = ESPFS.begin(false, "/fs", 10, "littlefs");
      }

      if (this->ready) {
        ESP_LOGE("storage", "DIAG: MOUNT SUCCESS! Total: %u, Used: %u", (unsigned int)GetTotalBytes(), (unsigned int)GetUsedBytes());
        
        // 3. Persistence verification with immediate read-back
        ESP_LOGE("storage", "DIAG: Writing test file /diag.txt...");
        File f = ESPFS.open("/diag.txt", "w");
        if (f) {
          f.println("DIAG_OK");
          f.flush();
          f.close();
          ESP_LOGE("storage", "DIAG: File written and closed. Verifying...");
          
          File v = ESPFS.open("/diag.txt", "r");
          if (v) {
            String content = v.readString();
            content.trim();
            ESP_LOGE("storage", "DIAG: Verification SUCCESS! Content: [%s]", content.c_str());
            v.close();
          } else {
            ESP_LOGE("storage", "DIAG: Verification FAILED! Could not open file for read immediately after write.");
          }
        } else {
          ESP_LOGE("storage", "DIAG: Failed to open /diag.txt for writing!");
        }

        listAllFiles();
      } else {
        ESP_LOGE("storage", "DIAG: CRITICAL - LittleFS could not be initialized even after format!");
      }
#elif defined(ESP8266)
      this->ready = ESPFS.begin();
      if (this->ready) {
        ESP_LOGE("storage", "LittleFS mounted. Total: %d, Used: %d", (int)GetTotalBytes(), (int)GetUsedBytes());
        listAllFiles();
      } else {
        ESP_LOGE("storage", "Failed to mount LittleFS!");
      }
#endif
    }

    void FileSystem::listAllFiles()
    {
      ESP_LOGE("storage", "Listing files at root:");
#ifdef ESP32
      // Try different root paths as some VFS drivers are picky
      File root = ESPFS.open("/");
      if (!root) {
        ESP_LOGD("storage", "Failed to open '/', trying '/.'");
        root = ESPFS.open("/.");
      }
      if (!root) {
        ESP_LOGD("storage", "Failed to open '/.', trying ''");
        root = ESPFS.open("");
      }
      
      if (!root) {
        ESP_LOGE("storage", "Failed to open root directory (tried '/', '/.', '')");
      } else if (!root.isDirectory()) {
        ESP_LOGE("storage", "Root path exists but is NOT a directory!");
        root.close();
      } else {
        int count = 0;
        File file = root.openNextFile();
        while (file) {
          ESP_LOGE("storage", "  [%s] - %d bytes", file.name(), (int)file.size());
          file = root.openNextFile();
          count++;
        }
        root.close();
        ESP_LOGE("storage", "Listing done. Total files found: %d", count);
      }
#endif
    }
  }
}
