import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_SENSOR_DATAPOINT
from .. import sbus_ns, CONF_SBUS_ID, Sbus

DEPENDENCIES = ["sbus"]
CODEOWNERS = ["@grid"]

SbusTextSensor = sbus_ns.class_("SbusTextSensor", text_sensor.TextSensor, cg.PollingComponent)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(SbusTextSensor)
    .extend(
        {
            cv.GenerateID(CONF_SBUS_ID): cv.use_id(Sbus),
            cv.Required(CONF_SENSOR_DATAPOINT): cv.uint8_t,
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)

    paren = await cg.get_variable(config[CONF_SBUS_ID])
    cg.add(var.set_sbus_parent(paren))

    cg.add(var.set_sensor_id(config[CONF_SENSOR_DATAPOINT]))
