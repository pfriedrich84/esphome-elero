"""Binary sensors for Elero Companion."""

from __future__ import annotations

from homeassistant.components.binary_sensor import BinarySensorDeviceClass, BinarySensorEntity, BinarySensorEntityDescription
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from . import device_info
from .coordinator import EleroDataUpdateCoordinator


WEB_UI_DESCRIPTION = BinarySensorEntityDescription(
    key="web_ui_enabled",
    translation_key="web_ui_enabled",
    device_class=BinarySensorDeviceClass.CONNECTIVITY,
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up Elero binary sensors."""
    coordinator: EleroDataUpdateCoordinator = entry.runtime_data
    async_add_entities([EleroWebUiEnabledBinarySensor(coordinator, entry)])


class EleroWebUiEnabledBinarySensor(CoordinatorEntity[EleroDataUpdateCoordinator], BinarySensorEntity):
    """Whether the Elero web UI/API reports enabled."""

    entity_description = WEB_UI_DESCRIPTION

    def __init__(self, coordinator: EleroDataUpdateCoordinator, entry: ConfigEntry) -> None:
        super().__init__(coordinator)
        self.entry = entry
        self._attr_unique_id = f"{entry.entry_id}_web_ui_enabled"
        self._attr_device_info = device_info(entry)
        self._attr_has_entity_name = True

    @property
    def is_on(self) -> bool | None:
        """Return whether the web UI/API is enabled."""
        if self.coordinator.data is None:
            return None
        return self.coordinator.data.web_ui_enabled
