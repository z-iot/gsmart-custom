import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@grid"]
# `wifi` je tu preto, že sken číta stav linky z toho istého miesta ako zvyšok
# API (`wifi::global_wifi_component`), nie z Arduino WiFi triedy.
DEPENDENCIES = ["network", "wifi"]
AUTO_LOAD = ["json"]

lan_scan_ns = cg.esphome_ns.namespace("lan_scan")
LanScanComponent = lan_scan_ns.class_("LanScanComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LanScanComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add_define("USE_GSMART_LAN_SCAN")
