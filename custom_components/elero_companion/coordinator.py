"""Data coordinator for Elero Companion."""

from __future__ import annotations

import logging

from homeassistant.core import HomeAssistant
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator, UpdateFailed

from .api import EleroApiClient, EleroApiError, EleroHubData, EleroNativeApiClient
from .const import DEFAULT_SCAN_INTERVAL, DOMAIN

_LOGGER = logging.getLogger(__name__)


class EleroDataUpdateCoordinator(DataUpdateCoordinator[EleroHubData]):
    """Fetch and cache Elero hub data."""

    def __init__(self, hass: HomeAssistant, api: EleroApiClient, node_name: str | None = None) -> None:
        super().__init__(
            hass,
            _LOGGER,
            name=DOMAIN,
            update_interval=DEFAULT_SCAN_INTERVAL,
        )
        self.api = api
        self.native_api = EleroNativeApiClient(hass, node_name) if node_name else None

    async def _async_update_data(self) -> EleroHubData:
        try:
            return await self.api.async_fetch_data()
        except EleroApiError as err:
            raise UpdateFailed(str(err)) from err
