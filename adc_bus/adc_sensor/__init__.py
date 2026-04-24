from esphome.components import sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_SENSOR_DATAPOINT
from .. import sbus_ns, CONF_SBUS_ID, Sbus

DEPENDENCIES = ["sbus"]
CODEOWNERS = ["@grid"]

SbusSensor = sbus_ns.class_("SbusSensor", sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = sensor.SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SbusSensor),
        cv.GenerateID(CONF_SBUS_ID): cv.use_id(Sbus),
        cv.Required(CONF_SENSOR_DATAPOINT): cv.uint8_t,
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    paren = await cg.get_variable(config[CONF_SBUS_ID])
    cg.add(var.set_sbus_parent(paren))

    cg.add(var.set_sensor_id(config[CONF_SENSOR_DATAPOINT]))
