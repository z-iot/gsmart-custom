import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import web_server_base
from ..api_core_v1 import ApiCoreV1, api_core_v1_ns

CODEOWNERS = ["@max-iot"]
DEPENDENCIES = ["web_server_base", "api_core_v1"]

api_adapter_rest_ns = cg.esphome_ns.namespace("api_adapter_rest")
ApiAdapterRest = api_adapter_rest_ns.class_("ApiAdapterRest", cg.Component)
arduino_json_ns = cg.global_ns.namespace("ArduinoJson")

CONF_API_CORE_ID = "api_core_id"
CONF_WEB_SERVER_BASE_ID = "web_server_base_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ApiAdapterRest),
        cv.Required(CONF_API_CORE_ID): cv.use_id(ApiCoreV1),
        cv.Required(CONF_WEB_SERVER_BASE_ID): cv.use_id(web_server_base.WebServerBase),
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    core = await cg.get_variable(config[CONF_API_CORE_ID])
    base = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
    var = cg.new_Pvariable(config[CONF_ID], base, core)
    await cg.register_component(var, config)
