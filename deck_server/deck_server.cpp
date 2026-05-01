#include "deck_server.h"

namespace esphome {
namespace deck_server {

DeckServer *deckServer = nullptr;

void DeckServer::setup() {
  this->base_->init();

  auto server = this->server();
  this->security_ = new SecurityService(server);
  this->security_->begin();
  this->auth_ = new AuthenticationService(server, this->security_);

  deckServer = this;
}

}  // namespace deck_server
}  // namespace esphome
