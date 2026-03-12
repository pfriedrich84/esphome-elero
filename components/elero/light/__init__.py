import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, sensor, text_sensor
from esphome.const import (
    CONF_CHANNEL,
    CONF_NAME,
    CONF_OUTPUT_ID,
    UNIT_DECIBEL_MILLIWATT,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    STATE_CLASS_MEASUREMENT,
)
from .. import elero_ns, elero, CONF_ELERO_ID

DEPENDENCIES = ["elero"]
AUTO_LOAD = ["sensor", "text_sensor"]

CONF_BLIND_ADDRESS = "blind_address"
CONF_REMOTE_ADDRESS = "remote_address"
CONF_PAYLOAD_1 = "payload_1"
CONF_PAYLOAD_2 = "payload_2"
CONF_PCKINF_1 = "pck_inf1"
CONF_PCKINF_2 = "pck_inf2"
CONF_HOP = "hop"
CONF_DIM_DURATION = "dim_duration"
CONF_COMMAND_ON = "command_on"
CONF_COMMAND_OFF = "command_off"
CONF_COMMAND_DIM_UP = "command_dim_up"
CONF_COMMAND_DIM_DOWN = "command_dim_down"
CONF_COMMAND_STOP = "command_stop"
CONF_COMMAND_CHECK = "command_check"
CONF_AUTO_SENSORS = "auto_sensors"
CONF_RSSI_SENSOR = "rssi_sensor"
CONF_STATUS_SENSOR = "status_sensor"

EleroLight = elero_ns.class_("EleroLight", light.LightOutput, cg.Component)

_RSSI_SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
    state_class=STATE_CLASS_MEASUREMENT,
)
_STATUS_SENSOR_SCHEMA = text_sensor.text_sensor_schema()


def _auto_sensor_validator(config):
    """At validation time, inject auto-sensor sub-configs when auto_sensors=True.

    Running this at validation time (not inside to_code) ensures that
    cv.declare_id() / CORE.register_id() are called in the correct phase and
    auto-generated IDs never collide with IDs from the validation phase.
    """
    if not config.get(CONF_AUTO_SENSORS, True):
        return config
    light_name = config.get(CONF_NAME, "Elero Light")
    result = dict(config)
    if CONF_RSSI_SENSOR not in result:
        result[CONF_RSSI_SENSOR] = _RSSI_SENSOR_SCHEMA(
            {CONF_NAME: f"{light_name} RSSI"}
        )
    if CONF_STATUS_SENSOR not in result:
        result[CONF_STATUS_SENSOR] = _STATUS_SENSOR_SCHEMA(
            {CONF_NAME: f"{light_name} Status"}
        )
    return result


CONFIG_SCHEMA = cv.All(
    light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(EleroLight),
            cv.GenerateID(CONF_ELERO_ID): cv.use_id(elero),
            cv.Required(CONF_BLIND_ADDRESS): cv.hex_int_range(min=0x0, max=0xFFFFFF),
            cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=255),
            cv.Required(CONF_REMOTE_ADDRESS): cv.hex_int_range(min=0x0, max=0xFFFFFF),
            cv.Optional(CONF_DIM_DURATION, default="0s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_PAYLOAD_1, default=0x00): cv.hex_int_range(min=0x0, max=0xFF),
            cv.Optional(CONF_PAYLOAD_2, default=0x04): cv.hex_int_range(min=0x0, max=0xFF),
            cv.Optional(CONF_PCKINF_1, default=0x6A): cv.hex_int_range(min=0x0, max=0xFF),
            cv.Optional(CONF_PCKINF_2, default=0x00): cv.hex_int_range(min=0x0, max=0xFF),
            cv.Optional(CONF_HOP, default=0x0A): cv.hex_int_range(min=0x0, max=0xFF),
            cv.Optional(CONF_COMMAND_ON, default=0x20): cv.hex_int_range(min=0x0, max=0xFF),
            cv.Optional(CONF_COMMAND_OFF, default=0x40): cv.hex_int_range(min=0x0, max=0xFF),
            cv.Optional(CONF_COMMAND_DIM_UP, default=0x20): cv.hex_int_range(min=0x0, max=0xFF),
            cv.Optional(CONF_COMMAND_DIM_DOWN, default=0x40): cv.hex_int_range(min=0x0, max=0xFF),
            cv.Optional(CONF_COMMAND_STOP, default=0x10): cv.hex_int_range(min=0x0, max=0xFF),
            cv.Optional(CONF_COMMAND_CHECK, default=0x00): cv.hex_int_range(min=0x0, max=0xFF),
            cv.Optional(CONF_AUTO_SENSORS, default=True): cv.boolean,
            cv.Optional(CONF_RSSI_SENSOR): _RSSI_SENSOR_SCHEMA,
            cv.Optional(CONF_STATUS_SENSOR): _STATUS_SENSOR_SCHEMA,
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    _auto_sensor_validator,
)


def _final_validate(config):
    """Cross-component validation: detect duplicate blind_address across all lights
    and cross-platform duplicates with covers."""
    from esphome.core import CORE

    if CORE.config is None:
        return config

    seen_addresses = {}

    # Check for duplicates among all elero light configs
    full_light_config = CORE.config.get("light", [])
    for entry in full_light_config:
        if entry.get("platform") != "elero":
            continue
        addr = entry.get(CONF_BLIND_ADDRESS)
        name = entry.get(CONF_NAME, "unnamed")
        if addr is None:
            continue
        if addr in seen_addresses:
            raise cv.Invalid(
                f"Duplicate blind_address 0x{addr:06x}: used by both "
                f"'{seen_addresses[addr]}' and '{name}'. "
                f"Each Elero light must have a unique blind_address."
            )
        seen_addresses[addr] = name

    # Cross-platform check: also detect collisions with elero covers
    full_cover_config = CORE.config.get("cover", [])
    for entry in full_cover_config:
        if entry.get("platform") != "elero":
            continue
        addr = entry.get(CONF_BLIND_ADDRESS)
        name = entry.get(CONF_NAME, "unnamed")
        if addr is None:
            continue
        if addr in seen_addresses:
            raise cv.Invalid(
                f"Duplicate blind_address 0x{addr:06x}: used by light "
                f"'{seen_addresses[addr]}' and cover '{name}'. "
                f"Each Elero device must have a unique blind_address."
            )

    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    parent = await cg.get_variable(config[CONF_ELERO_ID])
    cg.add(var.set_elero_parent(parent))
    cg.add(var.set_blind_address(config[CONF_BLIND_ADDRESS]))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_remote_address(config[CONF_REMOTE_ADDRESS]))
    cg.add(var.set_dim_duration(config[CONF_DIM_DURATION]))
    cg.add(var.set_payload_1(config[CONF_PAYLOAD_1]))
    cg.add(var.set_payload_2(config[CONF_PAYLOAD_2]))
    cg.add(var.set_pckinf_1(config[CONF_PCKINF_1]))
    cg.add(var.set_pckinf_2(config[CONF_PCKINF_2]))
    cg.add(var.set_hop(config[CONF_HOP]))
    cg.add(var.set_command_on(config[CONF_COMMAND_ON]))
    cg.add(var.set_command_off(config[CONF_COMMAND_OFF]))
    cg.add(var.set_command_dim_up(config[CONF_COMMAND_DIM_UP]))
    cg.add(var.set_command_dim_down(config[CONF_COMMAND_DIM_DOWN]))
    cg.add(var.set_command_stop(config[CONF_COMMAND_STOP]))
    cg.add(var.set_command_check(config[CONF_COMMAND_CHECK]))

    addr = config[CONF_BLIND_ADDRESS]

    # RSSI sensor — present when explicitly configured or auto_sensors=True
    if CONF_RSSI_SENSOR in config:
        rssi_var = await sensor.new_sensor(config[CONF_RSSI_SENSOR])
        cg.add(parent.register_rssi_sensor(addr, rssi_var))

    # Status text sensor — present when explicitly configured or auto_sensors=True
    if CONF_STATUS_SENSOR in config:
        status_var = await text_sensor.new_text_sensor(config[CONF_STATUS_SENSOR])
        cg.add(parent.register_text_sensor(addr, status_var))
