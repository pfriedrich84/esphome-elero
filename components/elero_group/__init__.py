import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover
from esphome.components.elero import CONF_ELERO_ID, elero, elero_ns

DEPENDENCIES = ["elero"]
AUTO_LOAD = ["cover"]

EleroGroupCover = elero_ns.class_("EleroGroupCover", cover.Cover, cg.Component)
# Reference resolved at code-gen time; avoids circular import
EleroCover = elero_ns.class_("EleroCover")

CONF_MEMBERS = "members"
CONF_ASSUMED_STATE = "assumed_state"
CONF_HIDE_MEMBERS = "hide_members"


def _validate_members(config):
    """Require at least 2 members and at most 10 (ELERO_MAX_DESTS)."""
    for group_conf in config:
        members = group_conf[CONF_MEMBERS]
        if len(members) < 2:
            raise cv.Invalid("A group must have at least 2 members")
        if len(members) > 10:
            raise cv.Invalid("A group cannot have more than 10 members (RF packet limit)")
        member_ids = [str(member) for member in members]
        if len(set(member_ids)) != len(member_ids):
            raise cv.Invalid("A group cannot contain the same member more than once")
    return config


CONFIG_SCHEMA = cv.All(
    cv.ensure_list(
        cover.cover_schema(EleroGroupCover)
        .extend(
            {
                cv.GenerateID(CONF_ELERO_ID): cv.use_id(elero),
                cv.Required(CONF_MEMBERS): cv.ensure_list(cv.use_id(EleroCover)),
                cv.Optional(CONF_ASSUMED_STATE, default=True): cv.boolean,
                cv.Optional(CONF_HIDE_MEMBERS, default=False): cv.boolean,
            }
        )
        .extend(cv.COMPONENT_SCHEMA)
    ),
    _validate_members,
)


async def to_code(config):
    for group_conf in config:
        var = await cover.new_cover(group_conf)
        await cg.register_component(var, group_conf)
        parent = await cg.get_variable(group_conf[CONF_ELERO_ID])
        cg.add(var.set_elero_parent(parent))
        cg.add(var.set_assumed_state(group_conf[CONF_ASSUMED_STATE]))
        cg.add(var.set_hide_members(group_conf[CONF_HIDE_MEMBERS]))
        for member_id in group_conf[CONF_MEMBERS]:
            member = await cg.get_variable(member_id)
            cg.add(var.add_member(member))
            if group_conf[CONF_HIDE_MEMBERS]:
                cg.add(member.set_internal(True))
