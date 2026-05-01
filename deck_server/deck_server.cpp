#include "gsmart_server.h"

namespace esphome {
namespace gsmart_server {

GsmartServer *gsmartServer = nullptr;

void GsmartServer::setup() {
  this->base_->init();

  auto server = this->server();
  this->security_ = new SecurityService(server);
  this->security_->begin();
  this->auth_ = new AuthenticationService(server, this->security_);

  gsmartServer = this;
}

}  // namespace gsmart_server
}  // namespace esphome
