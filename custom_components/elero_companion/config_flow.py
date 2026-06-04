"""Config flow for Elero Companion."""

from __future__ import annotations

from typing import Any

import voluptuous as vol
from homeassistant import config_entries
from homeassistant.const import CONF_PASSWORD, CONF_USERNAME
from homeassistant.helpers.aiohttp_client import async_get_clientsession

from .api import (
    EleroApiClient,
    EleroAuthError,
    EleroCannotConnect,
    EleroInvalidResponse,
    EleroWebUiDisabled,
    normalize_base_url,
)
from .const import CONF_URL, DOMAIN


STEP_USER_DATA_SCHEMA = vol.Schema(
    {
        vol.Required(CONF_URL): str,
        vol.Optional(CONF_USERNAME): str,
        vol.Optional(CONF_PASSWORD): str,
    }
)


class EleroCompanionConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    """Handle an Elero Companion config flow."""

    VERSION = 1

    async def async_step_user(self, user_input: dict[str, Any] | None = None):
        """Handle the initial step."""
        errors: dict[str, str] = {}

        if user_input is not None:
            try:
                base_url = normalize_base_url(user_input[CONF_URL])
                info = await self._async_validate_input(base_url, user_input)
            except ValueError:
                errors[CONF_URL] = "invalid_url"
            except EleroAuthError:
                errors["base"] = "invalid_auth"
            except EleroWebUiDisabled:
                errors["base"] = "web_ui_disabled"
            except EleroCannotConnect:
                errors["base"] = "cannot_connect"
            except EleroInvalidResponse:
                errors["base"] = "invalid_response"
            else:
                await self.async_set_unique_id(base_url)
                self._abort_if_unique_id_configured(updates={CONF_URL: base_url})
                data = {
                    CONF_URL: base_url,
                    CONF_USERNAME: user_input.get(CONF_USERNAME, ""),
                    CONF_PASSWORD: user_input.get(CONF_PASSWORD, ""),
                }
                return self.async_create_entry(
                    title=str(info.get("device_name") or "Elero hub"),
                    data=data,
                )

        return self.async_show_form(
            step_id="user",
            data_schema=STEP_USER_DATA_SCHEMA,
            errors=errors,
        )

    async def _async_validate_input(self, base_url: str, user_input: dict[str, Any]) -> dict[str, Any]:
        api = EleroApiClient(
            async_get_clientsession(self.hass),
            base_url,
            user_input.get(CONF_USERNAME),
            user_input.get(CONF_PASSWORD),
        )
        return await api.async_validate()
