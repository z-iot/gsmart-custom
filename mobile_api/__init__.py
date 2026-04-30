import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.const import CONF_ID, CONF_TRIGGER_ID

AUTO_LOAD = ["web_server_base", "gsmart_server"]
CONF_ON_IDENTIFY = "on_identify"

mobile_api_ns = cg.esphome_ns.namespace("mobile_api")
MobileApi = mobile_api_ns.class_("MobileApi", cg.Component)
IdentifyRequest = mobile_api_ns.struct("IdentifyRequest")
IdentifyTrigger = mobile_api_ns.class_(
    "IdentifyTrigger", automation.Trigger.template(IdentifyRequest)
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MobileApi),
        cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
            web_server_base.WebServerBase
        ),
        cv.Optional(CONF_ON_IDENTIFY): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(IdentifyTrigger),
            }
        ),
    },
    cv.only_with_arduino,
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    cg.add_define("USE_MOBILE_API")

    var = cg.new_Pvariable(config[CONF_ID], paren)
    await cg.register_component(var, config)

    for conf in config.get(CONF_ON_IDENTIFY, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(IdentifyRequest, "request")], conf)
