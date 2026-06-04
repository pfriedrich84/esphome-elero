"""Elero Companion integration."""

from __future__ import annotations

from homeassistant.config_entries import ConfigEntry
from homeassistant.const import CONF_PASSWORD, CONF_USERNAME, Platform
from homeassistant.core import HomeAssistant
from homeassistant.helpers.aiohttp_client import async_get_clientsession

from .api import EleroApiClient
from .const import CONF_URL, DOMAIN, MANUFACTURER, MODEL
from .coordinator import EleroDataUpdateCoordinator
from .repairs import async_update_issues

PLATFORMS: list[Platform] = [Platform.SENSOR, Platform.BINARY_SENSOR]


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Set up Elero Companion from a config entry."""
    api = EleroApiClient(
        async_get_clientsession(hass),
        entry.data[CONF_URL],
        entry.data.get(CONF_USERNAME),
        entry.data.get(CONF_PASSWORD),
    )
    coordinator = EleroDataUpdateCoordinator(hass, api)
    await coordinator.async_config_entry_first_refresh()

    entry.runtime_data = coordinator
    entry.async_on_unload(
        coordinator.async_add_listener(lambda: async_update_issues(hass, entry, coordinator))
    )

    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)
    async_update_issues(hass, entry, coordinator)
    return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Unload an Elero Companion config entry."""
    unload_ok = await hass.config_entries.async_unload_platforms(entry, PLATFORMS)
    if unload_ok:
        async_update_issues(hass, entry, None)
    return unload_ok


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
