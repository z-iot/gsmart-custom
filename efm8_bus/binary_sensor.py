import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from . import efm8_bus_ns, Efm8BusComponent, CONF_EFM8_BUS_ID

DEPENDENCIES = ["efm8_bus"]

CONF_CHANNEL = "channel"

Efm8Input = efm8_bus_ns.class_("Efm8Input", binary_sensor.BinarySensor)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(Efm8Input).extend(
    {
        cv.GenerateID(CONF_EFM8_BUS_ID): cv.use_id(Efm8BusComponent),
        cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=2),
    }
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)

    cg.add(var.set_channel(config[CONF_CHANNEL]))
    bus = await cg.get_variable(config[CONF_EFM8_BUS_ID])
    cg.add(bus.register_input(var))
