#include "fileSystem.h"

namespace esphome
{
  namespace storage
  {
    FileSystem *fileSystem = nullptr;

    FileSystem::FileSystem()
    {
      fileSystem = this;

#ifdef ESP32
      disableLoopWDT();
      ESPFS.begin(true);
      enableLoopWDT();
#elif defined(ESP8266)
      // ESP.wdtDisable();
      ESPFS.begin();
      // ESP.wdtEnable();
#endif
    };
  }
}
