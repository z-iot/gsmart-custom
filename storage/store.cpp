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

    namespace
    {
      bool parse_two_digits(const std::string &value, size_t offset, uint8_t &out)
      {
        if (offset + 1 >= value.size() || value[offset] < '0' || value[offset] > '9' ||
            value[offset + 1] < '0' || value[offset + 1] > '9')
          return false;

        out = static_cast<uint8_t>((value[offset] - '0') * 10 + (value[offset + 1] - '0'));
        return true;
      }

      bool parse_firmware_build_bytes(const std::string &version, uint8_t &hi, uint8_t &lo)
      {
        hi = 0;
        lo = 0;

        const size_t first_dot = version.find('.');
        if (first_dot != 4)
          return false;

        uint8_t year = 0;
        uint8_t month = 0;
        uint8_t day = 0;
        if (!parse_two_digits(version, 0, year) || !parse_two_digits(version, 2, month))
          return false;
        if (month < 1 || month > 12)
          return false;

        const size_t day_offset = first_dot + 1;
        if (!parse_two_digits(version, day_offset, day))
          return false;
        if (day < 1 || day > 31)
          return false;

        if (year < 26)
          return false;

        const uint16_t month_index = static_cast<uint16_t>(year - 26) * 12 + month;
        if (month_index == 0 || month_index > 255)
          return false;

        hi = static_cast<uint8_t>(month_index);
        lo = day;
        return true;
      }
    } // namespace

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

    FactoryResetResult Store::factory_reset(uint32_t reboot_delay_ms)
    {
      FactoryResetResult result;

#ifdef GSMART_FEATURE_FILESYSTEM
      if (this->file_system_ != nullptr)
        result.filesystemCleared = this->file_system_->clearAll();
#endif

      result.preferencesCleared = global_preferences != nullptr && global_preferences->reset();
      result.rebootScheduled = true;
      result.delayMs = reboot_delay_ms;
      this->set_timeout("store_factory_reset_reboot", reboot_delay_ms, []() { App.safe_reboot(); });

      return result;
    }

    void Store::getBuildNumber(uint8_t &hi, uint8_t &lo) const
    {
      if (!parse_firmware_build_bytes(this->_firmware_version, hi, lo))
      {
        hi = 0;
        lo = 0;
      }
    }

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
