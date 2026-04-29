import urllib.parse as urlparse
from esphome.cpp_generator import RawExpression
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_NUM_ATTEMPTS,
    CONF_PASSWORD,
    CONF_REBOOT_TIMEOUT,
    CONF_SAFE_MODE,
    CONF_TRIGGER_ID,
    CONF_TIMEOUT,
    CONF_METHOD,
    CONF_URL,
    CONF_ESP8266_DISABLE_SSL_SUPPORT
)
from esphome.core import CORE, coroutine_with_priority, Lambda

CODEOWNERS = ["@grid"]
DEPENDENCIES = ["network"]
AUTO_LOAD = ["socket", "md5", "json"]

CONF_ON_STATE_CHANGE = "on_state_change"
CONF_ON_BEGIN = "on_begin"
CONF_ON_PROGRESS = "on_progress"
CONF_ON_END = "on_end"
CONF_ON_ERROR = "on_error"

CONF_HEADERS = "headers"
CONF_USERAGENT = "useragent"
CONF_VERIFY_SSL = "verify_ssl"
CONF_ON_RESPONSE = "on_response"
CONF_FOLLOW_REDIRECTS = "follow_redirects"
CONF_REDIRECT_LIMIT = "redirect_limit"

CONF_FWVER = "fwver"

http_update_ns = cg.esphome_ns.namespace("http_update")
HttpUpdateState = http_update_ns.enum("HttpUpdateState")
HttpUpdateComponent = http_update_ns.class_("HttpUpdateComponent", cg.Component)
HttpUpdateStateChangeTrigger = http_update_ns.class_(
    "HttpUpdateStateChangeTrigger", automation.Trigger.template()
)
HttpUpdateStartTrigger = http_update_ns.class_("HttpUpdateStartTrigger", automation.Trigger.template())
HttpUpdateProgressTrigger = http_update_ns.class_("HttpUpdateProgressTrigger", automation.Trigger.template())
HttpUpdateEndTrigger = http_update_ns.class_("HttpUpdateEndTrigger", automation.Trigger.template())
HttpUpdateErrorTrigger = http_update_ns.class_("HttpUpdateErrorTrigger", automation.Trigger.template())

HttpUpdateFlashAction = http_update_ns.class_(
    "HttpUpdateFlashAction", automation.Action
)
HttpUpdateResponseTrigger = http_update_ns.class_(
    "HttpUpdateResponseTrigger", automation.Trigger
)


def validate_url(value):
    value = cv.string(value)
    try:
        parsed = list(urlparse.urlparse(value))
    except Exception as err:
        raise cv.Invalid("Invalid URL") from err

    if not parsed[0] or not parsed[1]:
        raise cv.Invalid("URL must have a URL scheme and host")

    if parsed[0] not in ["http", "https"]:
        raise cv.Invalid("Scheme must be http or https")

    if not parsed[2]:
        parsed[2] = "/"

    return urlparse.urlunparse(parsed)


def validate_secure_url(config):
    url_ = config[CONF_URL]
    if (
        config.get(CONF_VERIFY_SSL)
        and not isinstance(url_, Lambda)
        and url_.lower().startswith("https:")
    ):
        raise cv.Invalid(
            "Currently ESPHome doesn't support SSL verification. "
            "Set 'verify_ssl: false' to make insecure HTTPS requests."
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HttpUpdateComponent),
        cv.Optional(CONF_SAFE_MODE, default=True): cv.boolean,
        cv.Optional(CONF_PASSWORD): cv.string,
        cv.Optional(CONF_FWVER): cv.string,
        cv.Optional(
            CONF_REBOOT_TIMEOUT, default="5min"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_NUM_ATTEMPTS, default="10"): cv.positive_not_null_int,
        cv.Optional(CONF_ON_STATE_CHANGE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(HttpUpdateStateChangeTrigger),
            }
        ),
        cv.Optional(CONF_ON_BEGIN): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(HttpUpdateStartTrigger),
            }
        ),
        cv.Optional(CONF_ON_ERROR): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(HttpUpdateErrorTrigger),
            }
        ),
        cv.Optional(CONF_ON_PROGRESS): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(HttpUpdateProgressTrigger),
            }
        ),
        cv.Optional(CONF_ON_END): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(HttpUpdateEndTrigger),
            }
        ),

        cv.Optional(CONF_USERAGENT, "ESPHome"): cv.string,
        cv.Optional(CONF_FOLLOW_REDIRECTS, True): cv.boolean,
        cv.Optional(CONF_REDIRECT_LIMIT, 3): cv.int_,
        cv.Optional(
            CONF_TIMEOUT, default="5s"
        ): cv.positive_time_period_milliseconds,
        cv.SplitDefault(CONF_ESP8266_DISABLE_SSL_SUPPORT, esp8266=False): cv.All(
            cv.only_on_esp8266, cv.boolean
        ),
    }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.require_framework_version(
        esp8266_arduino=cv.Version(2, 5, 1),
        esp32_arduino=cv.Version(0, 0, 0),
    ),
)


@coroutine_with_priority(50.0)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    if CONF_PASSWORD in config:
        cg.add(var.set_auth_password(config[CONF_PASSWORD]))
        cg.add_define("USE_HTTPUPDATE_PASSWORD")

    if CONF_FWVER in config:
        cg.add(var.set_fwver(config[CONF_FWVER]))

    await cg.register_component(var, config)

    if config[CONF_SAFE_MODE]:
        condition = var.should_enter_safe_mode(
            config[CONF_NUM_ATTEMPTS], config[CONF_REBOOT_TIMEOUT]
        )
        cg.add(RawExpression(f"if ({condition}) return"))

    if CORE.is_esp32 and CORE.using_arduino:
        cg.add_library("Update", None)

    use_state_callback = False
    for conf in config.get(CONF_ON_STATE_CHANGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(HttpUpdateState, "state")], conf)
        use_state_callback = True
    for conf in config.get(CONF_ON_BEGIN, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
        use_state_callback = True
    for conf in config.get(CONF_ON_PROGRESS, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(float, "x")], conf)
        use_state_callback = True
    for conf in config.get(CONF_ON_END, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
        use_state_callback = True
    for conf in config.get(CONF_ON_ERROR, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(int, "x")], conf)
        use_state_callback = True
    if use_state_callback:
        cg.add_define("USE_HTTPUPDATE_STATE_CALLBACK")

    cg.add(var.set_timeout_req(config[CONF_TIMEOUT]))
    cg.add(var.set_useragent(config[CONF_USERAGENT]))
    cg.add(var.set_follow_redirects(config[CONF_FOLLOW_REDIRECTS]))
    cg.add(var.set_redirect_limit(config[CONF_REDIRECT_LIMIT]))

    if CORE.is_esp8266 and not config[CONF_ESP8266_DISABLE_SSL_SUPPORT]:
        cg.add_define("USE_HTTP_UPDATE_ESP8266_HTTPS")

    if CORE.is_esp32:
        cg.add_library("Networking", None)
        cg.add_library("WiFi", None)
        cg.add_library("NetworkClientSecure", None)
        cg.add_library("HTTPClient", None)
    if CORE.is_esp8266:
        cg.add_library("ESP8266HTTPClient", None)


HTTP_UPDATE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(HttpUpdateComponent),
        cv.Required(CONF_URL): cv.templatable(validate_url),
        cv.Optional(CONF_HEADERS): cv.All(
            cv.Schema({cv.string: cv.templatable(cv.string)})
        ),
        cv.Optional(CONF_VERIFY_SSL, default=True): cv.boolean,
        cv.Optional(CONF_ON_RESPONSE): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(HttpUpdateResponseTrigger)}
        ),
    }
).add_extra(validate_secure_url)
HTTP_UPDATE_GET_ACTION_SCHEMA = automation.maybe_conf(
    CONF_URL,
    HTTP_UPDATE_ACTION_SCHEMA.extend(
        {
            cv.Optional(CONF_METHOD, default="GET"): cv.one_of("GET", upper=True),
        }
    ),
)


@automation.register_action(
    "http_update.flash", HttpUpdateFlashAction, HTTP_UPDATE_GET_ACTION_SCHEMA, synchronous=True
)

async def http_update_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config[CONF_URL], args, cg.std_string)
    cg.add(var.set_url(template_))
    template_ = await cg.templatable(config[CONF_METHOD], args, cg.const_char_ptr)
    cg.add(var.set_method(template_))
    for key in config.get(CONF_HEADERS, []):
        template_ = await cg.templatable(
            config[CONF_HEADERS][key], args, cg.const_char_ptr
        )
        cg.add(var.add_header(key, template_))

    for conf in config.get(CONF_ON_RESPONSE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_response_trigger(trigger))
        await automation.build_automation(trigger, [(int, "status_code")], conf)

    return var
