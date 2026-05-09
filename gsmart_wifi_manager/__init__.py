import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import wifi
from esphome.const import CONF_ID, CONF_NETWORKS, CONF_SSID, CONF_PASSWORD

DEPENDENCIES = ["wifi"]

gsmart_wifi_manager_ns = cg.esphome_ns.namespace("gsmart_wifi_manager")
GsmartWifiManager = gsmart_wifi_manager_ns.class_("GsmartWifiManager", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(GsmartWifiManager),
    cv.Optional(CONF_NETWORKS): cv.ensure_list(wifi.WIFI_NETWORK_SCHEMA),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_component_variable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_NETWORKS in config:
        for network in config[CONF_NETWORKS]:
            cg.add(var.add_manufacture_network(network[CONF_SSID], network.get(CONF_PASSWORD, "")))
