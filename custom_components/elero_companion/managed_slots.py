"""Home Assistant entity-registry helpers for managed slot placeholders."""

from __future__ import annotations

import logging
import re

from homeassistant.core import HomeAssistant
from homeassistant.helpers import entity_registry as er

_LOGGER = logging.getLogger(__name__)

_COVER_COUNT_NAMES = {"Elero Managed Materialized Cover Count"}
_LIGHT_COUNT_NAMES = {"Elero Managed Materialized Light Count"}
_SLOT_RE = re.compile(r"^Elero Managed (?P<kind>Cover|Light) Slot (?P<index>\d+)$")
_ENTITY_ID_RE = re.compile(r"^(?P<domain>cover|light)\.elero_managed_(?P<kind>cover|light)_slot_(?P<index>\d+)$")


def async_sync_managed_slot_entities(hass: HomeAssistant) -> None:
    """Disable unbound managed placeholder slot entities in Home Assistant.

    ESPHome must pre-generate all slot entities at compile time. Home Assistant's
    entity registry is the right place to hide unused placeholders while leaving
    active bound slots enabled. We only touch entities that still match the
    generated managed slot naming pattern, and we only re-enable entities that
    this integration previously disabled.
    """
    cover_count = _state_int_by_friendly_name(hass, _COVER_COUNT_NAMES)
    light_count = _state_int_by_friendly_name(hass, _LIGHT_COUNT_NAMES)
    if cover_count is None and light_count is None:
        return

    registry = er.async_get(hass)
    for entry in registry.entities.values():
        slot = _slot_from_entry(entry)
        if slot is None:
            continue
        domain, index = slot
        active_count = cover_count if domain == "cover" else light_count
        if active_count is None:
            continue
        should_disable = index > active_count
        if should_disable:
            if entry.disabled_by is None:
                registry.async_update_entity(entry.entity_id, disabled_by=er.RegistryEntryDisabler.INTEGRATION)
                _LOGGER.debug("Disabled unbound managed placeholder entity %s", entry.entity_id)
        elif entry.disabled_by == er.RegistryEntryDisabler.INTEGRATION:
            registry.async_update_entity(entry.entity_id, disabled_by=None)
            _LOGGER.debug("Re-enabled bound managed slot entity %s", entry.entity_id)


def _state_int_by_friendly_name(hass: HomeAssistant, names: set[str]) -> int | None:
    """Return an integer state for the first entity with a matching friendly name."""
    for state in hass.states.async_all("text_sensor"):
        if state.attributes.get("friendly_name") in names:
            try:
                return int(state.state)
            except (TypeError, ValueError):
                return None
    return None


def _slot_from_entry(entry: er.RegistryEntry) -> tuple[str, int] | None:
    """Return (domain, one-based slot index) for a managed slot registry entry."""
    domain = entry.entity_id.split(".", 1)[0]
    if domain not in {"cover", "light"}:
        return None

    for candidate in (entry.original_name, entry.name):
        if not candidate:
            continue
        match = _SLOT_RE.match(candidate)
        if match is None:
            continue
        kind = match.group("kind").lower()
        if kind != domain:
            return None
        return domain, int(match.group("index"))

    match = _ENTITY_ID_RE.match(entry.entity_id)
    if match is None:
        return None
    return domain, int(match.group("index"))
