import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

from .. import CONF_ELERO_ID, elero, elero_ns

DEPENDENCIES = ["elero"]

EleroScanButton = elero_ns.class_("EleroScanButton", button.Button, cg.Component)
# References resolved at code-gen time; avoids circular import
EleroLight = elero_ns.class_("EleroLight")
EleroCover = elero_ns.class_("EleroCover")

CONF_SCAN_START = "scan_start"
CONF_LIGHT_ID = "light_id"
CONF_COVER_ID = "cover_id"
CONF_COMMAND_BYTE = "command_byte"


def _validate_exclusive_target(config):
    """cover_id and light_id are mutually exclusive."""
    if CONF_COVER_ID in config and CONF_LIGHT_ID in config:
        raise cv.Invalid("'cover_id' and 'light_id' cannot both be set on the same button")
    return config


CONFIG_SCHEMA = cv.All(
    button.button_schema(EleroScanButton)
    .extend(
        {
            cv.GenerateID(CONF_ELERO_ID): cv.use_id(elero),
            cv.Optional(CONF_SCAN_START, default=True): cv.boolean,
            cv.Optional(CONF_LIGHT_ID): cv.use_id(EleroLight),
            cv.Optional(CONF_COVER_ID): cv.use_id(EleroCover),
            cv.Optional(CONF_COMMAND_BYTE, default=0x44): cv.hex_int_range(min=0x0, max=0xFF),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    _validate_exclusive_target,
)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_component(var, config)
    parent = await cg.get_variable(config[CONF_ELERO_ID])
    cg.add(var.set_elero_parent(parent))
    cg.add(var.set_scan_start(config[CONF_SCAN_START]))

    if CONF_COVER_ID in config:
        cover_var = await cg.get_variable(config[CONF_COVER_ID])
        cg.add(var.set_cover(cover_var))
        cg.add(var.set_command_byte(config[CONF_COMMAND_BYTE]))
    elif CONF_LIGHT_ID in config:
        light_var = await cg.get_variable(config[CONF_LIGHT_ID])
        cg.add(var.set_light(light_var))
        cg.add(var.set_command_byte(config[CONF_COMMAND_BYTE]))
