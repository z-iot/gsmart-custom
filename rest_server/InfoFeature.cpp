#include "InfoFeature.h"
#include "esphome/components/json/json_util.h"
#include "esphome/core/helpers.h"
#include "esphome/components/storage/store.h"

InfoFeature::InfoFeature(std::shared_ptr<AsyncWebServer> server) {
  server->on(InfoFeature_PATH, HTTP_GET, std::bind(&InfoFeature::get, this, std::placeholders::_1));
}

void InfoFeature::get(AsyncWebServerRequest* request) {

  std::string data =  esphome::json::build_json([](JsonObject root) {
    root["Model"] = esphome::storage::store->get_model();
    root["Serial"] = esphome::storage::store->get_serial();;

  });

  request->send(200, "text/json", data.c_str());
}
