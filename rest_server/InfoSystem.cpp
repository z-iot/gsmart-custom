#include "InfoSystem.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/storage/store.h"
#include "esphome/components/storage/settings_schedule.h"
#include "esphome/core/helpers.h"

InfoSystem::InfoSystem(std::shared_ptr<AsyncWebServer> server)
{
  server->on(InfoSystem_PATH, HTTP_GET, std::bind(&InfoSystem::get, this, std::placeholders::_1));
}

void InfoSystem::get(AsyncWebServerRequest *request)
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

                                                 // TODO vyhodit
                                                 JsonObject dir = root.createNestedObject("root_dir");
                                                 esphome::storage::fileSystem->listDir(dir);

                                                 JsonObject schedule = root.createNestedObject("schedule");
                                                 esphome::storage::store->schedule->toJson(schedule);

                                                 JsonObject process = root.createNestedObject("process");

                                                 //  process["getCurrentScheduleRadiation"] = esphome::storage::store->getCurrentScheduleRadiation(id(esptime).now().timestamp);;
                                                 //  process["time"] = id(esptime).now().strftime("%a %d.%b %M:%S").c_str();

                                                 process["schedule_enabled"] = esphome::storage::store->schedule->enabled;
                                                 process["isRegionActive"] = esphome::storage::store->region->isRegionActive();
                                                 process["isMaster"] = esphome::storage::store->region->isMaster();
                                                 process["selfIndex"] = esphome::storage::store->region->selfIndex;
                                                 process["masterIndex"] = esphome::storage::store->region->layout.masterIndex;

                                                 uint8_t selfMac[6];
                                                 esphome::get_mac_address_raw(selfMac);
                                                 process["selfMac"] = esphome::storage::convertMacToStr(selfMac);

                                                 process["activeMode"] = esphome::storage::convertRadiationModeToStr(esphome::storage::store->global->radiation.activeMode);
                                                 // root["lastSource"] = esphome::storage::kindRadiationSourceToStr(esphome::storage::store->global->radiation.lastSource);


                                                // JsonObject region = root.createNestedObject("region");
                                                // esphome::storage::store->region->saveToJson(region);

                                                JsonObject usage = root.createNestedObject("usage");
                                                esphome::storage::store->usage->fillAdvertise(usage);
                                               });

  request->send(200, "text/json", data.c_str());
}
