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

namespace {
bool payload_starts_json_object(const std::string &payload) {
  for (char c : payload) {
    if (c == ' ' || c == '\n' || c == '\r' || c == '\t')
      continue;
    return c == '{';
  }
  return false;
}

std::string extract_msg_id(JsonObject root) {
  const char *msg_id = root["msg_id"] | "";
  return msg_id;
}
}  // namespace

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

bool CloudMqtt::publish_cloud_json(const std::string &suffix, const json::json_build_t &f, uint8_t qos, bool retain) {
  if (mqtt::global_mqtt_client == nullptr) {
    ESP_LOGW(TAG, "MQTT client is not available, skip publish to %s", suffix.c_str());
    return false;
  }
  return this->publish_json(this->getCloudTopic(suffix), f, qos, retain);
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
  if (!this->dispatch_command_(this->cmd_device_listeners_, topic, payload))
    ESP_LOGW(TAG, "Received command for unknown device topic: %s", topic.c_str());
}

bool CloudMqtt::dispatch_command_(const std::vector<CloudCmdListener *> &listeners, const std::string &topic,
                                  const std::string &payload) {
  for (auto *cmd_listener : listeners) {
    if (!cmd_listener->matches(topic))
      continue;

    if (cmd_listener->expects_json()) {
      bool handled = false;
      std::string msg_id;
      bool parsed = json::parse_json(payload, [cmd_listener, &topic, &handled, &msg_id](JsonObject root) -> bool {
        msg_id = extract_msg_id(root);
        handled = cmd_listener->process_json(topic, root);
        return true;
      });

      if (!parsed) {
        ESP_LOGW(TAG, "Invalid JSON command payload for topic: %s", topic.c_str());
        return true;
      }

      if (!msg_id.empty()) {
        this->publish_ack_(msg_id, handled ? "accepted" : "rejected", handled ? nullptr : "command_not_handled");
      }
      return true;
    }

    std::string msg_id;
    if (payload_starts_json_object(payload)) {
      json::parse_json(payload, [&msg_id](JsonObject root) -> bool {
        msg_id = extract_msg_id(root);
        return true;
      });
    }

    bool handled = cmd_listener->process(topic, payload);
    if (!msg_id.empty())
      this->publish_ack_(msg_id, handled ? "accepted" : "rejected", handled ? nullptr : "command_not_handled");
    return true;
  }

  if (payload_starts_json_object(payload)) {
    std::string msg_id;
    json::parse_json(payload, [&msg_id](JsonObject root) -> bool {
      msg_id = extract_msg_id(root);
      return true;
    });
    if (!msg_id.empty())
      this->publish_ack_(msg_id, "rejected", "unknown_command");
  }
  return false;
}

bool CloudMqtt::publish_ack_(const std::string &msg_id, const char *result, const char *error) {
  std::string device_id = storage::store != nullptr ? storage::store->get_serial() : get_mac_address().substr(6);
  return this->publish_cloud_json("ack", [msg_id, device_id, result, error](JsonObject root) {
    root["msg_id"] = msg_id;
    root["device_id"] = device_id;
    root["result"] = result;
    if (error == nullptr) {
      root["error"] = nullptr;
    } else {
      root["error"] = error;
    }
  });
}

#ifdef GSMART_FEATURE_REGION
void CloudMqtt::processRegionCommand(std::string topic, std::string payload) {
  if (!this->dispatch_command_(this->cmd_region_listeners_, topic, payload))
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

  json::parse_json(payload, [this, topic](JsonObject root) -> bool { return this->process_json(topic, root); });

  return true;
}

bool CloudJsonCmdTrigger::process_json(std::string topic, JsonObject root) {
  if (this->cmd_topic_ != topic)
    return false;
  this->trigger(root);
  return true;
}

bool CloudJsonCmdRegionTrigger::process(std::string topic, std::string payload) {
  if (this->cmd_topic_ != topic)
    return false;

  json::parse_json(payload, [this, topic](JsonObject root) -> bool { return this->process_json(topic, root); });

  return true;
}

bool CloudJsonCmdRegionTrigger::process_json(std::string topic, JsonObject root) {
  if (this->cmd_topic_ != topic)
    return false;
  this->trigger(root);
  return true;
}

}  // namespace cloud_mqtt
}  // namespace esphome

#endif  // USE_MQTT
