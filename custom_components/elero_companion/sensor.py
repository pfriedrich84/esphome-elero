"""Diagnostic sensors for Elero Companion."""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass
from typing import Any

from homeassistant.components.sensor import SensorEntity, SensorEntityDescription, SensorStateClass
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import UnitOfTime
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from . import device_info
from .api import EleroHubData
from .coordinator import EleroDataUpdateCoordinator


@dataclass(frozen=True, kw_only=True)
class EleroSensorEntityDescription(SensorEntityDescription):
    """Description for Elero diagnostic sensors."""

    value_fn: Callable[[EleroHubData], Any]


SENSOR_DESCRIPTIONS: tuple[EleroSensorEntityDescription, ...] = (
    EleroSensorEntityDescription(
        key="frequency_mhz",
        translation_key="frequency_mhz",
        native_unit_of_measurement="MHz",
        value_fn=lambda data: data.frequency.get("mhz"),
    ),
    EleroSensorEntityDescription(
        key="configured_covers",
        translation_key="configured_covers",
        state_class=SensorStateClass.MEASUREMENT,
        value_fn=lambda data: data.configured_covers,
    ),
    EleroSensorEntityDescription(
        key="configured_lights",
        translation_key="configured_lights",
        state_class=SensorStateClass.MEASUREMENT,
        value_fn=lambda data: data.configured_lights,
    ),
    EleroSensorEntityDescription(
        key="runtime_covers",
        translation_key="runtime_covers",
        state_class=SensorStateClass.MEASUREMENT,
        value_fn=lambda data: data.runtime_covers,
    ),
    EleroSensorEntityDescription(
        key="runtime_lights",
        translation_key="runtime_lights",
        state_class=SensorStateClass.MEASUREMENT,
        value_fn=lambda data: data.runtime_lights,
    ),
    EleroSensorEntityDescription(
        key="rx_count",
        translation_key="rx_count",
        state_class=SensorStateClass.TOTAL_INCREASING,
        value_fn=lambda data: data.diagnostics.get("rx_count"),
    ),
    EleroSensorEntityDescription(
        key="tx_count",
        translation_key="tx_count",
        state_class=SensorStateClass.TOTAL_INCREASING,
        value_fn=lambda data: data.diagnostics.get("tx_count"),
    ),
    EleroSensorEntityDescription(
        key="watchdog_recovery_count",
        translation_key="watchdog_recovery_count",
        state_class=SensorStateClass.TOTAL_INCREASING,
        value_fn=lambda data: data.diagnostics.get("watchdog_recovery_count"),
    ),
    EleroSensorEntityDescription(
        key="tx_drop_count",
        translation_key="tx_drop_count",
        state_class=SensorStateClass.TOTAL_INCREASING,
        value_fn=lambda data: data.diagnostics.get("tx_drop_count"),
    ),
    EleroSensorEntityDescription(
        key="drop_crc_fail",
        translation_key="drop_crc_fail",
        state_class=SensorStateClass.TOTAL_INCREASING,
        value_fn=lambda data: data.diagnostics.get("drop_crc_fail"),
    ),
    EleroSensorEntityDescription(
        key="drop_bounds",
        translation_key="drop_bounds",
        state_class=SensorStateClass.TOTAL_INCREASING,
        value_fn=lambda data: data.diagnostics.get("drop_bounds"),
    ),
    EleroSensorEntityDescription(
        key="tx_queue_latency_last_ms",
        translation_key="tx_queue_latency_last_ms",
        native_unit_of_measurement=UnitOfTime.MILLISECONDS,
        value_fn=lambda data: data.diagnostics.get("tx_queue_latency_last_ms"),
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up Elero diagnostic sensors."""
    coordinator: EleroDataUpdateCoordinator = entry.runtime_data
    async_add_entities(EleroSensor(coordinator, entry, description) for description in SENSOR_DESCRIPTIONS)


class EleroSensor(CoordinatorEntity[EleroDataUpdateCoordinator], SensorEntity):
    """Elero hub diagnostic sensor."""

    entity_description: EleroSensorEntityDescription

    def __init__(
        self,
        coordinator: EleroDataUpdateCoordinator,
        entry: ConfigEntry,
        description: EleroSensorEntityDescription,
    ) -> None:
        super().__init__(coordinator)
        self.entry = entry
        self.entity_description = description
        self._attr_unique_id = f"{entry.entry_id}_{description.key}"
        self._attr_device_info = device_info(entry)
        self._attr_has_entity_name = True

    @property
    def native_value(self) -> Any:
        """Return the sensor value."""
        if self.coordinator.data is None:
            return None
        return self.entity_description.value_fn(self.coordinator.data)
