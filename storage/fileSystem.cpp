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

#ifdef ESP32
#if CONFIG_AUTOSTART_ARDUINO
      disableLoopWDT();
#endif
      this->ready = ESPFS.begin(true);
      if (!this->ready) {
        ESP_LOGE("storage", "Failed to mount SPIFFS!");
      } else {
        ESP_LOGI("storage", "SPIFFS mounted successfully.");
      }
#if CONFIG_AUTOSTART_ARDUINO
      enableLoopWDT();
#endif
#elif defined(ESP8266)
      // ESP.wdtDisable();
      this->ready = ESPFS.begin();
      if (!this->ready) {
        ESP_LOGE("storage", "Failed to mount LittleFS!");
      }
      // ESP.wdtEnable();
#endif
    };
  }
}
