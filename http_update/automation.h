#pragma once

#include "esphome/core/defines.h"
#ifdef USE_HTTPUPDATE_STATE_CALLBACK

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "http_update.h"

namespace esphome {
namespace http_update {

class HttpUpdateStateChangeTrigger : public Trigger<HttpUpdateState> {
 public:
  explicit HttpUpdateStateChangeTrigger(HttpUpdateComponent *parent) {
    parent->add_on_state_callback([this, parent](HttpUpdateState state, float progress, uint8_t error) {
      if (!parent->is_failed()) {
        return trigger(state);
      }
    });
  }
};

class HttpUpdateStartTrigger : public Trigger<> {
 public:
  explicit HttpUpdateStartTrigger(HttpUpdateComponent *parent) {
    parent->add_on_state_callback([this, parent](HttpUpdateState state, float progress, uint8_t error) {
      if (state == HTTPUPDATE_STARTED && !parent->is_failed()) {
        trigger();
      }
    });
  }
};

class HttpUpdateProgressTrigger : public Trigger<float> {
 public:
  explicit HttpUpdateProgressTrigger(HttpUpdateComponent *parent) {
    parent->add_on_state_callback([this, parent](HttpUpdateState state, float progress, uint8_t error) {
      if (state == HTTPUPDATE_IN_PROGRESS && !parent->is_failed()) {
        trigger(progress);
      }
    });
  }
};

class HttpUpdateEndTrigger : public Trigger<> {
 public:
  explicit HttpUpdateEndTrigger(HttpUpdateComponent *parent) {
    parent->add_on_state_callback([this, parent](HttpUpdateState state, float progress, uint8_t error) {
      if (state == HTTPUPDATE_COMPLETED && !parent->is_failed()) {
        trigger();
      }
    });
  }
};

class HttpUpdateErrorTrigger : public Trigger<int> {
 public:
  explicit HttpUpdateErrorTrigger(HttpUpdateComponent *parent) {
    parent->add_on_state_callback([this, parent](HttpUpdateState state, float progress, uint8_t error) {
      if (state == HTTPUPDATE_ERROR && !parent->is_failed()) {
        trigger(error);
      }
    });
  }
};

}
}

#endif
