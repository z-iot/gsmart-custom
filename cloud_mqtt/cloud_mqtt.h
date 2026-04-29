#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT

#include "esphome/components/json/json_util.h"
#include "esphome/core/component.h"
#include "esphome/components/mqtt/custom_mqtt_device.h"
#include "esphome/components/mqtt/mqtt_client.h"

#include <string>
#include <vector>

namespace esphome {
namespace cloud_mqtt {

class CloudMqtt;

class CloudCmdListener {
 public:
  virtual bool process(std::string topic, std::string payload) = 0;
  virtual bool process_json(std::string topic, JsonObject root) { return false; }
  virtual bool expects_json() const { return false; }
  void set_parent(CloudMqtt *parent) { this->parent_ = parent; }
  void set_topic(const std::string &topic) { this->cmd_topic_ = topic; }
  bool matches(const std::string &topic) const { return this->cmd_topic_ == topic; }

 protected:
  CloudMqtt *parent_{nullptr};
  std::string cmd_topic_{};
};

class CloudMqtt : public Component, public mqtt::CustomMQTTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  std::string getCloudTopicBase();
  std::string getCloudTopic(const std::string &suffix);
  bool publish_cloud_json(const std::string &suffix, const json::json_build_t &f, uint8_t qos = 0,
                          bool retain = false);
  bool publish_config(const json::json_build_t &f) { return this->publish_cloud_json("config", f, 0, true); }
  bool publish_advertise(const json::json_build_t &f) { return this->publish_cloud_json("advertise", f); }
  bool publish_state(const json::json_build_t &f) { return this->publish_cloud_json("state", f); }
  bool publish_motion(const json::json_build_t &f) { return this->publish_cloud_json("motion", f); }
  bool publish_button(const json::json_build_t &f) { return this->publish_cloud_json("button", f); }
  bool publish_setup(const json::json_build_t &f) { return this->publish_cloud_json("setup", f); }

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
  void on_device_command_(const std::string &topic, const std::string &payload);
  void processDeviceCommand(std::string topic, std::string cmd);
  bool dispatch_command_(const std::vector<CloudCmdListener *> &listeners, const std::string &topic,
                         const std::string &payload);
  bool publish_ack_(const std::string &msg_id, const char *result, const char *error);

  std::string topic_base_{};
  std::string device_command_topic_prefix_{};
  bool configured_{false};
  bool device_commands_subscribed_{false};
  std::vector<CloudCmdListener *> cmd_device_listeners_;

#ifdef GSMART_FEATURE_REGION
  void on_region_command_(const std::string &topic, const std::string &payload);
  void processRegionCommand(std::string topic, std::string cmd);
  std::string region_command_topic_prefix_{};
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
  bool expects_json() const override { return true; }
  bool process_json(std::string topic, JsonObject root) override;
  bool process(std::string topic, std::string payload) override;
};

class CloudJsonCmdRegionTrigger : public Trigger<JsonObject>, public CloudCmdListener {
 public:
  explicit CloudJsonCmdRegionTrigger(CloudMqtt *parent) {
#ifdef GSMART_FEATURE_REGION
    parent->register_cmd_region_listener(this);
#endif
  }
  bool expects_json() const override { return true; }
  bool process_json(std::string topic, JsonObject root) override;
  bool process(std::string topic, std::string payload) override;
};

}  // namespace cloud_mqtt
}  // namespace esphome

#endif  // USE_MQTT
