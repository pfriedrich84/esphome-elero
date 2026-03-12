import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi
from esphome.const import CONF_ID

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["spi"]

# ESP32 strapping pins that can cause boot issues when used for SPI or I/O.
# GPIO12 (MTDI) is especially problematic: if pulled HIGH at boot by an SPI
# device, VDD_SDIO is set to 1.8V, breaking all SPI communication.
ESP32_STRAPPING_PINS = {0, 2, 5, 12, 15}

elero_ns = cg.esphome_ns.namespace("elero")
elero = elero_ns.class_("Elero", spi.SPIDevice, cg.Component)

CONF_GDO0_PIN = "gdo0_pin"
CONF_ELERO_ID = "elero_id"
CONF_FREQ0 = "freq0"
CONF_FREQ1 = "freq1"
CONF_FREQ2 = "freq2"
CONF_SEND_REPEATS = "send_repeats"
CONF_SEND_DELAY = "send_delay"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(elero),
            cv.Required(CONF_GDO0_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_FREQ0, default=0x7a): cv.hex_int_range(min=0x0, max=0xff),
            cv.Optional(CONF_FREQ1, default=0x71): cv.hex_int_range(min=0x0, max=0xff),
            cv.Optional(CONF_FREQ2, default=0x21): cv.hex_int_range(min=0x0, max=0xff),
            cv.Optional(CONF_SEND_REPEATS, default=5): cv.int_range(min=1, max=20),
            cv.Optional(CONF_SEND_DELAY, default="1ms"): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True))
)


def _warn_strapping_pins(config):
    """Warn if gdo0_pin or cs_pin uses an ESP32 strapping pin."""
    for key in (CONF_GDO0_PIN, "cs_pin"):
        pin_conf = config.get(key)
        if pin_conf is None:
            continue
        # pin_conf may be a dict with "number" or an int directly
        pin_num = pin_conf.get("number") if isinstance(pin_conf, dict) else pin_conf
        if isinstance(pin_num, int) and pin_num in ESP32_STRAPPING_PINS:
            _LOGGER.warning(
                "GPIO%d (%s) is an ESP32 strapping pin. If used for SPI "
                "(especially GPIO12 as MISO), it can cause VDD_SDIO voltage "
                "issues that break CC1101 communication. Consider using "
                "non-strapping pins (e.g. CLK=18, MISO=19, MOSI=23, CS=5, "
                "GDO0=26).",
                pin_num,
                key,
            )
    return config


FINAL_VALIDATE_SCHEMA = _warn_strapping_pins


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    gdo0_pin = await cg.gpio_pin_expression(config[CONF_GDO0_PIN])
    cg.add(var.set_gdo0_pin(gdo0_pin))
    cg.add(var.set_freq0(config[CONF_FREQ0]))
    cg.add(var.set_freq1(config[CONF_FREQ1]))
    cg.add(var.set_freq2(config[CONF_FREQ2]))
    cg.add(var.set_send_repeats(config[CONF_SEND_REPEATS]))
    cg.add(var.set_send_delay(config[CONF_SEND_DELAY].total_milliseconds))

    # Add RadioLib as PlatformIO library dependency
    cg.add_library("jgromes/RadioLib", "7.1.2")

    # Reserve a log listener slot so add_log_listener() works at runtime.
    # Required for ESPHome 2026.1.0+ (StaticVector migration).
    try:
        from esphome.components.logger import request_log_listener
        request_log_listener()
    except ImportError:
        pass  # Older ESPHome without StaticVector migration
