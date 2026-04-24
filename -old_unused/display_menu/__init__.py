import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display
from esphome.components.font import Font
from esphome.const import (
    CONF_ID,
    CONF_DIMENSIONS,
)
from esphome.core.entity_helpers import inherit_property_from
from esphome.components import lcd_base
from esphome.components.display_menu_base import (
    DISPLAY_MENU_BASE_SCHEMA,
    DisplayMenuComponent,
    display_menu_to_code,
)

CODEOWNERS = ["@grid"]

AUTO_LOAD = ["display_menu_base"]

display_menu_ns = cg.esphome_ns.namespace("display_menu")

CONF_DISPLAY = "display"
CONF_FONT = "font"
CONF_ROW_HEIGHT = "row_height"

DispBufMenuComponent = display_menu_ns.class_(
    "DispBufMenuComponent", DisplayMenuComponent
)

CONFIG_SCHEMA = DISPLAY_MENU_BASE_SCHEMA.extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DispBufMenuComponent),
            cv.GenerateID(CONF_DISPLAY): cv.use_id(display.DisplayBuffer),
            cv.Required(CONF_FONT): cv.use_id(Font),
            cv.Required(CONF_ROW_HEIGHT): cv.uint8_t,
        }
    )
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    disp = await cg.get_variable(config[CONF_DISPLAY])
    cg.add(var.set_display(disp))
    font = await cg.get_variable(config[CONF_FONT])
    cg.add(var.set_font(font))
    cg.add(var.set_row_height(config[CONF_ROW_HEIGHT]))
    await display_menu_to_code(var, config)

