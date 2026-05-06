// #ifdef USE_ARDUINO

#include "store.h"
// #include "esphome/core/log.h"
// #include "esphome/core/application.h"
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

      file_system_ = new FileSystem();
      ESP_LOGE(TAG, "FileSystem object created");

      schedule = new SettingsSchedule();
      ESP_LOGE(TAG, "Schedule object created");

      ESP_LOGCONFIG(TAG, "Contructing Store... done");
    };

    void Store::loop()
    {
    }

    void Store::setup()
    {
      ESP_LOGE(TAG, "--- STORE SETUP START ---");
      ESP_LOGCONFIG(TAG, "Setting up Store...");

      if (file_system_ != nullptr) {
        ESP_LOGE(TAG, "Calling file_system_->setup()...");
        file_system_->setup();
        ESP_LOGE(TAG, "FileSystem ready: %s", file_system_->isReady() ? "YES" : "NO");
      } else {
        ESP_LOGE(TAG, "CRITICAL: file_system_ is NULL!");
      }

#ifdef GSMART_FEATURE_REGION
      region->setup();
      ESP_LOGCONFIG(TAG, "region member count: %d", region->layout.memberCount);
#endif

      if (schedule != nullptr) {
        if (schedule->loadFromFile()) {
          ESP_LOGI(TAG, "Schedule loaded successfully (%d items)", schedule->schedule.size());
        } else {
          ESP_LOGW(TAG, "No schedule file found or failed to load, using defaults");
        }
      } else {
        ESP_LOGE(TAG, "CRITICAL: schedule is NULL!");
      }
#ifdef GSMART_FEATURE_USAGE
      usage->setup();
#endif
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
