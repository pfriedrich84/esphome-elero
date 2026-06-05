import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_DISABLED_BY_DEFAULT, CONF_ID, CONF_NAME, ENTITY_CATEGORY_DIAGNOSTIC

from ..elero import CONF_ELERO_ID, elero

DEPENDENCIES = ["elero"]
AUTO_LOAD = ["text_sensor"]

elero_managed_ns = cg.esphome_ns.namespace("elero_managed")
EleroManaged = elero_managed_ns.class_("EleroManaged", cg.Component)

CONF_ENABLED = "enabled"
CONF_MAX_DEVICES = "max_devices"
CONF_RESPONSE_TEXT_SENSOR = "response_text_sensor"
CONF_LAST_CALL_TEXT_SENSOR = "last_call_text_sensor"
CONF_LAST_OK_TEXT_SENSOR = "last_ok_text_sensor"
CONF_LAST_ERROR_TEXT_SENSOR = "last_error_text_sensor"
CONF_HUB_ID_TEXT_SENSOR = "hub_id_text_sensor"
CONF_REGISTRY_REVISION_TEXT_SENSOR = "registry_revision_text_sensor"
CONF_DEVICE_COUNT_TEXT_SENSOR = "device_count_text_sensor"

_RESPONSE_TEXT_SENSOR_SCHEMA = text_sensor.text_sensor_schema(
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    icon="mdi:api",
)


_DIAGNOSTIC_TEXT_SENSORS = {
    CONF_RESPONSE_TEXT_SENSOR: ("Elero Managed API Result", _RESPONSE_TEXT_SENSOR_SCHEMA),
    CONF_LAST_CALL_TEXT_SENSOR: ("Elero Managed Last Call", _RESPONSE_TEXT_SENSOR_SCHEMA),
    CONF_LAST_OK_TEXT_SENSOR: ("Elero Managed Last OK", _RESPONSE_TEXT_SENSOR_SCHEMA),
    CONF_LAST_ERROR_TEXT_SENSOR: ("Elero Managed Last Error", _RESPONSE_TEXT_SENSOR_SCHEMA),
    CONF_HUB_ID_TEXT_SENSOR: ("Elero Managed Hub ID", _RESPONSE_TEXT_SENSOR_SCHEMA),
    CONF_REGISTRY_REVISION_TEXT_SENSOR: ("Elero Managed Registry Revision", _RESPONSE_TEXT_SENSOR_SCHEMA),
    CONF_DEVICE_COUNT_TEXT_SENSOR: ("Elero Managed Device Count", _RESPONSE_TEXT_SENSOR_SCHEMA),
}


def _hidden_text_sensor_config(name):
    return {CONF_NAME: name, CONF_DISABLED_BY_DEFAULT: True}


def _auto_diagnostic_text_sensors(config):
    result = dict(config)
    for key, (name, schema) in _DIAGNOSTIC_TEXT_SENSORS.items():
        if key not in result:
            result[key] = schema(_hidden_text_sensor_config(name))
    return result

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EleroManaged),
            cv.GenerateID(CONF_ELERO_ID): cv.use_id(elero),
            cv.Optional(CONF_ENABLED, default=False): cv.boolean,
            cv.Optional(CONF_MAX_DEVICES, default=32): cv.int_range(min=0, max=32),
            cv.Optional(CONF_RESPONSE_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
            cv.Optional(CONF_LAST_CALL_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
            cv.Optional(CONF_LAST_OK_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
            cv.Optional(CONF_LAST_ERROR_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
            cv.Optional(CONF_HUB_ID_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
            cv.Optional(CONF_REGISTRY_REVISION_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
            cv.Optional(CONF_DEVICE_COUNT_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _auto_diagnostic_text_sensors,
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
    last_call_sensor = await text_sensor.new_text_sensor(config[CONF_LAST_CALL_TEXT_SENSOR])
    cg.add(var.set_last_call_text_sensor(last_call_sensor))
    last_ok_sensor = await text_sensor.new_text_sensor(config[CONF_LAST_OK_TEXT_SENSOR])
    cg.add(var.set_last_ok_text_sensor(last_ok_sensor))
    last_error_sensor = await text_sensor.new_text_sensor(config[CONF_LAST_ERROR_TEXT_SENSOR])
    cg.add(var.set_last_error_text_sensor(last_error_sensor))
    hub_id_sensor = await text_sensor.new_text_sensor(config[CONF_HUB_ID_TEXT_SENSOR])
    cg.add(var.set_hub_id_text_sensor(hub_id_sensor))
    revision_sensor = await text_sensor.new_text_sensor(config[CONF_REGISTRY_REVISION_TEXT_SENSOR])
    cg.add(var.set_registry_revision_text_sensor(revision_sensor))
    device_count_sensor = await text_sensor.new_text_sensor(config[CONF_DEVICE_COUNT_TEXT_SENSOR])
    cg.add(var.set_device_count_text_sensor(device_count_sensor))
