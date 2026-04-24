import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.components import web_server_base
from esphome.core import CORE

AUTO_LOAD = ["web_server_base"]

rest_server_ns = cg.esphome_ns.namespace("rest_server")
RestServer = rest_server_ns.class_("RestServer", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RestServer),
        cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
            web_server_base.WebServerBase
        ),
    },
    cv.only_with_arduino,
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    cg.add_define("USE_RESTSERVER")

    var = cg.new_Pvariable(config[CONF_ID], paren)
    await cg.register_component(var, config)

    if CORE.using_arduino:
        cg.add_library("ArduinoJWT", None)
        