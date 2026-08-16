import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import time as time_
from esphome.const import CONF_ID, CONF_TIME_ID
from ..api_core_v1 import ApiCoreV1

CODEOWNERS = ["@max-iot"]
DEPENDENCIES = ["api_core_v1", "http_update", "wifi"]
AUTO_LOAD = ["json"]

auto_update_ns = cg.esphome_ns.namespace("auto_update")
AutoUpdateComponent = auto_update_ns.class_("AutoUpdateComponent", cg.Component)

CONF_API_CORE_ID = "api_core_id"
CONF_BOARD_URL = "board_url"
CONF_PROMOSS_SECRET = "promoss_secret"
CONF_CHANNEL = "channel"
CONF_WINDOW_START_HOUR = "window_start_hour"
CONF_WINDOW_END_HOUR = "window_end_hour"
CONF_ENABLED = "enabled"
CONF_TIMEOUT = "timeout"


def validate_board_url(value):
    value = cv.string(value)
    if not (value.startswith("http://") or value.startswith("https://")):
        raise cv.Invalid("board_url must start with http:// or https://")
    return value.rstrip("/")


def validate_window(config):
    start = config[CONF_WINDOW_START_HOUR]
    end = config[CONF_WINDOW_END_HOUR]
    if end <= start:
        raise cv.Invalid(
            "window_end_hour must be after window_start_hour. The window is what "
            "the per-device slot is spread across; an empty one would mean every "
            "device checks at the same second."
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AutoUpdateComponent),
            cv.Required(CONF_API_CORE_ID): cv.use_id(ApiCoreV1),
            cv.Required(CONF_TIME_ID): cv.use_id(time_.RealTimeClock),
            cv.Required(CONF_BOARD_URL): validate_board_url,
            cv.Required(CONF_PROMOSS_SECRET): cv.string,
            cv.Optional(CONF_CHANNEL, default="stable"): cv.string,
            cv.Optional(CONF_WINDOW_START_HOUR, default=1): cv.int_range(min=0, max=23),
            cv.Optional(CONF_WINDOW_END_HOUR, default=3): cv.int_range(min=1, max=24),
            cv.Optional(CONF_ENABLED, default=True): cv.boolean,
            cv.Optional(CONF_TIMEOUT, default="15s"): cv.positive_time_period_milliseconds,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate_window,
    # The ESP8266 build has neither http_update nor the room for a TLS session,
    # so a rex is updated by its region master over the LAN instead. Letting this
    # compile there would produce a device that checks every night and can never
    # install what it finds.
    cv.only_on_esp32,
    cv.only_with_arduino,
)


async def to_code(config):
    core = await cg.get_variable(config[CONF_API_CORE_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_api_core(core))
    cg.add(var.set_time(await cg.get_variable(config[CONF_TIME_ID])))
    cg.add(var.set_board_url(config[CONF_BOARD_URL]))
    cg.add(var.set_promoss_secret(config[CONF_PROMOSS_SECRET]))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_window(config[CONF_WINDOW_START_HOUR], config[CONF_WINDOW_END_HOUR]))
    cg.add(var.set_enabled(config[CONF_ENABLED]))
    cg.add(var.set_timeout_ms(config[CONF_TIMEOUT].total_milliseconds))
    cg.add_define("USE_GSMART_AUTO_UPDATE")
