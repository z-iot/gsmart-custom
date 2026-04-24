from esphome.components import switch
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_SWITCH_DATAPOINT
from .. import sbus_ns, CONF_SBUS_ID, Sbus

DEPENDENCIES = ["sbus"]
CODEOWNERS = ["@grid"]

SbusSwitch = sbus_ns.class_("SbusSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SbusSwitch),
        cv.GenerateID(CONF_SBUS_ID): cv.use_id(Sbus),
        cv.Required(CONF_SWITCH_DATAPOINT): cv.uint8_t,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await switch.register_switch(var, config)

    paren = await cg.get_variable(config[CONF_SBUS_ID])
    cg.add(var.set_sbus_parent(paren))

    cg.add(var.set_switch_id(config[CONF_SWITCH_DATAPOINT]))
