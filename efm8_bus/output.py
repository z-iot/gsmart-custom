import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_CHANNEL, CONF_ID
from . import efm8_bus_ns, Efm8BusComponent, CONF_EFM8_BUS_ID

DEPENDENCIES = ["efm8_bus"]

Efm8Output = efm8_bus_ns.class_("Efm8Output", output.FloatOutput)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_EFM8_BUS_ID): cv.use_id(Efm8BusComponent),
        cv.Required(CONF_ID): cv.declare_id(Efm8Output),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=2),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await output.register_output(var, config)

    parent = await cg.get_variable(config[CONF_EFM8_BUS_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
