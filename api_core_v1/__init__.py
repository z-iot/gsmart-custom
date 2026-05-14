import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_TRIGGER_ID

CODEOWNERS = ["@max-iot"]
DEPENDENCIES = ["json"]

api_core_v1_ns = cg.esphome_ns.namespace("api_core_v1")
ApiCoreV1 = api_core_v1_ns.class_("ApiCoreV1", cg.Component)
IdentifyRequest = api_core_v1_ns.struct("IdentifyRequest")
IdentifyTrigger = api_core_v1_ns.class_("IdentifyTrigger", automation.Trigger.template(IdentifyRequest))

CONF_ON_IDENTIFY = "on_identify"
CONF_FIRMWARE_VERSION = "firmware_version"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ApiCoreV1),
        cv.Optional(CONF_FIRMWARE_VERSION, default=""): cv.string,
        cv.Optional(CONF_ON_IDENTIFY): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(IdentifyTrigger),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_firmware_version(config[CONF_FIRMWARE_VERSION]))

    for conf in config.get(CONF_ON_IDENTIFY, []):
        trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trig, [(IdentifyRequest, "x")], conf)
