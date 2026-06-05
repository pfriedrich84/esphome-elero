"""Elero Companion integration."""

from __future__ import annotations

import voluptuous as vol
from homeassistant.components import persistent_notification
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import CONF_PASSWORD, CONF_USERNAME, Platform
from homeassistant.core import HomeAssistant, ServiceCall
from homeassistant.exceptions import HomeAssistantError
from homeassistant.helpers.aiohttp_client import async_get_clientsession

from .api import EleroApiClient
from .const import (
    CONF_ESPHOME_NODE,
    CONF_URL,
    DOMAIN,
    MANUFACTURER,
    MODEL,
    SERVICE_CLEAR_MANAGED_REGISTRY,
    SERVICE_GET_INFO,
    SERVICE_PUSH_MANAGED_REGISTRY,
    SERVICE_VALIDATE_MANAGED_REGISTRY,
)
from .coordinator import EleroDataUpdateCoordinator
from .managed_slots import async_sync_managed_slot_entities
from .repairs import async_update_issues

PLATFORMS: list[Platform] = [Platform.SENSOR, Platform.BINARY_SENSOR]

_SERVICE_ENTRY_FIELD = vol.Optional("entry_id")
_REGISTRY_SERVICE_SCHEMA = vol.Schema({_SERVICE_ENTRY_FIELD: str, vol.Required("registry_json"): str})
_CLEAR_SERVICE_SCHEMA = vol.Schema(
    {
        _SERVICE_ENTRY_FIELD: str,
        vol.Required("registry_revision"): vol.Coerce(int),
        vol.Optional("confirm", default="CLEAR"): str,
    }
)
_ENTRY_ONLY_SERVICE_SCHEMA = vol.Schema({_SERVICE_ENTRY_FIELD: str})


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Set up Elero Companion from a config entry."""
    api = EleroApiClient(
        async_get_clientsession(hass),
        entry.data[CONF_URL],
        entry.data.get(CONF_USERNAME),
        entry.data.get(CONF_PASSWORD),
    )
    coordinator = EleroDataUpdateCoordinator(
        hass,
        api,
        entry.options.get(CONF_ESPHOME_NODE) or entry.data.get(CONF_ESPHOME_NODE) or None,
    )
    await coordinator.async_config_entry_first_refresh()

    entry.runtime_data = coordinator

    def _handle_coordinator_update() -> None:
        async_update_issues(hass, entry, coordinator)
        async_sync_managed_slot_entities(hass)

    entry.async_on_unload(coordinator.async_add_listener(_handle_coordinator_update))

    _async_setup_services(hass)

    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)
    async_update_issues(hass, entry, coordinator)
    async_sync_managed_slot_entities(hass)
    return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Unload an Elero Companion config entry."""
    unload_ok = await hass.config_entries.async_unload_platforms(entry, PLATFORMS)
    if unload_ok:
        async_update_issues(hass, entry, None)
    return unload_ok


def _async_setup_services(hass: HomeAssistant) -> None:
    """Register companion-level managed registry services."""
    if hass.services.has_service(DOMAIN, SERVICE_PUSH_MANAGED_REGISTRY):
        return

    async def async_get_info(call: ServiceCall) -> None:
        coordinator = _coordinator_for_service_call(hass, call)
        await _native_api(coordinator).async_get_info()

    async def async_validate_registry(call: ServiceCall) -> None:
        coordinator = _coordinator_for_service_call(hass, call)
        await _native_api(coordinator).async_validate_managed_registry(call.data["registry_json"])

    async def async_push_registry(call: ServiceCall) -> None:
        coordinator = _coordinator_for_service_call(hass, call)
        await _native_api(coordinator).async_push_managed_registry(call.data["registry_json"])
        async_sync_managed_slot_entities(hass)
        persistent_notification.async_create(
            hass,
            "Managed Elero registry pushed. Restart the ESPHome node so generated managed slots bind the new registry.",
            title="Elero managed registry restart required",
            notification_id=f"{DOMAIN}_managed_registry_restart_required",
        )

    async def async_clear_registry(call: ServiceCall) -> None:
        coordinator = _coordinator_for_service_call(hass, call)
        await _native_api(coordinator).async_clear_managed_registry(
            call.data["registry_revision"],
            call.data["confirm"],
        )
        async_sync_managed_slot_entities(hass)
        persistent_notification.async_create(
            hass,
            "Managed Elero registry cleared. Restart the ESPHome node so managed slot availability reflects the cleared registry.",
            title="Elero managed registry restart recommended",
            notification_id=f"{DOMAIN}_managed_registry_restart_required",
        )

    hass.services.async_register(DOMAIN, SERVICE_GET_INFO, async_get_info, schema=_ENTRY_ONLY_SERVICE_SCHEMA)
    hass.services.async_register(
        DOMAIN,
        SERVICE_VALIDATE_MANAGED_REGISTRY,
        async_validate_registry,
        schema=_REGISTRY_SERVICE_SCHEMA,
    )
    hass.services.async_register(
        DOMAIN,
        SERVICE_PUSH_MANAGED_REGISTRY,
        async_push_registry,
        schema=_REGISTRY_SERVICE_SCHEMA,
    )
    hass.services.async_register(
        DOMAIN,
        SERVICE_CLEAR_MANAGED_REGISTRY,
        async_clear_registry,
        schema=_CLEAR_SERVICE_SCHEMA,
    )


def _coordinator_for_service_call(hass: HomeAssistant, call: ServiceCall) -> EleroDataUpdateCoordinator:
    """Return the target coordinator for a companion service call."""
    entries = [entry for entry in hass.config_entries.async_entries(DOMAIN) if getattr(entry, "runtime_data", None)]
    entry_id = call.data.get("entry_id")
    if entry_id:
        entries = [entry for entry in entries if entry.entry_id == entry_id]
    if not entries:
        raise HomeAssistantError("No loaded Elero Companion config entry matches the service call")
    if len(entries) > 1:
        raise HomeAssistantError("Multiple Elero Companion entries are loaded; provide entry_id")
    return entries[0].runtime_data


def _native_api(coordinator: EleroDataUpdateCoordinator):
    """Return the Native API adapter for a coordinator."""
    if coordinator.native_api is None:
        raise HomeAssistantError(
            "Configure esphome_node for this Elero Companion entry before using managed registry services"
        )
    return coordinator.native_api


def device_info(entry: ConfigEntry) -> dict:
    """Return device registry info for the Elero hub."""
    coordinator = entry.runtime_data
    return {
        "identifiers": {(DOMAIN, entry.entry_id)},
        "manufacturer": MANUFACTURER,
        "model": MODEL,
        "name": coordinator.data.device_name if coordinator.data else entry.title,
        "configuration_url": entry.data.get(CONF_URL),
    }
