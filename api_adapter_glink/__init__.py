import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_URL
from ..api_core_v1 import ApiCoreV1

CODEOWNERS = ["@max-iot"]
DEPENDENCIES = ["api_core_v1", "wifi"]
AUTO_LOAD = ["json"]

api_adapter_glink_ns = cg.esphome_ns.namespace("api_adapter_glink")
ApiAdapterGLink = api_adapter_glink_ns.class_("ApiAdapterGLink", cg.Component)

CONF_API_CORE_ID = "api_core_id"
CONF_KEY_ID = "key_id"
CONF_SECRET = "secret"
CONF_HEARTBEAT_INTERVAL = "heartbeat_interval"


def validate_ws_url(value):
    value = cv.string(value)
    if not (value.startswith("ws://") or value.startswith("wss://")):
        raise cv.Invalid("G-Link device url must start with ws:// or wss://")
    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ApiAdapterGLink),
            cv.Required(CONF_API_CORE_ID): cv.use_id(ApiCoreV1),
            cv.Required(CONF_URL): validate_ws_url,
            cv.Required(CONF_KEY_ID): cv.string,
            cv.Required(CONF_SECRET): cv.string,
            cv.Optional(CONF_HEARTBEAT_INTERVAL, default="20s"): cv.positive_time_period_milliseconds,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
    cv.only_with_arduino,
)


async def to_code(config):
    core = await cg.get_variable(config[CONF_API_CORE_ID])
    var = cg.new_Pvariable(config[CONF_ID], core)
    await cg.register_component(var, config)

    cg.add(var.set_url(config[CONF_URL]))
    cg.add(var.set_key_id(config[CONF_KEY_ID]))
    cg.add(var.set_secret(config[CONF_SECRET]))
    cg.add(var.set_heartbeat_interval(config[CONF_HEARTBEAT_INTERVAL].total_milliseconds))
    cg.add_library("links2004/WebSockets", "2.7.2")
