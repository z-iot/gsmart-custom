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

#ifdef GSMART_FEATURE_FILESYSTEM
      fileSystem = new FileSystem();
#endif
#ifdef GSMART_FEATURE_SCHEDULE
      schedule = new SettingsSchedule();
#endif
      ESP_LOGCONFIG(TAG, "Contructing Store... done");
    };

    void Store::loop()
    {
    }

    void Store::setup()
    {
      ESP_LOGCONFIG(TAG, "Setting up Store...");
#ifdef GSMART_FEATURE_REGION
      region->setup();
      ESP_LOGCONFIG(TAG, "region member count: %d", region->layout.memberCount);
#endif
#ifdef GSMART_FEATURE_SCHEDULE
      schedule->loadFromFile();
      ESP_LOGCONFIG(TAG, "schedule size: %d", schedule->schedule.size());
#endif
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
