#include "mobile_api.h"

namespace esphome {
namespace mobile_api {

// Intentionally empty: mobile_api is being redesigned as a clean v1 REST
// surface that will reuse helpers from gsmart_server::payloads. Until the
// new contract lands, the legacy gsmart-deck routes are served by
// control_server.
void MobileApi::setup() { this->base_->init(); }

}  // namespace mobile_api
}  // namespace esphome
