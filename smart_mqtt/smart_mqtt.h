#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT

#include "esphome/core/component.h"
#include "esphome/components/mqtt/mqtt_client.h"

namespace esphome
{
    namespace smart_mqtt
    {
        class SmartMqtt;

        class SmartCmdListener
        {
        public:
            virtual bool process(std::string topic, std::string payload) = 0;
            void set_parent(SmartMqtt *parent) { parent_ = parent; }
            void set_topic(const std::string &topic) { this->cmdTopic_ = topic; }

        protected:
            SmartMqtt *parent_{nullptr};
            std::string cmdTopic_{};
        };

        class SmartMqtt : public Component
        {
        public:
            void setup() override;
            void dump_config() override;
            float get_setup_priority() const override;

            std::string getSmartTopicBase();
            std::string getSmartTopic(std::string suffix);

#ifdef GSMART_FEATURE_REGION
            void regionSubscribe(std::string regionSerial);
            void regionUnsubscribe(std::string regionSerial);
#endif
            void register_cmd_device_listener(SmartCmdListener *cmdListener)
            {
                cmdListener->set_parent(this);
                this->cmdDeviceListeners_.push_back(cmdListener);
            }
#ifdef GSMART_FEATURE_REGION
            void register_cmd_region_listener(SmartCmdListener *cmdListener)
            {
                cmdListener->set_parent(this);
                this->cmdRegionListeners_.push_back(cmdListener);
            }
#endif

        protected:
            void processDeviceCommand(std::string topic, std::string cmd);
            std::vector<SmartCmdListener *> cmdDeviceListeners_;

#ifdef GSMART_FEATURE_REGION
            void processRegionCommand(std::string topic, std::string cmd);
            std::vector<SmartCmdListener *> cmdRegionListeners_;
#endif
        };

        class SmartCmdTrigger : public Trigger<std::string>, public SmartCmdListener
        {
        public:
            explicit SmartCmdTrigger(SmartMqtt *parent) { parent->register_cmd_device_listener(this); }
            bool process(std::string topic, std::string payload) override;
        };

        class SmartJsonCmdTrigger : public Trigger<JsonObject>, public SmartCmdListener
        {
        public:
            explicit SmartJsonCmdTrigger(SmartMqtt *parent) { parent->register_cmd_device_listener(this); }
            bool process(std::string topic, std::string payload) override;
        };

// #ifdef GSMART_FEATURE_REGION
        class SmartJsonCmdRegionTrigger : public Trigger<JsonObject>, public SmartCmdListener
        {
        public:
            explicit SmartJsonCmdRegionTrigger(SmartMqtt *parent) 
            { 
#ifdef GSMART_FEATURE_REGION
                parent->register_cmd_region_listener(this); 
#endif
            }
            bool process(std::string topic, std::string payload) override;
        };
// #endif
    }
}
#endif
