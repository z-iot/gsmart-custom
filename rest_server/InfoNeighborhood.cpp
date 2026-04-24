#include "InfoNeighborhood.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/storage/store.h"
#include "esphome/components/storage/settings_schedule.h"

InfoNeighborhood::InfoNeighborhood(std::shared_ptr<AsyncWebServer> server)
{
  server->on(InfoNeighborhood_PATH, HTTP_GET, std::bind(&InfoNeighborhood::get, this, std::placeholders::_1));
}

void InfoNeighborhood::get(AsyncWebServerRequest *request)
{

  std::string data = esphome::json::build_json([](JsonObject root)
                                               {
      root["today-m3"] = 22.5;

#ifdef ESP32
      root["esp_platform"] = "esp32";
      root["max_alloc_heap"] = ESP.getMaxAllocHeap();
      root["psram_size"] = ESP.getPsramSize();
      root["free_psram"] = ESP.getFreePsram();
#elif defined(ESP8266)
      root["esp_platform"] = "esp8266";
      root["max_alloc_heap"] = ESP.getMaxFreeBlockSize();
      root["heap_fragmentation"] = ESP.getHeapFragmentation();
#endif
      root["cpu_freq_mhz"] = ESP.getCpuFreqMHz();
      root["free_heap"] = ESP.getFreeHeap();
      root["sketch_size"] = ESP.getSketchSize();
      root["free_sketch_space"] = ESP.getFreeSketchSpace();
      root["sdk_version"] = ESP.getSdkVersion();
      root["flash_chip_size"] = ESP.getFlashChipSize();
      root["flash_chip_speed"] = ESP.getFlashChipSpeed();

      root["fs_total"] = esphome::storage::fileSystem->GetTotalBytes();
      root["fs_used"] = esphome::storage::fileSystem->GetUsedBytes();

      //TODO vyhodit
      JsonObject dir = root.createNestedObject("root_dir");
      esphome::storage::fileSystem->listDir(dir);
    });

  request->send(200, "text/json", data.c_str());
}
