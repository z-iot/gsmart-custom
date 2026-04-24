import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
import esphome.components.mqtt as mqtt
from esphome.const import (
    CONF_ID,
    CONF_TOPIC,
    CONF_TRIGGER_ID,
)

AUTO_LOAD = ["mqtt"]
# DEPENDENCIES = ["mqtt"]
# AUTO_LOAD = ["json"]

CONF_ON_CMD = "on_cmd"
CONF_ON_JSON_CMD = "on_json_cmd"
CONF_ON_JSON_CMD_REGION = "on_json_cmd_region"

smart_mqtt_ns = cg.esphome_ns.namespace("smart_mqtt")
SmartMqttComponent = smart_mqtt_ns.class_("SmartMqtt", cg.Component)

SmartCmdTrigger = smart_mqtt_ns.class_(
    "SmartCmdTrigger", automation.Trigger.template(cg.std_string)
)
SmartJsonCmdTrigger = smart_mqtt_ns.class_(
    "SmartJsonCmdTrigger", automation.Trigger.template(cg.JsonObject)
)
SmartJsonCmdRegionTrigger = smart_mqtt_ns.class_(
    "SmartJsonCmdRegionTrigger", automation.Trigger.template(cg.JsonObject)
)

CONFIG_SCHEMA = cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SmartMqttComponent),

            cv.Optional(CONF_ON_CMD): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SmartCmdTrigger),
                    cv.Required(CONF_TOPIC): cv.subscribe_topic,
                }
            ),
            cv.Optional(CONF_ON_JSON_CMD): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SmartJsonCmdTrigger),
                    cv.Required(CONF_TOPIC): cv.subscribe_topic,
                }
            ),
            cv.Optional(CONF_ON_JSON_CMD_REGION): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SmartJsonCmdRegionTrigger),
                    cv.Required(CONF_TOPIC): cv.subscribe_topic,
                }
            ),
        }
    )


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    # cg.add(var.set_qos(config[CONF_QOS]))

    for conf in config.get(CONF_ON_CMD, []):
        trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        cg.add(trig.set_topic(conf[CONF_TOPIC]))
        await automation.build_automation(trig, [(cg.std_string, "x")], conf)

    for conf in config.get(CONF_ON_JSON_CMD, []):
        trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        cg.add(trig.set_topic(conf[CONF_TOPIC]))
        await automation.build_automation(trig, [(cg.JsonObject, "x")], conf)        

    for conf in config.get(CONF_ON_JSON_CMD_REGION, []):
        trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        cg.add(trig.set_topic(conf[CONF_TOPIC]))
        await automation.build_automation(trig, [(cg.JsonObject, "x")], conf)    