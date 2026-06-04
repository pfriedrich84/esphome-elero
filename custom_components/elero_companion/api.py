"""Async client for the ESPHome Elero web API."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any
from urllib.parse import urlparse

import aiohttp

from .const import DEFAULT_TIMEOUT


class EleroApiError(Exception):
    """Base API error."""


class EleroCannotConnect(EleroApiError):
    """Raised when the Elero hub cannot be reached."""


class EleroAuthError(EleroApiError):
    """Raised when HTTP Basic Auth fails."""


class EleroWebUiDisabled(EleroApiError):
    """Raised when the Elero web UI/API is disabled."""


class EleroInvalidResponse(EleroApiError):
    """Raised when the hub returns an unexpected response."""


@dataclass(slots=True)
class EleroHubData:
    """Combined data fetched from an Elero hub."""

    info: dict[str, Any]
    status: dict[str, Any]
    runtime: dict[str, Any]
    ui_status: dict[str, Any]
    frequency: dict[str, Any]
    yaml_export: str | None = None

    @property
    def device_name(self) -> str:
        """Return the hub's display name."""
        return str(self.info.get("device_name") or "Elero hub")

    @property
    def configured_covers(self) -> int:
        """Return configured cover count."""
        value = self.info.get("configured_covers")
        if value is not None:
            return int(value)
        return len(self.status.get("covers", []))

    @property
    def configured_lights(self) -> int:
        """Return configured light count."""
        value = self.info.get("configured_lights")
        if value is not None:
            return int(value)
        return len(self.status.get("lights", []))

    @property
    def runtime_covers(self) -> int:
        """Return runtime adopted cover count."""
        return self._runtime_device_count("cover")

    @property
    def runtime_lights(self) -> int:
        """Return runtime adopted light count."""
        return self._runtime_device_count("light")

    def _runtime_device_count(self, device_type: str) -> int:
        """Return runtime adopted device count for a type."""
        blinds = self.runtime.get("blinds", [])
        if not isinstance(blinds, list):
            return 0
        return len([item for item in blinds if item.get("device_type") == device_type])

    @property
    def diagnostics(self) -> dict[str, Any]:
        """Return diagnostic counters from status."""
        diagnostics = self.status.get("diagnostics", {})
        return diagnostics if isinstance(diagnostics, dict) else {}

    @property
    def web_ui_enabled(self) -> bool | None:
        """Return whether the Elero web UI/API reports enabled."""
        enabled = self.ui_status.get("enabled")
        return enabled if isinstance(enabled, bool) else None


def normalize_base_url(value: str) -> str:
    """Normalize user input into an origin URL without a path.

    Users often paste the web UI URL (``http://host/elero``) or a concrete
    endpoint (``http://host/elero/api/info``). The client appends API paths
    itself, so keep only scheme and network location.
    """
    raw = value.strip()
    if not raw:
        raise ValueError("empty host")
    if "://" not in raw:
        raw = f"http://{raw}"
    parsed = urlparse(raw)
    if parsed.scheme not in {"http", "https"} or not parsed.netloc:
        raise ValueError("invalid URL")
    return f"{parsed.scheme}://{parsed.netloc}"


class EleroApiClient:
    """Small async client for the Elero hub web API."""

    def __init__(
        self,
        session: aiohttp.ClientSession,
        base_url: str,
        username: str | None = None,
        password: str | None = None,
        timeout: int = DEFAULT_TIMEOUT,
    ) -> None:
        self.session = session
        self.base_url = normalize_base_url(base_url)
        self.auth = aiohttp.BasicAuth(username, password or "") if username else None
        self.timeout = aiohttp.ClientTimeout(total=timeout)

    async def async_get_json(self, path: str) -> dict[str, Any]:
        """GET a JSON endpoint."""
        data = await self._async_request("GET", path)
        if not isinstance(data, dict):
            raise EleroInvalidResponse(f"Expected JSON object from {path}")
        return data

    async def async_get_text(self, path: str) -> str:
        """GET a text endpoint."""
        data = await self._async_request("GET", path, json_response=False)
        if not isinstance(data, str):
            raise EleroInvalidResponse(f"Expected text from {path}")
        return data

    async def _async_request(self, method: str, path: str, *, json_response: bool = True) -> Any:
        url = f"{self.base_url}{path}"
        try:
            async with self.session.request(method, url, auth=self.auth, timeout=self.timeout) as response:
                if response.status in {401, 403}:
                    raise EleroAuthError("Authentication failed")
                if response.status == 503:
                    raise EleroWebUiDisabled("Elero web UI/API is disabled")
                if response.status >= 400:
                    raise EleroCannotConnect(f"HTTP {response.status} from {path}")
                if json_response:
                    return await response.json(content_type=None)
                return await response.text()
        except EleroApiError:
            raise
        except (aiohttp.ClientError, TimeoutError) as err:
            raise EleroCannotConnect(str(err)) from err
        except ValueError as err:
            raise EleroInvalidResponse(str(err)) from err

    async def async_validate(self) -> dict[str, Any]:
        """Validate connectivity and return hub info."""
        info = await self.async_get_json("/elero/api/info")
        if "device_name" not in info:
            raise EleroInvalidResponse("Missing device_name in /elero/api/info")
        return info

    async def async_fetch_data(self) -> EleroHubData:
        """Fetch the companion MVP data set."""
        info = await self.async_get_json("/elero/api/info")
        status = await self.async_get_json("/elero/api/status")

        try:
            runtime = await self.async_get_json("/elero/api/runtime")
        except EleroApiError:
            runtime = {}

        try:
            ui_status = await self.async_get_json("/elero/api/ui/status")
        except EleroApiError:
            ui_status = {}

        try:
            frequency = await self.async_get_json("/elero/api/frequency")
        except EleroApiError:
            frequency = {}

        try:
            yaml_export = await self.async_get_text("/elero/api/yaml")
        except EleroApiError:
            yaml_export = None

        return EleroHubData(
            info=info,
            status=status,
            runtime=runtime,
            ui_status=ui_status,
            frequency=frequency,
            yaml_export=yaml_export,
        )
