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
      ESP_LOGE("storage", ">>> LittleFS setup starting (Default Mount, Part: littlefs) <<<");
#ifdef ESP32
#if CONFIG_AUTOSTART_ARDUINO
      disableLoopWDT();
#endif
      // Explicitly passing "littlefs" partition label to avoid default "spiffs" lookup in some versions of the library
      this->ready = ESPFS.begin(true, "/littlefs", 10, "littlefs"); 

      if (!this->ready) {
        ESP_LOGE("storage", "CRITICAL: Failed to mount LittleFS partition!");
      } else {
        ESP_LOGE("storage", "LittleFS mounted. Total: %d, Used: %d", (int)GetTotalBytes(), (int)GetUsedBytes());
        
        // Persistence check
        bool persistence_ok = false;
        File test_read = ESPFS.open("/persist_test.txt", "r");
        if (test_read) {
          String content = test_read.readString();
          test_read.close();
          content.trim();
          ESP_LOGE("storage", "PERSISTENCE: Found file! Content: [%s]", content.c_str());
          persistence_ok = true;
        } else {
          ESP_LOGE("storage", "PERSISTENCE: Test file not found or failed to open for read. Writing new one...");
          File f = ESPFS.open("/persist_test.txt", "w");
          if (f) {
            f.println("BOOT_SUCCESS");
            f.flush();
            f.close();
            ESP_LOGE("storage", "PERSISTENCE: Test file written and flushed.");
            
            // Immediate verification using open instead of exists
            File v = ESPFS.open("/persist_test.txt", "r");
            if (v) {
              ESP_LOGE("storage", "PERSISTENCE: Verified file exists immediately after write.");
              v.close();
              persistence_ok = true;
            } else {
              ESP_LOGE("storage", "PERSISTENCE: ERROR! File does not exist (open failed) immediately after write!");
            }
          } else {
            ESP_LOGE("storage", "PERSISTENCE: Failed to write test file!");
          }
        }

        // SELF-HEALING
        if (!persistence_ok) {
          ESP_LOGE("storage", "!!! CRITICAL: Filesystem check failed. FORCING REFORMAT !!!");
          ESPFS.format();
          ESPFS.end();
          this->ready = ESPFS.begin(true, "/littlefs", 10, "littlefs");
          if (this->ready) {
            ESP_LOGE("storage", "LittleFS mounted after format. Retrying test write...");
            File f = ESPFS.open("/persist_test.txt", "w");
            if (f) {
              f.println("FORMAT_SUCCESS");
              f.flush();
              f.close();
              ESP_LOGE("storage", "PERSISTENCE: Test file written after format.");
            }
          }
        }

        ESP_LOGE("storage", "Usage: Total: %d, Used: %d", (int)GetTotalBytes(), (int)GetUsedBytes());
        listAllFiles();
      }
#if CONFIG_AUTOSTART_ARDUINO
      enableLoopWDT();
#endif
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
