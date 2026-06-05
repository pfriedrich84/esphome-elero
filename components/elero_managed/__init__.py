import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

from ..elero import CONF_ELERO_ID, elero

DEPENDENCIES = ["elero"]

elero_managed_ns = cg.esphome_ns.namespace("elero_managed")
EleroManaged = elero_managed_ns.class_("EleroManaged", cg.Component)

CONF_ENABLED = "enabled"
CONF_MAX_DEVICES = "max_devices"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(EleroManaged),
        cv.GenerateID(CONF_ELERO_ID): cv.use_id(elero),
        cv.Optional(CONF_ENABLED, default=False): cv.boolean,
        cv.Optional(CONF_MAX_DEVICES, default=32): cv.int_range(min=0, max=32),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    hub = await cg.get_variable(config[CONF_ELERO_ID])
    cg.add(var.set_parent(hub))
    cg.add(var.set_enabled(config[CONF_ENABLED]))
    cg.add(var.set_max_devices(config[CONF_MAX_DEVICES]))
