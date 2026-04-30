#include "mobile_api.h"

#include "ConfigConnect.h"
#include "ConfigConsumable.h"
#include "ConfigData.h"
#include "ConfigDef.h"
#include "ConfigDevice.h"
#include "ConfigMode.h"
#include "ConfigScheduller.h"
#include "ConfigSecurity.h"
#include "ConfigTreatment.h"
#include "InfoFeature.h"
#include "InfoNeighborhood.h"
#include "InfoSystem.h"

namespace esphome {
namespace mobile_api {

void MobileApi::setup() {
  this->base_->init();
  std::shared_ptr<AsyncWebServer> server(this->base_->get_server(), [](AsyncWebServer *) {});

  // Handlers bind `this`, so they must outlive setup().
  new InfoFeature(server);
  new InfoSystem(server);
  new InfoNeighborhood(server);

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

}  // namespace mobile_api
}  // namespace esphome
