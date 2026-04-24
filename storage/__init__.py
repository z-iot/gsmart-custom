import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE, coroutine_with_priority
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_TRIGGER_ID,
)

AUTO_LOAD = ["json"]
# DEPENDENCIES = ["wifi"]
CODEOWNERS = ["Grid"]
CONF_MODEL = "model"
CONF_ON_SITUATION_CHANGE = "on_situation_change"
CONF_ON_SITUATION_DURATION_CHANGE = "on_situation_duration_change"
CONF_ON_CHANGE_RADIATION_MODE = "on_change_radiation_mode"

storage_ns = cg.esphome_ns.namespace("storage")
RadiationMode = storage_ns.enum("RadiationMode")
Store = storage_ns.class_("Store", cg.Component, cg.Controller)

SituationChangeTrigger = storage_ns.class_("SituationChangeTrigger", automation.Trigger.template())
SituationDurationChangeTrigger = storage_ns.class_("SituationDurationChangeTrigger", automation.Trigger.template())
ChangeRadiationModeTrigger = storage_ns.class_("ChangeRadiationModeTrigger", automation.Trigger.template())

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Store),
            cv.Required(CONF_MODEL): cv.string,
            cv.Optional(CONF_ON_SITUATION_CHANGE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SituationChangeTrigger),
                }
            ),
            cv.Optional(CONF_ON_SITUATION_DURATION_CHANGE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SituationDurationChangeTrigger),
                }
            ),
            cv.Optional(CONF_ON_CHANGE_RADIATION_MODE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ChangeRadiationModeTrigger),
                }
            ),
        },
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_arduino,
)


@coroutine_with_priority(64.0)
async def to_code(config):
    if CORE.using_arduino:
        if CORE.is_esp32:
            cg.add_library("SPIFFS", None)
        elif CORE.is_esp8266:
            cg.add_library("LittleFS", None)

    if config[CONF_MODEL] == "sibra":
        cg.add_define("GSMART_MODEL_SIBRA")
        cg.add_define("GSMART_EMITTER")
    elif config[CONF_MODEL] == "opera":
        cg.add_define("GSMART_MODEL_OPERA")
        cg.add_define("GSMART_EMITTER")
    elif config[CONF_MODEL] == "aqua":
        cg.add_define("GSMART_MODEL_AQUA")
        cg.add_define("GSMART_EMITTER")
    elif config[CONF_MODEL] == "mobi":
        cg.add_define("GSMART_MODEL_MOBI")
        cg.add_define("GSMART_EMITTER")
    elif config[CONF_MODEL] == "panel":
        cg.add_define("GSMART_MODEL_PANEL")
        cg.add_define("GSMART_ACTUATOR")
    elif config[CONF_MODEL] == "rex":
        cg.add_define("GSMART_MODEL_REX")
        cg.add_define("GSMART_ACTUATOR")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add_define("USE_STORAGE")

    cg.add(var.set_model(config[CONF_MODEL]))
    
    for conf in config.get(CONF_ON_SITUATION_CHANGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_SITUATION_DURATION_CHANGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_CHANGE_RADIATION_MODE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(RadiationMode, "x")], conf)

    # if CORE.is_esp32:
    #     cg.add_library("DNSServer", None)
    #     cg.add_library("WiFi", None)
    # if CORE.is_esp8266:
    #     cg.add_library("DNSServer", None)
    # cg.add_define("USE_WEBSERVER_PORT", config[CONF_PORT])
    # cg.add(var.set_include_internal(config[CONF_INCLUDE_INTERNAL]))
