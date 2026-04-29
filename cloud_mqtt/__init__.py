import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_TOPIC,
    CONF_TRIGGER_ID,
)

AUTO_LOAD = ["mqtt", "json"]
DEPENDENCIES = ["mqtt", "storage"]

CONF_ON_CMD = "on_cmd"
CONF_ON_JSON_CMD = "on_json_cmd"
CONF_ON_JSON_CMD_REGION = "on_json_cmd_region"

cloud_mqtt_ns = cg.esphome_ns.namespace("cloud_mqtt")
CloudMqttComponent = cloud_mqtt_ns.class_("CloudMqtt", cg.Component)

CloudCmdTrigger = cloud_mqtt_ns.class_(
    "CloudCmdTrigger", automation.Trigger.template(cg.std_string)
)
CloudJsonCmdTrigger = cloud_mqtt_ns.class_(
    "CloudJsonCmdTrigger", automation.Trigger.template(cg.JsonObject)
)
CloudJsonCmdRegionTrigger = cloud_mqtt_ns.class_(
    "CloudJsonCmdRegionTrigger", automation.Trigger.template(cg.JsonObject)
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CloudMqttComponent),
        cv.Optional(CONF_ON_CMD): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CloudCmdTrigger),
                cv.Required(CONF_TOPIC): cv.subscribe_topic,
            }
        ),
        cv.Optional(CONF_ON_JSON_CMD): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CloudJsonCmdTrigger),
                cv.Required(CONF_TOPIC): cv.subscribe_topic,
            }
        ),
        cv.Optional(CONF_ON_JSON_CMD_REGION): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    CloudJsonCmdRegionTrigger
                ),
                cv.Required(CONF_TOPIC): cv.subscribe_topic,
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

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
