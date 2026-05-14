#pragma once

#include "esphome/core/component.h"
#include "esphome/components/mqtt/mqtt_client.h"
#include "esphome/components/api_core_v1/api_core_v1.h"

namespace esphome {
namespace api_adapter_mqtt {

class ApiAdapterMqtt : public Component {
 public:
  ApiAdapterMqtt(api_core_v1::ApiCoreV1 *core) : core_(core) {}

  void setup() override;
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

 protected:
  void subscribe_topics_();
  void on_message_(const std::string &topic, const std::string &payload);
  void publish_response_(const std::string &suffix, JsonVariant rid, std::function<void(JsonObject)> builder);

  api_core_v1::ApiCoreV1 *core_;
  std::string base_topic_;
  bool subscribed_{false};
};

}  // namespace api_adapter_mqtt
}  // namespace esphome
