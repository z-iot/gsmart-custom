from __future__ import annotations

import gzip

import esphome.codegen as cg
from esphome.components import web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
import esphome.config_validation as cv
from esphome.const import (
    CONF_CSS_INCLUDE,
    CONF_CSS_URL,
    CONF_ID,
    CONF_INCLUDE_INTERNAL,
    CONF_JS_INCLUDE,
    CONF_JS_URL,
    CONF_LOG,
    CONF_VERSION,
    CONF_WEB_SERVER_ID,
    CONF_PORT,
    CONF_OTA,
    CONF_LOCAL,
)
from esphome.core import CORE, coroutine_with_priority

AUTO_LOAD = ["json", "web_server_base"]
DEPENDENCIES = ["web_server"]

web_server_ns = cg.esphome_ns.namespace("web_server")
EspServer = web_server_ns.class_("EspServer", cg.Component)

def default_url(config):
    config = config.copy()
    if config[CONF_VERSION] == 1:
        if CONF_CSS_URL not in config:
            config[CONF_CSS_URL] = "https://esphome.io/_static/webserver-v1.min.css"
        if CONF_JS_URL not in config:
            config[CONF_JS_URL] = "https://esphome.io/_static/webserver-v1.min.js"
    if config[CONF_VERSION] == 2:
        if CONF_CSS_URL not in config:
            config[CONF_CSS_URL] = ""
        if CONF_JS_URL not in config:
            config[CONF_JS_URL] = "https://oi.esphome.io/v2/www.js"
    if config[CONF_VERSION] == 3:
        if CONF_CSS_URL not in config:
            config[CONF_CSS_URL] = ""
        if CONF_JS_URL not in config:
            config[CONF_JS_URL] = "https://oi.esphome.io/v3/www.js"
    return config

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EspServer),
            cv.Optional(CONF_VERSION, default=2): cv.one_of(1, 2, 3, int=True),
            cv.Optional(CONF_CSS_URL): cv.string,
            cv.Optional(CONF_CSS_INCLUDE): cv.file_,
            cv.Optional(CONF_JS_URL): cv.string,
            cv.Optional(CONF_JS_INCLUDE): cv.file_,
            cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
                web_server_base.WebServerBase
            ),
            cv.GenerateID(CONF_WEB_SERVER_ID): cv.use_id(web_server_ns.class_("WebServer", cg.Component)),
            cv.Optional(CONF_INCLUDE_INTERNAL, default=False): cv.boolean,
            cv.Optional(CONF_LOG, default=True): cv.boolean,
            # Legacy/ignored fields to keep YAML compatibility
            cv.Optional(CONF_PORT): cv.port,
            cv.Optional(CONF_OTA): cv.boolean,
            cv.Optional(CONF_LOCAL): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    default_url,
)

def build_index_html(config) -> str:
    html = "<!DOCTYPE html><html><head><meta charset=UTF-8><link rel=icon href=data:>"
    css_include = config.get(CONF_CSS_INCLUDE)
    js_include = config.get(CONF_JS_INCLUDE)
    if css_include:
        html += "<link rel=stylesheet href=/0.css>"
    if config[CONF_CSS_URL]:
        html += f'<link rel=stylesheet href="{config[CONF_CSS_URL]}">'
    html += "</head><body>"
    if js_include:
        html += "<script type=module src=/0.js></script>"
    html += "<esp-app></esp-app>"
    if config[CONF_JS_URL]:
        html += f'<script src="{config[CONF_JS_URL]}"></script>'
    html += "</body></html>"
    return html

def add_resource_as_progmem(
    resource_name: str, content: str, compress: bool = True
) -> None:
    content_encoded = content.encode("utf-8")
    if compress:
        content_encoded = gzip.compress(content_encoded)
    content_encoded_size = len(content_encoded)
    bytes_as_int = ", ".join(str(x) for x in content_encoded)
    uint8_t = f"extern const uint8_t ESPHOME_ESP_SERVER_{resource_name}[] PROGMEM;"
    uint8_t_def = f"const uint8_t ESPHOME_ESP_SERVER_{resource_name}[{content_encoded_size}] PROGMEM = {{{bytes_as_int}}}"
    size_t = f"extern const size_t ESPHOME_ESP_SERVER_{resource_name}_SIZE;"
    size_t_def = f"const size_t ESPHOME_ESP_SERVER_{resource_name}_SIZE = {content_encoded_size}"
    cg.add_global(cg.RawExpression(uint8_t))
    cg.add_global(cg.RawExpression(uint8_t_def))
    cg.add_global(cg.RawExpression(size_t))
    cg.add_global(cg.RawExpression(size_t_def))

@coroutine_with_priority(40.0)
async def to_code(config):
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
    ws = await cg.get_variable(config[CONF_WEB_SERVER_ID])
    var = cg.new_Pvariable(config[CONF_ID], paren, ws)
    await cg.register_component(var, config)

    version = config[CONF_VERSION]
    cg.add_define("USE_ESP_SERVER")
    cg.add_define("USE_ESP_SERVER_VERSION", version)
    
    if version >= 2:
        add_resource_as_progmem("INDEX_HTML", build_index_html(config), compress=False)
    
    cg.add(var.set_expose_log(config[CONF_LOG]))
    
    if CONF_CSS_INCLUDE in config:
        cg.add_define("USE_ESP_SERVER_CSS_INCLUDE")
        path = CORE.relative_config_path(config[CONF_CSS_INCLUDE])
        with open(file=path, encoding="utf-8") as css_file:
            add_resource_as_progmem("CSS_INCLUDE", css_file.read())
            
    if CONF_JS_INCLUDE in config:
        cg.add_define("USE_ESP_SERVER_JS_INCLUDE")
        path = CORE.relative_config_path(config[CONF_JS_INCLUDE])
        with open(file=path, encoding="utf-8") as js_file:
            add_resource_as_progmem("JS_INCLUDE", js_file.read())
            
    cg.add(var.set_include_internal(config[CONF_INCLUDE_INTERNAL]))
