import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi
from esphome.const import CONF_ID

DEPENDENCIES = ["spi"]

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
