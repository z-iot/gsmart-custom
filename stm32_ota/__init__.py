import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.const import CONF_ID

AUTO_LOAD = ["web_server_base"]

stm32_ota_ns = cg.esphome_ns.namespace("stm32_ota")
STM32OTA = stm32_ota_ns.class_("STM32OTA", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(STM32OTA),
        cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
            web_server_base.WebServerBase
        ),
    },
    cv.only_with_arduino,
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    cg.add_define("USE_STM32_OTA")

    var = cg.new_Pvariable(config[CONF_ID], paren)
    await cg.register_component(var, config)
