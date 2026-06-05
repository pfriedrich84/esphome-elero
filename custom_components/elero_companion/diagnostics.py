"""Diagnostics for Elero Companion."""

from __future__ import annotations

from typing import Any

from homeassistant.config_entries import ConfigEntry
from homeassistant.const import CONF_PASSWORD, CONF_USERNAME
from homeassistant.core import HomeAssistant

from .const import CONF_ESPHOME_NODE, CONF_URL
from .coordinator import EleroDataUpdateCoordinator

TO_REDACT = {CONF_USERNAME, CONF_PASSWORD}


async def async_get_config_entry_diagnostics(
    hass: HomeAssistant,
    entry: ConfigEntry,
) -> dict[str, Any]:
    """Return sanitized diagnostics for a config entry."""
    coordinator: EleroDataUpdateCoordinator = entry.runtime_data
    data = coordinator.data

    entry_data = dict(entry.data)
    for key in TO_REDACT:
        if key in entry_data and entry_data[key]:
            entry_data[key] = "REDACTED"

    diagnostics: dict[str, Any] = {
        "entry": {
            "title": entry.title,
            CONF_URL: entry_data.get(CONF_URL),
            CONF_USERNAME: entry_data.get(CONF_USERNAME),
            CONF_ESPHOME_NODE: entry_data.get(CONF_ESPHOME_NODE),
            "has_password": bool(entry.data.get(CONF_PASSWORD)),
        },
        "last_update_success": coordinator.last_update_success,
    }

    if data is not None:
        diagnostics["hub"] = {
            "info": data.info,
            "status": _without_large_fields(data.status),
            "runtime": data.runtime,
            "ui_status": data.ui_status,
            "frequency": data.frequency,
            "yaml_export_available": data.yaml_export is not None,
        }

    return diagnostics


def _without_large_fields(value: dict[str, Any]) -> dict[str, Any]:
    """Remove large or noisy fields from diagnostics."""
    redacted = dict(value)
    redacted.pop("log_entries", None)
    redacted.pop("packets", None)
    return redacted
