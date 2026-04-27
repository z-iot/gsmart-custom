import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_NUMBER_DATAPOINT,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_STEP,
)
from .. import sbus_ns, CONF_SBUS_ID, Sbus

DEPENDENCIES = ["sbus"]
CODEOWNERS = ["@frankiboy1"]

SbusNumber = sbus_ns.class_("SbusNumber", number.Number, cg.Component)


def validate_min_max(config):
    if config[CONF_MAX_VALUE] <= config[CONF_MIN_VALUE]:
        raise cv.Invalid("max_value must be greater than min_value")
    return config


CONFIG_SCHEMA = cv.All(
    number.number_schema(SbusNumber)
    .extend(
        {
            cv.GenerateID(CONF_SBUS_ID): cv.use_id(Sbus),
            cv.Required(CONF_NUMBER_DATAPOINT): cv.uint8_t,
            cv.Required(CONF_MAX_VALUE): cv.float_,
            cv.Required(CONF_MIN_VALUE): cv.float_,
            cv.Required(CONF_STEP): cv.positive_float,
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    validate_min_max,
)


async def to_code(config):
    var = await number.new_number(
        config,
        min_value=config[CONF_MIN_VALUE],
        max_value=config[CONF_MAX_VALUE],
        step=config[CONF_STEP],
    )
    await cg.register_component(var, config)

    paren = await cg.get_variable(config[CONF_SBUS_ID])
    cg.add(var.set_sbus_parent(paren))

    cg.add(var.set_number_id(config[CONF_NUMBER_DATAPOINT]))
