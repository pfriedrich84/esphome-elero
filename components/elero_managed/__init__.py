import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID, CONF_NAME, ENTITY_CATEGORY_DIAGNOSTIC

from ..elero import CONF_ELERO_ID, elero

DEPENDENCIES = ["elero"]
AUTO_LOAD = ["text_sensor"]

elero_managed_ns = cg.esphome_ns.namespace("elero_managed")
EleroManaged = elero_managed_ns.class_("EleroManaged", cg.Component)

CONF_ENABLED = "enabled"
CONF_MAX_DEVICES = "max_devices"
CONF_RESPONSE_TEXT_SENSOR = "response_text_sensor"

_RESPONSE_TEXT_SENSOR_SCHEMA = text_sensor.text_sensor_schema(
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    icon="mdi:api",
)


def _auto_response_text_sensor(config):
    if CONF_RESPONSE_TEXT_SENSOR not in config:
        result = dict(config)
        result[CONF_RESPONSE_TEXT_SENSOR] = _RESPONSE_TEXT_SENSOR_SCHEMA({CONF_NAME: "Elero Managed API Result"})
        return result
    return config

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EleroManaged),
            cv.GenerateID(CONF_ELERO_ID): cv.use_id(elero),
            cv.Optional(CONF_ENABLED, default=False): cv.boolean,
            cv.Optional(CONF_MAX_DEVICES, default=32): cv.int_range(min=0, max=32),
            cv.Optional(CONF_RESPONSE_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _auto_response_text_sensor,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    hub = await cg.get_variable(config[CONF_ELERO_ID])
    cg.add(var.set_parent(hub))
    cg.add(var.set_enabled(config[CONF_ENABLED]))
    cg.add(var.set_max_devices(config[CONF_MAX_DEVICES]))

    response_sensor = await text_sensor.new_text_sensor(config[CONF_RESPONSE_TEXT_SENSOR])
    cg.add(var.set_response_text_sensor(response_sensor))
