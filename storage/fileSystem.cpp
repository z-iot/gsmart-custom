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
      ESPFS.begin(true);
#if CONFIG_AUTOSTART_ARDUINO
      enableLoopWDT();
#endif
#elif defined(ESP8266)
      // ESP.wdtDisable();
      ESPFS.begin();
      // ESP.wdtEnable();
#endif
    };
  }
}
