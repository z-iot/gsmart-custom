import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from ..api_core_v1 import ApiCoreV1, api_core_v1_ns

CODEOWNERS = ["@max-iot"]
DEPENDENCIES = ["mqtt", "api_core_v1"]

api_adapter_mqtt_ns = cg.esphome_ns.namespace("api_adapter_mqtt")
ApiAdapterMqtt = api_adapter_mqtt_ns.class_("ApiAdapterMqtt", cg.Component)

CONF_API_CORE_ID = "api_core_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ApiAdapterMqtt),
        cv.Required(CONF_API_CORE_ID): cv.use_id(ApiCoreV1),
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    core = await cg.get_variable(config[CONF_API_CORE_ID])
    var = cg.new_Pvariable(config[CONF_ID], core)
    await cg.register_component(var, config)
