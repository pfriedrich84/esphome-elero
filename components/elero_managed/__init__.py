import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover, light, text_sensor
from esphome.const import CONF_ID, CONF_NAME, CONF_OUTPUT_ID, ENTITY_CATEGORY_DIAGNOSTIC

from ..elero import CONF_ELERO_ID, elero

DEPENDENCIES = ["elero"]
AUTO_LOAD = ["cover", "light", "text_sensor"]

elero_managed_ns = cg.esphome_ns.namespace("elero_managed")
EleroManaged = elero_managed_ns.class_("EleroManaged", cg.Component)
EleroManagedCoverSlot = elero_managed_ns.class_("EleroManagedCoverSlot", cover.Cover, cg.Component)
EleroManagedLightSlot = elero_managed_ns.class_("EleroManagedLightSlot", light.LightOutput, cg.Component)

CONF_ENABLED = "enabled"
CONF_MAX_DEVICES = "max_devices"
CONF_PREALLOCATED_COVER_SLOTS = "preallocated_cover_slots"
CONF_PREALLOCATED_LIGHT_SLOTS = "preallocated_light_slots"
CONF_RESPONSE_TEXT_SENSOR = "response_text_sensor"
CONF_LAST_CALL_TEXT_SENSOR = "last_call_text_sensor"
CONF_LAST_OK_TEXT_SENSOR = "last_ok_text_sensor"
CONF_LAST_ERROR_TEXT_SENSOR = "last_error_text_sensor"
CONF_HUB_ID_TEXT_SENSOR = "hub_id_text_sensor"
CONF_REGISTRY_REVISION_TEXT_SENSOR = "registry_revision_text_sensor"
CONF_DEVICE_COUNT_TEXT_SENSOR = "device_count_text_sensor"
CONF_MANAGED_ENABLED_TEXT_SENSOR = "managed_enabled_text_sensor"
CONF_SCHEMA_VERSION_TEXT_SENSOR = "schema_version_text_sensor"
CONF_COMPONENT_VERSION_TEXT_SENSOR = "component_version_text_sensor"
CONF_HUB_MAC_TEXT_SENSOR = "hub_mac_text_sensor"
CONF_MAX_DEVICES_TEXT_SENSOR = "max_devices_text_sensor"
CONF_ENTITY_MATERIALIZATION_TEXT_SENSOR = "entity_materialization_text_sensor"
CONF_MATERIALIZED_COVER_COUNT_TEXT_SENSOR = "materialized_cover_count_text_sensor"
CONF_MATERIALIZED_LIGHT_COUNT_TEXT_SENSOR = "materialized_light_count_text_sensor"
CONF_MANAGED_COVER_SLOT_CONFIGS = "managed_cover_slot_configs"
CONF_MANAGED_LIGHT_SLOT_CONFIGS = "managed_light_slot_configs"

_RESPONSE_TEXT_SENSOR_SCHEMA = text_sensor.text_sensor_schema(
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    icon="mdi:api",
)
_MANAGED_COVER_SLOT_SCHEMA = cover.cover_schema(EleroManagedCoverSlot).extend(cv.COMPONENT_SCHEMA)
_MANAGED_LIGHT_SLOT_SCHEMA = light.light_schema(EleroManagedLightSlot, light.LightType.BRIGHTNESS_ONLY).extend(
    cv.COMPONENT_SCHEMA
)


_DIAGNOSTIC_TEXT_SENSORS = {
    CONF_LAST_CALL_TEXT_SENSOR: ("Elero Managed Last Call", _RESPONSE_TEXT_SENSOR_SCHEMA),
    CONF_LAST_OK_TEXT_SENSOR: ("Elero Managed Last OK", _RESPONSE_TEXT_SENSOR_SCHEMA),
    CONF_LAST_ERROR_TEXT_SENSOR: ("Elero Managed Last Error", _RESPONSE_TEXT_SENSOR_SCHEMA),
    CONF_MANAGED_ENABLED_TEXT_SENSOR: ("Elero Managed Enabled", _RESPONSE_TEXT_SENSOR_SCHEMA),
    CONF_SCHEMA_VERSION_TEXT_SENSOR: ("Elero Managed Schema Version", _RESPONSE_TEXT_SENSOR_SCHEMA),
    CONF_COMPONENT_VERSION_TEXT_SENSOR: ("Elero Managed Component Version", _RESPONSE_TEXT_SENSOR_SCHEMA),
    CONF_HUB_ID_TEXT_SENSOR: ("Elero Managed Hub ID", _RESPONSE_TEXT_SENSOR_SCHEMA),
    CONF_HUB_MAC_TEXT_SENSOR: ("Elero Managed Hub MAC", _RESPONSE_TEXT_SENSOR_SCHEMA),
    CONF_MAX_DEVICES_TEXT_SENSOR: ("Elero Managed Max Devices", _RESPONSE_TEXT_SENSOR_SCHEMA),
    CONF_REGISTRY_REVISION_TEXT_SENSOR: ("Elero Managed Registry Revision", _RESPONSE_TEXT_SENSOR_SCHEMA),
    CONF_DEVICE_COUNT_TEXT_SENSOR: ("Elero Managed Device Count", _RESPONSE_TEXT_SENSOR_SCHEMA),
    CONF_ENTITY_MATERIALIZATION_TEXT_SENSOR: (
        "Elero Managed Entity Materialization",
        _RESPONSE_TEXT_SENSOR_SCHEMA,
    ),
    CONF_MATERIALIZED_COVER_COUNT_TEXT_SENSOR: (
        "Elero Managed Materialized Cover Count",
        _RESPONSE_TEXT_SENSOR_SCHEMA,
    ),
    CONF_MATERIALIZED_LIGHT_COUNT_TEXT_SENSOR: (
        "Elero Managed Materialized Light Count",
        _RESPONSE_TEXT_SENSOR_SCHEMA,
    ),
}


def _diagnostic_text_sensor_config(name):
    return {CONF_NAME: name}


def _auto_diagnostic_text_sensors(config):
    result = dict(config)
    for key, (name, schema) in _DIAGNOSTIC_TEXT_SENSORS.items():
        if key not in result:
            result[key] = schema(_diagnostic_text_sensor_config(name))
    return result


def _validate_preallocated_slots(config):
    slot_count = config.get(CONF_PREALLOCATED_COVER_SLOTS, 0) + config.get(CONF_PREALLOCATED_LIGHT_SLOTS, 0)
    if slot_count > config.get(CONF_MAX_DEVICES, 32):
        raise cv.Invalid(
            "preallocated_cover_slots + preallocated_light_slots must be less than or equal to max_devices"
        )
    return config


def _auto_managed_slot_configs(config):
    result = dict(config)
    base_id = str(config[CONF_ID])
    cover_slots = []
    for i in range(config.get(CONF_PREALLOCATED_COVER_SLOTS, 0)):
        cover_slots.append(
            _MANAGED_COVER_SLOT_SCHEMA(
                {
                    CONF_ID: cv.declare_id(EleroManagedCoverSlot)(f"{base_id}_managed_cover_slot_{i}"),
                    CONF_NAME: f"Elero Managed Cover Slot {i + 1}",
                }
            )
        )
    light_slots = []
    for i in range(config.get(CONF_PREALLOCATED_LIGHT_SLOTS, 0)):
        light_slots.append(
            _MANAGED_LIGHT_SLOT_SCHEMA(
                {
                    CONF_ID: cv.declare_id(light.LightState)(f"{base_id}_managed_light_slot_{i}"),
                    CONF_OUTPUT_ID: cv.declare_id(EleroManagedLightSlot)(f"{base_id}_managed_light_output_slot_{i}"),
                    CONF_NAME: f"Elero Managed Light Slot {i + 1}",
                    "default_transition_length": "0s",
                }
            )
        )
    result[CONF_MANAGED_COVER_SLOT_CONFIGS] = cover_slots
    result[CONF_MANAGED_LIGHT_SLOT_CONFIGS] = light_slots
    return result


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EleroManaged),
            cv.GenerateID(CONF_ELERO_ID): cv.use_id(elero),
            cv.Optional(CONF_ENABLED, default=False): cv.boolean,
            cv.Optional(CONF_MAX_DEVICES, default=32): cv.int_range(min=0, max=32),
            cv.Optional(CONF_PREALLOCATED_COVER_SLOTS, default=0): cv.int_range(min=0, max=32),
            cv.Optional(CONF_PREALLOCATED_LIGHT_SLOTS, default=0): cv.int_range(min=0, max=32),
            cv.Optional(CONF_RESPONSE_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
            cv.Optional(CONF_LAST_CALL_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
            cv.Optional(CONF_LAST_OK_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
            cv.Optional(CONF_LAST_ERROR_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
            cv.Optional(CONF_MANAGED_ENABLED_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
            cv.Optional(CONF_SCHEMA_VERSION_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
            cv.Optional(CONF_COMPONENT_VERSION_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
            cv.Optional(CONF_HUB_ID_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
            cv.Optional(CONF_HUB_MAC_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
            cv.Optional(CONF_MAX_DEVICES_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
            cv.Optional(CONF_REGISTRY_REVISION_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
            cv.Optional(CONF_DEVICE_COUNT_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
            cv.Optional(CONF_ENTITY_MATERIALIZATION_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
            cv.Optional(CONF_MATERIALIZED_COVER_COUNT_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
            cv.Optional(CONF_MATERIALIZED_LIGHT_COUNT_TEXT_SENSOR): _RESPONSE_TEXT_SENSOR_SCHEMA,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_preallocated_slots,
    _auto_diagnostic_text_sensors,
    _auto_managed_slot_configs,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    hub = await cg.get_variable(config[CONF_ELERO_ID])
    cg.add(var.set_parent(hub))
    cg.add(var.set_enabled(config[CONF_ENABLED]))
    cg.add(var.set_max_devices(config[CONF_MAX_DEVICES]))
    cg.add(var.set_preallocated_cover_slots(config[CONF_PREALLOCATED_COVER_SLOTS]))
    cg.add(var.set_preallocated_light_slots(config[CONF_PREALLOCATED_LIGHT_SLOTS]))

    if CONF_RESPONSE_TEXT_SENSOR in config:
        response_sensor = await text_sensor.new_text_sensor(config[CONF_RESPONSE_TEXT_SENSOR])
        cg.add(var.set_response_text_sensor(response_sensor))
    last_call_sensor = await text_sensor.new_text_sensor(config[CONF_LAST_CALL_TEXT_SENSOR])
    cg.add(var.set_last_call_text_sensor(last_call_sensor))
    last_ok_sensor = await text_sensor.new_text_sensor(config[CONF_LAST_OK_TEXT_SENSOR])
    cg.add(var.set_last_ok_text_sensor(last_ok_sensor))
    last_error_sensor = await text_sensor.new_text_sensor(config[CONF_LAST_ERROR_TEXT_SENSOR])
    cg.add(var.set_last_error_text_sensor(last_error_sensor))
    managed_enabled_sensor = await text_sensor.new_text_sensor(config[CONF_MANAGED_ENABLED_TEXT_SENSOR])
    cg.add(var.set_managed_enabled_text_sensor(managed_enabled_sensor))
    schema_version_sensor = await text_sensor.new_text_sensor(config[CONF_SCHEMA_VERSION_TEXT_SENSOR])
    cg.add(var.set_schema_version_text_sensor(schema_version_sensor))
    component_version_sensor = await text_sensor.new_text_sensor(config[CONF_COMPONENT_VERSION_TEXT_SENSOR])
    cg.add(var.set_component_version_text_sensor(component_version_sensor))
    hub_id_sensor = await text_sensor.new_text_sensor(config[CONF_HUB_ID_TEXT_SENSOR])
    cg.add(var.set_hub_id_text_sensor(hub_id_sensor))
    hub_mac_sensor = await text_sensor.new_text_sensor(config[CONF_HUB_MAC_TEXT_SENSOR])
    cg.add(var.set_hub_mac_text_sensor(hub_mac_sensor))
    max_devices_sensor = await text_sensor.new_text_sensor(config[CONF_MAX_DEVICES_TEXT_SENSOR])
    cg.add(var.set_max_devices_text_sensor(max_devices_sensor))
    revision_sensor = await text_sensor.new_text_sensor(config[CONF_REGISTRY_REVISION_TEXT_SENSOR])
    cg.add(var.set_registry_revision_text_sensor(revision_sensor))
    device_count_sensor = await text_sensor.new_text_sensor(config[CONF_DEVICE_COUNT_TEXT_SENSOR])
    cg.add(var.set_device_count_text_sensor(device_count_sensor))
    entity_materialization_sensor = await text_sensor.new_text_sensor(config[CONF_ENTITY_MATERIALIZATION_TEXT_SENSOR])
    cg.add(var.set_entity_materialization_text_sensor(entity_materialization_sensor))
    materialized_cover_count_sensor = await text_sensor.new_text_sensor(
        config[CONF_MATERIALIZED_COVER_COUNT_TEXT_SENSOR]
    )
    cg.add(var.set_materialized_cover_count_text_sensor(materialized_cover_count_sensor))
    materialized_light_count_sensor = await text_sensor.new_text_sensor(
        config[CONF_MATERIALIZED_LIGHT_COUNT_TEXT_SENSOR]
    )
    cg.add(var.set_materialized_light_count_text_sensor(materialized_light_count_sensor))

    for slot_config in config[CONF_MANAGED_COVER_SLOT_CONFIGS]:
        cover_slot = await cover.new_cover(slot_config)
        await cg.register_component(cover_slot, slot_config)
        cg.add(cover_slot.set_managed_slot(True))
        cg.add(var.add_preallocated_cover_slot(cover_slot))

    for slot_config in config[CONF_MANAGED_LIGHT_SLOT_CONFIGS]:
        light_slot = cg.new_Pvariable(slot_config[CONF_OUTPUT_ID])
        await cg.register_component(light_slot, slot_config)
        await light.register_light(light_slot, slot_config)
        cg.add(light_slot.set_managed_slot(True))
        cg.add(var.add_preallocated_light_slot(light_slot))
