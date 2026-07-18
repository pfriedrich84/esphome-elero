"""Schema and codegen coverage for elero_group hide_members."""

import asyncio
import importlib
import sys
from unittest.mock import AsyncMock, Mock, call, patch

import pytest
import esphome.config_validation as cv

from components import elero

sys.modules.setdefault("esphome.components.elero", elero)
elero_group = importlib.import_module("components.elero_group")


def test_group_member_bounds():
    with pytest.raises(cv.Invalid, match="at least 2"):
        elero_group._validate_members([{"members": ["one"]}])
    with pytest.raises(cv.Invalid, match="more than 10"):
        elero_group._validate_members([{"members": list(range(11))}])
    with pytest.raises(cv.Invalid, match="same member"):
        elero_group._validate_members([{"members": ["one", "one"]}])
    config = [{"members": ["one", "two"]}]
    assert elero_group._validate_members(config) is config


def test_hide_members_codegen_marks_every_member_internal():
    group = Mock()
    parent = Mock()
    members = [Mock(), Mock()]
    config = [
        {
            "elero_id": "hub",
            "assumed_state": True,
            "hide_members": True,
            "members": ["left", "right"],
        }
    ]

    with (
        patch.object(elero_group.cover, "new_cover", AsyncMock(return_value=group)),
        patch.object(elero_group.cg, "register_component", AsyncMock()),
        patch.object(
            elero_group.cg,
            "get_variable",
            AsyncMock(side_effect=[parent, *members]),
        ),
        patch.object(elero_group.cg, "add") as add,
    ):
        asyncio.run(elero_group.to_code(config))

    group.set_hide_members.assert_called_once_with(True)
    for member in members:
        member.set_internal.assert_called_once_with(True)
        assert call(member.set_internal.return_value) in add.call_args_list


def test_visible_members_codegen_does_not_mark_internal():
    group = Mock()
    parent = Mock()
    members = [Mock(), Mock()]
    config = [
        {
            "elero_id": "hub",
            "assumed_state": True,
            "hide_members": False,
            "members": ["left", "right"],
        }
    ]

    with (
        patch.object(elero_group.cover, "new_cover", AsyncMock(return_value=group)),
        patch.object(elero_group.cg, "register_component", AsyncMock()),
        patch.object(
            elero_group.cg,
            "get_variable",
            AsyncMock(side_effect=[parent, *members]),
        ),
        patch.object(elero_group.cg, "add"),
    ):
        asyncio.run(elero_group.to_code(config))

    group.set_hide_members.assert_called_once_with(False)
    for member in members:
        member.set_internal.assert_not_called()
