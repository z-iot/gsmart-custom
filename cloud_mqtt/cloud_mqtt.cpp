#include "cloud_mqtt.h"

#ifdef USE_MQTT

#include "esphome/components/network/util.h"
#include "esphome/components/storage/store.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cloud_mqtt {

static const char *const TAG = "cloud_mqtt";

std::string CloudMqtt::getCloudTopicBase() {
  if (!this->topic_base_.empty())
    return this->topic_base_;

  std::string model = storage::store != nullptr ? storage::store->get_model() : App.get_name();
  std::string serial = storage::store != nullptr ? storage::store->get_serial() : get_mac_address().substr(6);
  return model + "/" + serial;
}

std::string CloudMqtt::getCloudTopic(const std::string &suffix) {
  if (suffix.empty())
    return this->getCloudTopicBase();
  return this->getCloudTopicBase() + "/" + suffix;
}

void CloudMqtt::setup() {
  this->topic_base_ = this->getCloudTopicBase();
  this->configure_mqtt_client_();
  this->subscribe_device_commands_();

#ifdef GSMART_FEATURE_REGION
  if (storage::store != nullptr && storage::store->region != nullptr) {
    this->regionSubscribe(storage::convertRegionSerialtoStr(storage::store->region->layout.serial));
  }
#endif
}

void CloudMqtt::loop() {
  if (!this->configured_ || mqtt::global_mqtt_client == nullptr)
    return;

  if (network::is_connected()) {
    mqtt::global_mqtt_client->enable();
  } else {
    mqtt::global_mqtt_client->disable();
  }
}

void CloudMqtt::configure_mqtt_client_() {
  if (mqtt::global_mqtt_client == nullptr) {
    this->mark_failed();
    ESP_LOGE(TAG, "MQTT client is not available");
    return;
  }

  mqtt::global_mqtt_client->set_birth_message(mqtt::MQTTMessage{
      .topic = this->getCloudTopic("status"),
      .payload = "online",
      .qos = 0,
      .retain = true,
  });
  mqtt::global_mqtt_client->set_last_will(mqtt::MQTTMessage{
      .topic = this->getCloudTopic("status"),
      .payload = "offline",
      .qos = 0,
      .retain = true,
  });
  mqtt::global_mqtt_client->set_shutdown_message(mqtt::MQTTMessage{
      .topic = this->getCloudTopic("status"),
      .payload = "offline",
      .qos = 0,
      .retain = true,
  });
  mqtt::global_mqtt_client->set_log_message_template(mqtt::MQTTMessage{
      .topic = "",
      .payload = "",
      .qos = 0,
      .retain = false,
  });

  this->configured_ = true;
}

void CloudMqtt::subscribe_device_commands_() {
  if (mqtt::global_mqtt_client == nullptr || this->device_commands_subscribed_)
    return;

  this->device_command_topic_prefix_ = this->getCloudTopic("cmd/");
  ESP_LOGD(TAG, "Subscribe to device topic: %s", (this->device_command_topic_prefix_ + "#").c_str());
  this->subscribe(this->device_command_topic_prefix_ + "#", &CloudMqtt::on_device_command_);

  this->device_commands_subscribed_ = true;
}

void CloudMqtt::on_device_command_(const std::string &topic, const std::string &payload) {
  if (topic.compare(0, this->device_command_topic_prefix_.length(), this->device_command_topic_prefix_) != 0)
    return;

  std::string t = topic.substr(this->device_command_topic_prefix_.length());
  ESP_LOGD(TAG, "Received device cmd topic: %s, payload: %s", t.c_str(), payload.c_str());
  this->defer([this, t, payload]() { this->processDeviceCommand(t, payload); });
}

void CloudMqtt::processDeviceCommand(std::string topic, std::string payload) {
  bool found = false;
  for (auto *cmd_listener : this->cmd_device_listeners_) {
    if (cmd_listener->process(topic, payload)) {
      found = true;
      break;
    }
  }
  if (!found)
    ESP_LOGW(TAG, "Received command for unknown device topic: %s", topic.c_str());
}

#ifdef GSMART_FEATURE_REGION
void CloudMqtt::processRegionCommand(std::string topic, std::string payload) {
  bool found = false;
  for (auto *cmd_listener : this->cmd_region_listeners_) {
    if (cmd_listener->process(topic, payload)) {
      found = true;
      break;
    }
  }
  if (!found)
    ESP_LOGW(TAG, "Received command for unknown region topic: %s", topic.c_str());
}
#endif

float CloudMqtt::get_setup_priority() const { return setup_priority::BEFORE_CONNECTION; }

void CloudMqtt::dump_config() {
  ESP_LOGCONFIG(TAG, "Cloud MQTT:");
  ESP_LOGCONFIG(TAG, "  Topic Base: %s", this->getCloudTopicBase().c_str());
}

#ifdef GSMART_FEATURE_REGION
void CloudMqtt::regionSubscribe(std::string region_serial) {
  if (region_serial.empty() || mqtt::global_mqtt_client == nullptr)
    return;

  this->region_command_topic_prefix_ = "region/" + region_serial + "/cmd/";
  ESP_LOGD(TAG, "Subscribe to region topic: %s", (this->region_command_topic_prefix_ + "#").c_str());
  this->subscribe(this->region_command_topic_prefix_ + "#", &CloudMqtt::on_region_command_);
}

void CloudMqtt::regionUnsubscribe(std::string region_serial) {
  if (region_serial.empty() || mqtt::global_mqtt_client == nullptr)
    return;
  mqtt::global_mqtt_client->unsubscribe("region/" + region_serial + "/cmd/#");
}

void CloudMqtt::on_region_command_(const std::string &topic, const std::string &payload) {
  if (topic.compare(0, this->region_command_topic_prefix_.length(), this->region_command_topic_prefix_) != 0)
    return;

  std::string t = topic.substr(this->region_command_topic_prefix_.length());
  this->defer([this, t, payload]() { this->processRegionCommand(t, payload); });
}
#endif

bool CloudCmdTrigger::process(std::string topic, std::string payload) {
  if (this->cmd_topic_ != topic)
    return false;
  this->trigger(payload);
  return true;
}

bool CloudJsonCmdTrigger::process(std::string topic, std::string payload) {
  if (this->cmd_topic_ != topic)
    return false;

  json::parse_json(payload, [this](JsonObject root) -> bool {
    this->trigger(root);
    return true;
  });

  return true;
}

bool CloudJsonCmdRegionTrigger::process(std::string topic, std::string payload) {
  if (this->cmd_topic_ != topic)
    return false;

  json::parse_json(payload, [this](JsonObject root) -> bool {
    this->trigger(root);
    return true;
  });

  return true;
}

}  // namespace cloud_mqtt
}  // namespace esphome

#endif  // USE_MQTT
