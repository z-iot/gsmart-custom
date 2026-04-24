from esphome.components import binary_sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_SENSOR_DATAPOINT
from .. import sbus_ns, CONF_SBUS_ID, Sbus

DEPENDENCIES = ["sbus"]
CODEOWNERS = ["@jesserockz"]

SbusBinarySensor = sbus_ns.class_(
    "SbusBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SbusBinarySensor),
        cv.GenerateID(CONF_SBUS_ID): cv.use_id(Sbus),
        cv.Required(CONF_SENSOR_DATAPOINT): cv.uint8_t,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await binary_sensor.register_binary_sensor(var, config)

    paren = await cg.get_variable(config[CONF_SBUS_ID])
    cg.add(var.set_sbus_parent(paren))

    cg.add(var.set_sensor_id(config[CONF_SENSOR_DATAPOINT]))
