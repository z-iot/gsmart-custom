import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID

AUTO_LOAD = ["binary_sensor"]

CONF_EFM8_BUS_ID = "efm8_bus"
CONF_CLK_PIN = "clk_pin"
CONF_DATA_PIN = "data_pin"

efm8_bus_ns = cg.esphome_ns.namespace("efm8_bus")

Efm8BusComponent = efm8_bus_ns.class_("Efm8BusComponent", cg.Component)

MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Efm8BusComponent),
        cv.Required(CONF_CLK_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_DATA_PIN): pins.gpio_input_pullup_pin_schema,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    clk = await cg.gpio_pin_expression(config[CONF_CLK_PIN])
    cg.add(var.set_clk_pin(clk))
    data = await cg.gpio_pin_expression(config[CONF_DATA_PIN])
    cg.add(var.set_data_pin(data))
