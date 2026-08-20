import logging
from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_URL
from esphome.core import CORE
from ..api_core_v1 import ApiCoreV1

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@max-iot"]
DEPENDENCIES = ["api_core_v1", "wifi"]
AUTO_LOAD = ["json"]

api_adapter_glink_ns = cg.esphome_ns.namespace("api_adapter_glink")
ApiAdapterGLink = api_adapter_glink_ns.class_("ApiAdapterGLink", cg.Component)

CONF_API_CORE_ID = "api_core_id"
CONF_PROMOSS_SECRET = "promoss_secret"
CONF_HEARTBEAT_INTERVAL = "heartbeat_interval"
CONF_FULL_HEARTBEAT_INTERVAL = "full_heartbeat_interval"
CONF_TLS_CA_CERT = "tls_ca_cert"


def validate_ws_url(value):
    value = cv.string(value)
    if not (value.startswith("ws://") or value.startswith("wss://")):
        raise cv.Invalid("G-Link device url must start with ws:// or wss://")
    if value.startswith("wss://") and CORE.is_esp8266:
        raise cv.Invalid(
            "wss:// cannot work on the ESP8266. The WebSockets library leaves BearSSL at its "
            "default 16 kB receive buffer, which has to come out of one contiguous block, and a "
            "REX has roughly 15 kB of largest free block left. The TLS handshake never completes, "
            "so the device retries forever and never reaches authentication - it just looks "
            "offline. Point this build at a plain ws:// G-Link endpoint instead."
        )
    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ApiAdapterGLink),
            cv.Required(CONF_API_CORE_ID): cv.use_id(ApiCoreV1),
            cv.Required(CONF_URL): validate_ws_url,
            cv.Required(CONF_PROMOSS_SECRET): cv.string,
            cv.Optional(CONF_TLS_CA_CERT, default=""): cv.string,
            cv.Optional(CONF_HEARTBEAT_INTERVAL, default="20s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_FULL_HEARTBEAT_INTERVAL, default="5min"): cv.positive_time_period_milliseconds,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_arduino,
)


async def to_code(config):
    core = await cg.get_variable(config[CONF_API_CORE_ID])
    var = cg.new_Pvariable(config[CONF_ID], core)
    await cg.register_component(var, config)

    cg.add(var.set_url(config[CONF_URL]))
    cg.add(var.set_promoss_secret(config[CONF_PROMOSS_SECRET]))
    cg.add(var.set_tls_ca_cert(config[CONF_TLS_CA_CERT]))
    cg.add(var.set_heartbeat_interval(config[CONF_HEARTBEAT_INTERVAL].total_milliseconds))
    cg.add(var.set_full_heartbeat_interval(config[CONF_FULL_HEARTBEAT_INTERVAL].total_milliseconds))
    add_websockets_library()


def add_websockets_library():
    """Declare the WebSockets dependency - vendored on the ESP8266, from the registry elsewhere.

    A REX has to build the library without its TLS half or BearSSL eats 100 kB of a 1 MB flash for
    a wss:// path validate_ws_url() refuses anyway, and there is no way to ask the registry copy for
    that. `components/websockets-override/` is the same 2.7.2 release with three marked hunks; its
    README carries the numbers. Keep both arms on the same version.

    The override lives in the firmware repository rather than in this submodule, so a checkout used
    on its own still builds - it just builds the registry copy and a REX image too big to OTA. Say
    so rather than let that be discovered from a size.
    """
    if not CORE.is_esp8266:
        cg.add_library("links2004/WebSockets", "2.7.2")
        return

    override = Path(CORE.relative_config_path("components/websockets-override"))
    if not (override / "library.json").is_file():
        _LOGGER.warning(
            "components/websockets-override is missing; building the ESP8266 against the registry "
            "WebSockets. That links BearSSL, which no REX can use, and the image grows by about "
            "100 kB - past the point where it still fits through OTA."
        )
        cg.add_library("links2004/WebSockets", "2.7.2")
        return

    cg.add_library("WebSockets", None, f"symlink://{override.resolve().as_posix()}")
