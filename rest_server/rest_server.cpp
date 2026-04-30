// #ifdef USE_ARDUINO

#include "rest_server.h"
// #include "esphome/core/application.h"

#include "InfoFeature.h"
#include "InfoSystem.h"
#include "InfoNeighborhood.h"
#include "ConfigDevice.h"
#include "ConfigScheduller.h"
#include "ConfigMode.h"
#include "ConfigTreatment.h"
#include "ConfigConnect.h"
#include "ConfigSecurity.h"
#include "ConfigConsumable.h"

#include "ConfigDef.h"
#include "ConfigData.h"

namespace esphome
{
  namespace rest_server
  {

    void RestServer::setupServer()
    {
      std::shared_ptr<AsyncWebServer> server(this->base_->get_server(), [](AsyncWebServer *) {});

      // Static UI is now served by control_server. Auth/security by gsmart_server.

      // rest info — must be heap-allocated; handlers bind `this` and would dangle
      // if these were stack locals destroyed when setupServer() returns.
      new InfoFeature(server);
      new InfoSystem(server);
      new InfoNeighborhood(server);
      // rest config
      new ConfigDevice(server);
      new ConfigScheduller(server);
      new ConfigMode(server);
      new ConfigTreatment(server);
      new ConfigConnect(server);
      new ConfigSecurity(server);
      new ConfigConsumable(server);

      new ConfigDef(server);
      new ConfigData(server);
    }

  }
}

// #endif  // USE_ARDUINO
