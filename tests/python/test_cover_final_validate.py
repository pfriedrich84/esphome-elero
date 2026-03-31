"""Tests for cover _final_validate — duplicate blind_address detection."""

import pytest
from unittest.mock import patch
from conftest import make_cover_config
from esphome.config_validation import Invalid

from components.elero.cover import _final_validate


def test_single_cover_passes():
    with patch("components.elero.cover.CORE") as mock_core:
        mock_core.config = {
            "cover": [make_cover_config(blind_address=0x111111, name="Cover1")]
        }
        result = _final_validate(make_cover_config())
        assert result is not None


def test_two_covers_different_addresses_pass():
    with patch("components.elero.cover.CORE") as mock_core:
        mock_core.config = {
            "cover": [
                make_cover_config(blind_address=0x111111, name="Cover1"),
                make_cover_config(blind_address=0x222222, name="Cover2"),
            ]
        }
        result = _final_validate(make_cover_config())
        assert result is not None


def test_duplicate_address_raises():
    with patch("components.elero.cover.CORE") as mock_core:
        mock_core.config = {
            "cover": [
                make_cover_config(blind_address=0x111111, name="Cover1"),
                make_cover_config(blind_address=0x111111, name="Cover2"),
            ]
        }
        with pytest.raises(Invalid, match="Duplicate blind_address"):
            _final_validate(make_cover_config())


def test_core_config_none_passes():
    with patch("components.elero.cover.CORE") as mock_core:
        mock_core.config = None
        result = _final_validate(make_cover_config())
        assert result is not None


def test_no_cover_section_passes():
    with patch("components.elero.cover.CORE") as mock_core:
        mock_core.config = {}
        result = _final_validate(make_cover_config())
        assert result is not None


def test_non_elero_covers_ignored():
    with patch("components.elero.cover.CORE") as mock_core:
        mock_core.config = {
            "cover": [
                {"platform": "template", "blind_address": 0x111111, "name": "Other"},
                make_cover_config(blind_address=0x111111, name="Elero1"),
            ]
        }
        result = _final_validate(make_cover_config())
        assert result is not None


def test_cover_without_address_skipped():
    with patch("components.elero.cover.CORE") as mock_core:
        mock_core.config = {
            "cover": [
                {"platform": "elero", "name": "NoAddr"},
                make_cover_config(blind_address=0x111111, name="HasAddr"),
            ]
        }
        result = _final_validate(make_cover_config())
        assert result is not None


def test_error_contains_hex_address():
    with patch("components.elero.cover.CORE") as mock_core:
        mock_core.config = {
            "cover": [
                make_cover_config(blind_address=0xABCDEF, name="First"),
                make_cover_config(blind_address=0xABCDEF, name="Second"),
            ]
        }
        with pytest.raises(Invalid, match="0xabcdef"):
            _final_validate(make_cover_config())


def test_error_contains_both_names():
    with patch("components.elero.cover.CORE") as mock_core:
        mock_core.config = {
            "cover": [
                make_cover_config(blind_address=0x123456, name="Alpha"),
                make_cover_config(blind_address=0x123456, name="Beta"),
            ]
        }
        with pytest.raises(Invalid, match="Alpha") as exc_info:
            _final_validate(make_cover_config())
        assert "Beta" in str(exc_info.value)
