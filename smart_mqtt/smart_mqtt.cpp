#include "smart_mqtt.h"
#include "esphome/components/storage/store.h"

#ifdef USE_MQTT

#include "esphome/core/log.h"

namespace esphome
{
  namespace smart_mqtt
  {

    static const char *const TAG = "smart_mqtt";

    std::string SmartMqtt::getSmartTopicBase()
    {
      return storage::store->get_model() + "/" + storage::store->get_serial();
    }

    std::string SmartMqtt::getSmartTopic(std::string suffix)
    {
      return this->getSmartTopicBase() + "/" + suffix;
    }

    void SmartMqtt::setup()
    {
      std::string topicBase = getSmartTopicBase();
      mqtt::global_mqtt_client->set_topic_prefix(topicBase, ""); // TODO: add prefix

      mqtt::global_mqtt_client->set_birth_message(mqtt::MQTTMessage{
          .topic = topicBase + "/status",
          .payload = "online",
          .qos = 0,
          .retain = true,
      });
      mqtt::global_mqtt_client->set_last_will(mqtt::MQTTMessage{
          .topic = topicBase + "/status",
          .payload = "offline",
          .qos = 0,
          .retain = true,
      });
      mqtt::global_mqtt_client->set_shutdown_message(mqtt::MQTTMessage{
          .topic = topicBase + "/status",
          .payload = "offline",
          .qos = 0,
          .retain = true,
      });
      mqtt::global_mqtt_client->set_log_message_template(mqtt::MQTTMessage{
          .topic = "",
          .payload = "",
          .qos = 0,
          .retain = true,
      });

      mqtt::global_mqtt_client->subscribe(
          this->getSmartTopic("cmd/#"),
          [this](const std::string &topic, const std::string &payload)
          {
        int prefixLen = this->getSmartTopic("cmd/").length();
        std::string t = topic.substr(prefixLen);
        ESP_LOGD(TAG, "Received device cmd topic: %s, payload: %s", t.c_str(), payload.c_str());
        this->defer([this, t, payload]()
                    { this->processDeviceCommand(t, payload); }); });

#ifdef GSMART_FEATURE_REGION
        regionSubscribe(storage::convertRegionSerialtoStr(storage::store->region->layout.serial));
#endif  
    }

    void SmartMqtt::processDeviceCommand(std::string topic, std::string payload)
    {
      bool found = false;
      for (auto *cmdListener : this->cmdDeviceListeners_)
      {
        if (cmdListener->process(topic, payload))
        {
          found = true;
          break;
        }
      }
      if (!found)
        ESP_LOGW(TAG, "Received command for unknown device topic: %s", topic.c_str());
    }

#ifdef GSMART_FEATURE_REGION
    void SmartMqtt::processRegionCommand(std::string topic, std::string payload)
    {
      bool found = false;
      for (auto *cmdListener : this->cmdRegionListeners_)
      {
        if (cmdListener->process(topic, payload))
        {
          found = true;
          break;
        }
      }
      if (!found)
        ESP_LOGW(TAG, "Received command for unknown region topic: %s", topic.c_str());
    }
#endif  

    float SmartMqtt::get_setup_priority() const
    {
      return setup_priority::BEFORE_CONNECTION;
    }
    void SmartMqtt::dump_config()
    {
      // TODO
      //  LOG_SENSOR("", "Smart MQTT", this);
      //  ESP_LOGCONFIG(TAG, "  Topic: %s", this->topic_.c_str());
    }

#ifdef GSMART_FEATURE_REGION
    void SmartMqtt::regionSubscribe(std::string regionSerial)
    {
      if (regionSerial == "")
        return;
      std::string topicBase = "region/" + regionSerial + "/cmd/";
      ESP_LOGD(TAG, "Subcribe to region topic: %s", (topicBase + "#").c_str());
      mqtt::global_mqtt_client->subscribe(
          topicBase + "#",
          [this, topicBase](const std::string &topic, const std::string &payload)
          {
        int prefixLen = topicBase.length();
        std::string t = topic.substr(prefixLen);
        // ESP_LOGD(TAG, "Received region cmd topic: %s, payload: %s", t.c_str(), payload.c_str());
        this->defer([this, t, payload]()
                    { this->processRegionCommand(t, payload); }); });
    }

    void SmartMqtt::regionUnsubscribe(std::string regionSerial)
    {
      if (regionSerial == "")
        return;
      mqtt::global_mqtt_client->unsubscribe("region/" + regionSerial + "/cmd/#");
    }
#endif  

    bool SmartCmdTrigger::process(std::string topic, std::string payload)
    {
      if (this->cmdTopic_ != topic)
        return false;
      this->trigger(payload);
      return true;
    }

    bool SmartJsonCmdTrigger::process(std::string topic, std::string payload)
    {
      if (this->cmdTopic_ != topic)
        return false;

      json::parse_json(payload, [this](JsonObject root) -> bool {
        this->trigger(root);
        return true;
      });

      return true;
    }

// #ifdef GSMART_FEATURE_REGION
    bool SmartJsonCmdRegionTrigger::process(std::string topic, std::string payload)
    {
      if (this->cmdTopic_ != topic)
        return false;

      json::parse_json(payload, [this](JsonObject root) -> bool {
        this->trigger(root);
        return true;
      });

      return true;
    }
// #endif  

  }
}

#endif
