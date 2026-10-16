"""The optional feedback mode must reach the existing light output via codegen."""

import asyncio
from unittest.mock import AsyncMock, Mock, patch

import pytest

from components.elero import light as elero_light


@pytest.mark.parametrize("assumed", [False, True])
def test_assumed_state_codegen(assumed):
    output = Mock()
    config = {
        "output_id": "light_output", "elero_id": "hub",
        "blind_address": 0xE99B2B, "remote_address": 0x458130, "channel": 3,
        "dim_duration": 0, "assumed_state": assumed,
        "payload_1": 0, "payload_2": 3, "pck_inf1": 0x6A, "pck_inf2": 0x10,
        "hop": 0, "command_on": 0x31, "command_off": 0x52,
        "command_dim_up": 0x20, "command_dim_down": 0x40,
        "command_stop": 0x10, "command_check": 0,
    }
    with (
        patch.object(elero_light.cg, "new_Pvariable", return_value=output),
        patch.object(elero_light.cg, "register_component", AsyncMock()),
        patch.object(elero_light.light, "register_light", AsyncMock()),
        patch.object(elero_light.cg, "get_variable", AsyncMock(return_value=Mock())),
        patch.object(elero_light.cg, "add"),
    ):
        asyncio.run(elero_light.to_code(config))
    output.set_assumed_state.assert_called_once_with(assumed)
    output.set_command_on.assert_called_once_with(0x31)
    output.set_command_off.assert_called_once_with(0x52)
