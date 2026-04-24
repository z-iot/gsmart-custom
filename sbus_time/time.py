import esphome.config_validation as cv
import esphome.codegen as cg
from esphome import automation
from esphome.components import time
from esphome.const import CONF_ID
from esphome.components.sbus import sbus_ns, CONF_SBUS_ID, Sbus

CODEOWNERS = ["@grid"]
DEPENDENCIES = ["sbus"]

CONF_TIME_DATAPOINT = "time_datapoint"

sbus_time_ns = cg.esphome_ns.namespace("sbus_time")
SbusTimeComponent = sbus_time_ns.class_("SbusTimeComponent", time.RealTimeClock)
WriteAction = sbus_time_ns.class_("WriteAction", automation.Action)
ReadAction = sbus_time_ns.class_("ReadAction", automation.Action)


CONFIG_SCHEMA = time.TIME_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SbusTimeComponent),
        cv.GenerateID(CONF_SBUS_ID): cv.use_id(Sbus),
        cv.Required(CONF_TIME_DATAPOINT): cv.uint8_t,
    }
).extend(cv.COMPONENT_SCHEMA)


@automation.register_action(
    "sbus_time.write_time",
    WriteAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(SbusTimeComponent),
        }
    ),
)
async def sbus_time_write_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "sbus_time.read_time",
    ReadAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(SbusTimeComponent),
        }
    ),
)
async def sbus_time_read_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await time.register_time(var, config)

    paren = await cg.get_variable(config[CONF_SBUS_ID])
    cg.add(var.set_sbus_parent(paren))

    cg.add(var.set_time_id(config[CONF_TIME_DATAPOINT]))
