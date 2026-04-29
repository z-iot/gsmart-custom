#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT

#include "esphome/core/component.h"
#include "esphome/components/mqtt/mqtt_client.h"

#include <string>
#include <vector>

namespace esphome {
namespace cloud_mqtt {

class CloudMqtt;

class CloudCmdListener {
 public:
  virtual bool process(std::string topic, std::string payload) = 0;
  void set_parent(CloudMqtt *parent) { this->parent_ = parent; }
  void set_topic(const std::string &topic) { this->cmd_topic_ = topic; }

 protected:
  CloudMqtt *parent_{nullptr};
  std::string cmd_topic_{};
};

class CloudMqtt : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  std::string getCloudTopicBase();
  std::string getCloudTopic(const std::string &suffix);

#ifdef GSMART_FEATURE_REGION
  void regionSubscribe(std::string region_serial);
  void regionUnsubscribe(std::string region_serial);
#endif

  void register_cmd_device_listener(CloudCmdListener *cmd_listener) {
    cmd_listener->set_parent(this);
    this->cmd_device_listeners_.push_back(cmd_listener);
  }

#ifdef GSMART_FEATURE_REGION
  void register_cmd_region_listener(CloudCmdListener *cmd_listener) {
    cmd_listener->set_parent(this);
    this->cmd_region_listeners_.push_back(cmd_listener);
  }
#endif

 protected:
  void configure_mqtt_client_();
  void subscribe_device_commands_();
  void processDeviceCommand(std::string topic, std::string cmd);

  std::string topic_base_{};
  bool configured_{false};
  bool device_commands_subscribed_{false};
  std::vector<CloudCmdListener *> cmd_device_listeners_;

#ifdef GSMART_FEATURE_REGION
  void processRegionCommand(std::string topic, std::string cmd);
  std::vector<CloudCmdListener *> cmd_region_listeners_;
#endif
};

class CloudCmdTrigger : public Trigger<std::string>, public CloudCmdListener {
 public:
  explicit CloudCmdTrigger(CloudMqtt *parent) { parent->register_cmd_device_listener(this); }
  bool process(std::string topic, std::string payload) override;
};

class CloudJsonCmdTrigger : public Trigger<JsonObject>, public CloudCmdListener {
 public:
  explicit CloudJsonCmdTrigger(CloudMqtt *parent) { parent->register_cmd_device_listener(this); }
  bool process(std::string topic, std::string payload) override;
};

class CloudJsonCmdRegionTrigger : public Trigger<JsonObject>, public CloudCmdListener {
 public:
  explicit CloudJsonCmdRegionTrigger(CloudMqtt *parent) {
#ifdef GSMART_FEATURE_REGION
    parent->register_cmd_region_listener(this);
#endif
  }
  bool process(std::string topic, std::string payload) override;
};

}  // namespace cloud_mqtt
}  // namespace esphome

#endif  // USE_MQTT
