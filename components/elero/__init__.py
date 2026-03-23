import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi
from esphome.const import CONF_ID
from esphome.core import CORE

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["spi"]

# ESP32 strapping pins that can cause boot issues when used for SPI or I/O.
# GPIO12 (MTDI) is especially problematic on original ESP32: if pulled HIGH at
# boot by an SPI device, VDD_SDIO is set to 1.8V, breaking all SPI communication.
ESP32_STRAPPING_PINS = {0, 2, 5, 12, 15}

# ESP32-S3 has different strapping pins. GPIO12 is NOT a strapping pin on S3.
# GPIO3 = JTAG signal source select (harmless in practice).
ESP32_S3_STRAPPING_PINS = {0, 3, 45, 46}

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


def _validate_strapping_pins(config):
    """Validate pin usage based on ESP32 variant strapping pins."""
    variant = CORE.data.get("esp32", {}).get("variant", "ESP32")
    is_s3 = variant == "ESP32S3"
    strapping_pins = ESP32_S3_STRAPPING_PINS if is_s3 else ESP32_STRAPPING_PINS

    for key in (CONF_GDO0_PIN, "cs_pin"):
        pin_conf = config.get(key)
        if pin_conf is None:
            continue
        # pin_conf may be a dict with "number" or an int directly
        pin_num = pin_conf.get("number") if isinstance(pin_conf, dict) else pin_conf
        if not isinstance(pin_num, int):
            continue
        # GPIO12 is catastrophic on original ESP32 — block compilation.
        # It controls VDD_SDIO voltage at boot; if the CC1101 pulls it HIGH,
        # VDD_SDIO locks at 1.8V and breaks all SPI communication permanently.
        # On ESP32-S3, GPIO12 is NOT a strapping pin and is safe to use.
        if pin_num == 12 and not is_s3:
            raise cv.Invalid(
                f"GPIO12 cannot be used for {key} on original ESP32 (strapping pin "
                f"controlling VDD_SDIO). If the CC1101 pulls GPIO12 HIGH at boot, "
                f"VDD_SDIO is set to 1.8V, breaking all SPI communication "
                f"(symptoms: 'SPI write verify failed: rc=-16', MARCSTATE stuck at "
                f"0x00). Use non-strapping pins instead: CLK=GPIO18, MISO=GPIO19, "
                f"MOSI=GPIO23, CS=GPIO5, GDO0=GPIO26."
            )
        # Warn about strapping pins (variant-specific set).
        if pin_num in strapping_pins:
            variant_label = "ESP32-S3" if is_s3 else "ESP32"
            _LOGGER.warning(
                "GPIO%d (%s) is an %s strapping pin. Attaching external "
                "pull-up/down resistors or peripherals can cause unexpected "
                "boot failures. This is usually safe for GDO0/CS if no "
                "external pull resistors conflict with boot requirements.",
                pin_num,
                key,
                variant_label,
            )
    return config


FINAL_VALIDATE_SCHEMA = _validate_strapping_pins


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
