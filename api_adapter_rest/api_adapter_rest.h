#pragma once

#include "esphome/core/component.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/api_core_v1/api_core_v1.h"

namespace esphome {
namespace api_adapter_rest {

class ApiAdapterRest : public Component {
 public:
  ApiAdapterRest(web_server_base::WebServerBase *base, api_core_v1::ApiCoreV1 *core)
      : base_(base), core_(core) {}

  void setup() override;
  float get_setup_priority() const override { return setup_priority::WIFI - 0.5f; }

 protected:
  web_server_base::WebServerBase *base_;
  api_core_v1::ApiCoreV1 *core_;
};

}  // namespace api_adapter_rest
}  // namespace esphome
