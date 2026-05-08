#include "api_adapter_mqtt.h"
#include "esphome/core/log.h"
#include "esphome/components/storage/store.h"
#include "esphome/components/json/json_util.h"

namespace esphome {
namespace api_adapter_mqtt {

static const char *const TAG = "api_adapter_mqtt";

void ApiAdapterMqtt::setup() {
  if (mqtt::global_mqtt_client == nullptr) {
    this->mark_failed();
    return;
  }

  std::string serial = storage::store != nullptr ? storage::store->get_serial() : "unknown";
  this->base_topic_ = "gsmart/mobile/" + serial + "/" + this->core_->get_version_path();

  this->subscribe_topics_();
}

void ApiAdapterMqtt::subscribe_topics_() {
  if (this->subscribed_)
    return;

  ESP_LOGD(TAG, "Subscribing to API topics: %s/#", this->base_topic_.c_str());
  mqtt::global_mqtt_client->subscribe(
      this->base_topic_ + "/#",
      [this](const std::string &topic, const std::string &payload) { this->on_message_(topic, payload); });
  this->subscribed_ = true;
}

void ApiAdapterMqtt::on_message_(const std::string &topic, const std::string &payload) {
  if (topic.compare(0, this->base_topic_.length(), this->base_topic_) != 0)
    return;

  std::string sub_topic = topic.substr(this->base_topic_.length() + 1);
  ESP_LOGD(TAG, "Received API MQTT message: %s, payload: %s", sub_topic.c_str(), payload.c_str());

  json::parse_json(payload, [this, sub_topic](JsonObject root) -> bool {
    JsonVariant rid = root["rid"];

    if (sub_topic == "info/get") {
      this->publish_response_("info/state", rid, [this](JsonObject res) { this->core_->build_info(res); });
    } else if (sub_topic == "status/get") {
      this->publish_response_("status/state", rid, [this](JsonObject res) { this->core_->build_status(res); });
    } else if (sub_topic == "diagnostics/get") {
      this->publish_response_("diagnostics/state", rid, [this](JsonObject res) { this->core_->build_diagnostics(res); });
    } else if (sub_topic == "consumption/get") {
      this->publish_response_("consumption/state", rid, [this](JsonObject res) { this->core_->build_consumption(res); });
    } else if (sub_topic == "control/mode/set") {
      this->publish_response_("control/mode/res", rid, [this, root](JsonObject res) { this->core_->handle_control_mode(root, res); });
    } else if (sub_topic == "control/identify/set") {
      this->publish_response_("control/identify/res", rid, [this, root](JsonObject res) { this->core_->handle_identify(root, res); });
    }
    // Add more handlers as needed
    return true;
  });
}

void ApiAdapterMqtt::publish_response_(const std::string &suffix, JsonVariant rid, std::function<void(JsonObject)> builder) {
  std::string full_topic = this->base_topic_ + "/" + suffix;
  mqtt::global_mqtt_client->publish_json(full_topic, [rid, builder](JsonObject root) {
    if (!rid.isNull()) {
      root["rid"] = rid;
    }
    builder(root);
  });
}

}  // namespace api_adapter_mqtt
}  // namespace esphome
