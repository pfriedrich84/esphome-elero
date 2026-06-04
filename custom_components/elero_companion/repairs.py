"""Repairs support for Elero Companion."""

from __future__ import annotations

from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers import issue_registry as ir

from .const import DOMAIN
from .coordinator import EleroDataUpdateCoordinator

ISSUE_IDS = {
    "web_ui_disabled",
    "no_devices_configured",
    "runtime_devices_not_persisted",
    "radio_watchdog_recoveries",
    "tx_drops",
}


def async_update_issues(
    hass: HomeAssistant,
    entry: ConfigEntry,
    coordinator: EleroDataUpdateCoordinator | None,
) -> None:
    """Create or clear Repairs issues for the current hub state."""
    active: set[str] = set()

    if coordinator is not None and coordinator.data is not None:
        data = coordinator.data
        diagnostics = data.diagnostics

        if data.web_ui_enabled is False:
            active.add("web_ui_disabled")

        if data.configured_covers == 0 and data.configured_lights == 0:
            active.add("no_devices_configured")

        if data.runtime_covers > 0 or data.runtime_lights > 0:
            active.add("runtime_devices_not_persisted")

        if int(diagnostics.get("watchdog_recovery_count") or 0) > 0:
            active.add("radio_watchdog_recoveries")

        if int(diagnostics.get("tx_drop_count") or 0) > 0:
            active.add("tx_drops")

    for issue_id in ISSUE_IDS - active:
        ir.async_delete_issue(hass, DOMAIN, _entry_issue_id(entry, issue_id))

    for issue_id in active:
        ir.async_create_issue(
            hass,
            DOMAIN,
            _entry_issue_id(entry, issue_id),
            is_fixable=False,
            severity=ir.IssueSeverity.WARNING,
            translation_key=issue_id,
            translation_placeholders={"name": entry.title},
        )


def _entry_issue_id(entry: ConfigEntry, issue_id: str) -> str:
    """Return an issue id scoped to a config entry."""
    return f"{entry.entry_id}_{issue_id}"
