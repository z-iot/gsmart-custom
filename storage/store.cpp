// #ifdef USE_ARDUINO

#include "store.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#ifdef USE_MQTT
#include "esphome/components/mqtt/mqtt_client.h"
#endif
// #include "esphome/core/entity_base.h"
// #include "esphome/core/util.h"
// #include "esphome/components/json/json_util.h"
// #include "esphome/components/network/util.h"
// #include "StreamString.h"
// #include <cstdlib>

// #ifdef USE_LOGGER
// #include <esphome/components/logger/logger.h>
// #endif

namespace esphome
{
  namespace storage
  {

    static const char *const TAG = "store";

    Store::Store()
    {
      store = this;
      ESP_LOGCONFIG(TAG, "Contructing Store...");

#ifdef GSMART_FEATURE_FILESYSTEM
      file_system_ = new FileSystem();
      ESP_LOGCONFIG(TAG, "FileSystem object created");
#endif

#ifdef GSMART_FEATURE_SCHEDULE
      schedule = new SettingsSchedule();
      ESP_LOGCONFIG(TAG, "Schedule object created");
#endif

      ESP_LOGCONFIG(TAG, "Contructing Store... done");
    };

    void Store::loop()
    {
    }

    void Store::setup()
    {
      ESP_LOGE(TAG, "--- STORE SETUP START ---");
      ESP_LOGCONFIG(TAG, "Setting up Store...");

#ifdef GSMART_FEATURE_FILESYSTEM
      if (file_system_ != nullptr) {
        ESP_LOGCONFIG(TAG, "Calling file_system_->setup()...");
        file_system_->setup();
        ESP_LOGCONFIG(TAG, "FileSystem ready: %s", file_system_->isReady() ? "YES" : "NO");
      }
#endif

#ifdef GSMART_FEATURE_REGION
      region->setup();
      ESP_LOGCONFIG(TAG, "region member count: %d", region->layout.memberCount);
#endif

#ifdef GSMART_FEATURE_SCHEDULE
      if (schedule != nullptr) {
        if (schedule->loadFromFile()) {
          ESP_LOGI(TAG, "Schedule loaded successfully (%d items)", schedule->schedule.size());
        } else {
          ESP_LOGW(TAG, "No schedule file found or failed to load, using defaults");
        }
      } else {
        ESP_LOGE(TAG, "CRITICAL: schedule is NULL!");
      }
#endif
#ifdef GSMART_FEATURE_USAGE
      usage->setup();
#endif

#ifdef GSMART_FEATURE_FILESYSTEM
      if (settingsMode != nullptr) {
        if (settingsMode->loadFromFile()) {
          ESP_LOGI(TAG, "Mode settings loaded successfully");
        } else {
          ESP_LOGW(TAG, "No mode settings file found or failed to load, using defaults");
        }
      }
      
      if (settingsDevice != nullptr) {
        if (settingsDevice->loadFromFile()) {
          ESP_LOGI(TAG, "Device settings loaded successfully");
        } else {
          ESP_LOGW(TAG, "No device settings file found or failed to load, using defaults");
        }
      }
#endif

      // Dynamic naming logic
      std::string serial = str_lower_case(this->get_serial());
      std::string model = this->get_model();

      // 1. Set dynamic identity (e.g. mobi/b80175)
      // This affects MQTT topic prefix. Note: App.name is set early and cannot be changed here.
      std::string mqtt_prefix = model + "/" + serial;
#ifdef USE_MQTT
      if (mqtt::global_mqtt_client != nullptr) {
        mqtt::global_mqtt_client->set_topic_prefix(mqtt_prefix, mqtt_prefix);
        ESP_LOGI(TAG, "Dynamic MQTT topic prefix set to: %s", mqtt_prefix.c_str());
      } else {
        ESP_LOGW(TAG, "MQTT client not found, skipping topic prefix setting");
      }
#endif

#ifdef USE_WIFI
      if (wifi::global_wifi_component != nullptr) {
        wifi::WiFiAP ap = wifi::global_wifi_component->get_ap();
        std::string ssid = "Gsmart-" + serial;
        ap.set_ssid(ssid);
        wifi::global_wifi_component->set_ap(ap);
        ESP_LOGI(TAG, "SoftAP SSID configured in WiFiComponent: %s", ssid.c_str());
      } else {
        ESP_LOGE(TAG, "WiFiComponent not found, cannot set SoftAP SSID");
      }
#endif

      ESP_LOGE(TAG, "--- STORE SETUP END ---");
    }

    void Store::dump_config()
    {
      ESP_LOGCONFIG(TAG, "Store:");
      // ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->base_->get_port());
    }
    float Store::get_setup_priority() const { return setup_priority::DATA; }

    Store *store = nullptr; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

  }
}

// #endif  // USE_ARDUINO
