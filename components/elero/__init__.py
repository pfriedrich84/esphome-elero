import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor as esphome_sensor
from esphome.components import spi
from esphome.const import (
    CONF_ID,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
)
from esphome.core import CORE

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["sensor"]

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
CONF_AUTO_SENSORS = "auto_sensors"
CONF_FREQUENCY_SENSOR = "frequency_sensor"
CONF_RX_COUNT_SENSOR = "rx_count_sensor"
CONF_TX_COUNT_SENSOR = "tx_count_sensor"
CONF_WATCHDOG_RECOVERY_SENSOR = "watchdog_recovery_sensor"

_FREQUENCY_SENSOR_SCHEMA = esphome_sensor.sensor_schema(
    unit_of_measurement="MHz",
    accuracy_decimals=2,
    state_class=STATE_CLASS_MEASUREMENT,
    icon="mdi:sine-wave",
)
_RX_COUNT_SENSOR_SCHEMA = esphome_sensor.sensor_schema(
    accuracy_decimals=0,
    state_class=STATE_CLASS_TOTAL_INCREASING,
    icon="mdi:counter",
)
_TX_COUNT_SENSOR_SCHEMA = esphome_sensor.sensor_schema(
    accuracy_decimals=0,
    state_class=STATE_CLASS_TOTAL_INCREASING,
    icon="mdi:counter",
)
_WATCHDOG_RECOVERY_SENSOR_SCHEMA = esphome_sensor.sensor_schema(
    accuracy_decimals=0,
    state_class=STATE_CLASS_TOTAL_INCREASING,
    icon="mdi:alert-circle-outline",
)


def _auto_sensor_validator(config):
    """At validation time, inject hub-level diagnostic sensor configs when auto_sensors=True."""
    if not config.get(CONF_AUTO_SENSORS, True):
        return config
    result = dict(config)
    prefix = "Elero"
    if CONF_FREQUENCY_SENSOR not in result:
        result[CONF_FREQUENCY_SENSOR] = _FREQUENCY_SENSOR_SCHEMA({"name": f"{prefix} Frequency"})
    if CONF_RX_COUNT_SENSOR not in result:
        result[CONF_RX_COUNT_SENSOR] = _RX_COUNT_SENSOR_SCHEMA({"name": f"{prefix} RX Count"})
    if CONF_TX_COUNT_SENSOR not in result:
        result[CONF_TX_COUNT_SENSOR] = _TX_COUNT_SENSOR_SCHEMA({"name": f"{prefix} TX Count"})
    if CONF_WATCHDOG_RECOVERY_SENSOR not in result:
        result[CONF_WATCHDOG_RECOVERY_SENSOR] = _WATCHDOG_RECOVERY_SENSOR_SCHEMA(
            {"name": f"{prefix} Watchdog Recovery Count"}
        )
    return result


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(elero),
            cv.Required(CONF_GDO0_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_FREQ0, default=0x7A): cv.hex_int_range(min=0x0, max=0xFF),
            cv.Optional(CONF_FREQ1, default=0x71): cv.hex_int_range(min=0x0, max=0xFF),
            cv.Optional(CONF_FREQ2, default=0x21): cv.hex_int_range(min=0x0, max=0xFF),
            cv.Optional(CONF_SEND_REPEATS, default=1): cv.int_range(min=1, max=20),
            cv.Optional(CONF_SEND_DELAY, default="10ms"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_AUTO_SENSORS, default=True): cv.boolean,
            cv.Optional(CONF_FREQUENCY_SENSOR): _FREQUENCY_SENSOR_SCHEMA,
            cv.Optional(CONF_RX_COUNT_SENSOR): _RX_COUNT_SENSOR_SCHEMA,
            cv.Optional(CONF_TX_COUNT_SENSOR): _TX_COUNT_SENSOR_SCHEMA,
            cv.Optional(CONF_WATCHDOG_RECOVERY_SENSOR): _WATCHDOG_RECOVERY_SENSOR_SCHEMA,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True)),
    _auto_sensor_validator,
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

    # Hub-level diagnostic sensors (auto_sensors or explicitly configured)
    if CONF_FREQUENCY_SENSOR in config:
        freq_sens = await esphome_sensor.new_sensor(config[CONF_FREQUENCY_SENSOR])
        cg.add(var.set_frequency_sensor(freq_sens))
    if CONF_RX_COUNT_SENSOR in config:
        rx_sens = await esphome_sensor.new_sensor(config[CONF_RX_COUNT_SENSOR])
        cg.add(var.set_rx_count_sensor(rx_sens))
    if CONF_TX_COUNT_SENSOR in config:
        tx_sens = await esphome_sensor.new_sensor(config[CONF_TX_COUNT_SENSOR])
        cg.add(var.set_tx_count_sensor(tx_sens))
    if CONF_WATCHDOG_RECOVERY_SENSOR in config:
        wd_sens = await esphome_sensor.new_sensor(config[CONF_WATCHDOG_RECOVERY_SENSOR])
        cg.add(var.set_watchdog_recovery_sensor(wd_sens))

    # Add RadioLib as PlatformIO library dependency.
    # RadioLib's Module.h includes <SPI.h> when RADIOLIB_BUILD_ARDUINO is defined
    # (i.e., when the ARDUINO macro is >= 100). The Arduino SPI library is a
    # built-in framework library, but ESPHome sets lib_ldf_mode=off which prevents
    # PlatformIO from auto-discovering it. Adding "SPI" explicitly ensures the
    # Arduino SPI include path is available when compiling RadioLib.
    cg.add_library("jgromes/RadioLib", "7.1.2")
    cg.add_library("SPI", None)

    # Exclude all unused RadioLib modules to reduce firmware binary size.
    # This project only uses CC1101; all other radio drivers and protocol
    # decoders are dead code.  RadioLib v7.1.2 honours RADIOLIB_EXCLUDE_*
    # preprocessor defines (guarded via #if in each module header/source).
    # Using add_build_flag (not add_define) so flags propagate to library code.
    _radiolib_exclusions = [
        # Hardware radio modules (everything except CC1101)
        "RADIOLIB_EXCLUDE_NRF24",
        "RADIOLIB_EXCLUDE_RF69",
        "RADIOLIB_EXCLUDE_SX1231",
        "RADIOLIB_EXCLUDE_SI443X",
        "RADIOLIB_EXCLUDE_RFM2X",
        "RADIOLIB_EXCLUDE_SX127X",
        "RADIOLIB_EXCLUDE_SX126X",
        # STM32WLX is NOT listed here — RadioLib auto-excludes it on non-STM32 platforms
        "RADIOLIB_EXCLUDE_SX128X",
        "RADIOLIB_EXCLUDE_LR11X0",
        # Protocol decoders (none used by this project)
        "RADIOLIB_EXCLUDE_AFSK",
        "RADIOLIB_EXCLUDE_AX25",
        "RADIOLIB_EXCLUDE_APRS",
        "RADIOLIB_EXCLUDE_BELL",
        "RADIOLIB_EXCLUDE_HELLSCHREIBER",
        "RADIOLIB_EXCLUDE_MORSE",
        "RADIOLIB_EXCLUDE_PAGER",
        "RADIOLIB_EXCLUDE_RTTY",
        "RADIOLIB_EXCLUDE_SSTV",
        "RADIOLIB_EXCLUDE_FSK4",
        "RADIOLIB_EXCLUDE_LORAWAN",
        "RADIOLIB_EXCLUDE_DIRECT_RECEIVE",
    ]
    for flag in _radiolib_exclusions:
        cg.add_build_flag(f"-D{flag}=1")

    # Reserve a log listener slot so add_log_listener() works at runtime.
    # Required for ESPHome 2026.1.0+ (StaticVector migration).
    try:
        from esphome.components.logger import request_log_listener

        request_log_listener()
    except ImportError:
        pass  # Older ESPHome without StaticVector migration
