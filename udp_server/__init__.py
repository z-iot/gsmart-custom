import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (CONF_ID, CONF_TRIGGER_ID, CONF_PORT, CONF_CHANNEL, CONF_MAC_ADDRESS)

DEPENDENCIES = ["network"]

udp_server_ns = cg.esphome_ns.namespace("udp_server")
UdpServer = udp_server_ns.class_("UdpServer", cg.Component)

CONF_ON_CONTROL = "on_control"
CONF_ON_STATUS = "on_status"
CONF_ON_IDENTITY = "on_identity"
CONF_ON_RECONFIG = "on_reconfig"

CONF_ON_STATUS_FILL = "on_status_fill"
CONF_ON_IDENTITY_FILL = "on_identity_fill"

PacketControl = udp_server_ns.struct("PacketControl")
PacketStatus = udp_server_ns.struct("PacketStatus")
PacketIdentity = udp_server_ns.struct("PacketIdentity")
PacketReconfig = udp_server_ns.struct("PacketReconfig")

UdpControlNotifyTrigger = udp_server_ns.class_(
    "UdpControlNotifyTrigger", automation.Trigger.template(PacketControl)
)
UdpStatusNotifyTrigger = udp_server_ns.class_(
    "UdpStatusNotifyTrigger", automation.Trigger.template(PacketStatus)
)
UdpIdentityNotifyTrigger = udp_server_ns.class_(
    "UdpIdentityNotifyTrigger", automation.Trigger.template(PacketIdentity)
)
UdpReconfigNotifyTrigger = udp_server_ns.class_(
    "UdpReconfigNotifyTrigger", automation.Trigger.template(PacketReconfig)
)

UdpStatusFillTrigger = udp_server_ns.class_(
    "UdpStatusFillTrigger", automation.Trigger.template(PacketStatus)
)
UdpIdentityFillTrigger = udp_server_ns.class_(
    "UdpIdentityFillTrigger", automation.Trigger.template(PacketIdentity)
)
# RadiationAction = udp_server_ns.class_("RadiationAction", automation.Action)
# MotionAction = udp_server_ns.class_("MotionAction", automation.Action)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UdpServer),
            cv.Optional(CONF_PORT, default=30100): cv.port,
            cv.Optional(CONF_CHANNEL, default=0): cv.int_range(min=0, max=999),
            cv.Optional(CONF_ON_CONTROL): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        UdpControlNotifyTrigger
                    ),
                } 
            ),
            cv.Optional(CONF_ON_STATUS): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        UdpStatusNotifyTrigger
                    ),
                } 
            ),
            cv.Optional(CONF_ON_IDENTITY): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        UdpIdentityNotifyTrigger
                    ),
                } 
            ),
            cv.Optional(CONF_ON_RECONFIG): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        UdpReconfigNotifyTrigger
                    ),
                } 
            ),

            cv.Optional(CONF_ON_STATUS_FILL): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        UdpStatusFillTrigger
                    ),
                } 
            ),
            cv.Optional(CONF_ON_IDENTITY_FILL): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        UdpIdentityFillTrigger
                    ),
                } 
            ),            
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_arduino,
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add_define("USE_UDPSERVER")
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_channel(config[CONF_CHANNEL]))

    for conf in config.get(CONF_ON_CONTROL, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(PacketControl, "packet")], conf)
    for conf in config.get(CONF_ON_STATUS, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(PacketStatus, "packet")], conf)
    for conf in config.get(CONF_ON_IDENTITY, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(PacketIdentity, "packet")], conf)
    for conf in config.get(CONF_ON_RECONFIG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(PacketReconfig, "packet")], conf)

    for conf in config.get(CONF_ON_STATUS_FILL, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(PacketStatus, "packet")], conf)
    for conf in config.get(CONF_ON_IDENTITY_FILL, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(PacketIdentity, "packet")], conf)

# @automation.register_action(
#     "udp_server.radiation",
#     RadiationAction,
#     cv.maybe_simple_value(
#         {
#             cv.GenerateID(): cv.use_id(UdpServer),
#             cv.Required(CONF_MODE): cv.templatable(cv.enum(RADIATION_MODE, upper=True)),
#         },
#         key=CONF_MODE,
#     ),
# )
# async def udp_server_radiation_to_code(config, action_id, template_arg, args):
#     var = cg.new_Pvariable(action_id, template_arg)
#     await cg.register_parented(var, config[CONF_ID])
#     template_ = await cg.templatable(config[CONF_MODE], args, RadiationMode)
#     cg.add(var.set_mode(template_))
#     return var

# @automation.register_action(
#     "udp_server.motion",
#     MotionAction,
#     cv.maybe_simple_value(
#         {
#             cv.GenerateID(): cv.use_id(UdpServer),
#             cv.Required(CONF_STATE): cv.templatable(cv.boolean),
#         },
#         key=CONF_STATE,
#     ),
# )
# async def udp_server_motion_to_code(config, action_id, template_arg, args):
#     var = cg.new_Pvariable(action_id, template_arg)
#     await cg.register_parented(var, config[CONF_ID])
#     template_ = await cg.templatable(config[CONF_STATE], args, bool)
#     cg.add(var.set_state(template_))
#     return var
