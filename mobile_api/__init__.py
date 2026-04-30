import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.const import CONF_ID
from pathlib import Path

AUTO_LOAD = ["web_server_base", "gsmart_server"]

mobile_api_ns = cg.esphome_ns.namespace("mobile_api")
MobileApi = mobile_api_ns.class_("MobileApi", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MobileApi),
        cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
            web_server_base.WebServerBase
        ),
    },
    cv.only_with_arduino,
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    cg.add_define("USE_MOBILE_API")
    component_dir = Path(__file__).parent.resolve().as_posix()
    cg.add_build_flag(f"-I{component_dir}")

    var = cg.new_Pvariable(config[CONF_ID], paren)
    await cg.register_component(var, config)
