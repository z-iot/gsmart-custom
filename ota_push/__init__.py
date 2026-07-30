import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@grid"]
DEPENDENCIES = ["http_update", "udp_server", "network"]
# sha256: OTA protocol 2 authenticates with SHA256 and rejects a client that does
# not offer it. md5 stays for protocol 1 targets and for the image digest.
AUTO_LOAD = ["md5", "sha256"]

CONF_OTA_PASSWORD = "ota_password"

ota_push_ns = cg.esphome_ns.namespace("ota_push")
OtaPushComponent = ota_push_ns.class_("OtaPushComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(OtaPushComponent),
        cv.Optional(CONF_OTA_PASSWORD, default="promoss1"): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_ota_password(config[CONF_OTA_PASSWORD]))
    cg.add_define("USE_GSMART_OTA_PUSH")
