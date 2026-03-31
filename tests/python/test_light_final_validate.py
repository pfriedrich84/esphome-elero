"""Tests for light _final_validate — duplicate address detection (within lights + cross-platform)."""

import pytest
from unittest.mock import patch
from conftest import make_cover_config, make_light_config
from esphome.config_validation import Invalid

from components.elero.light import _final_validate


def test_single_light_passes():
    with patch("components.elero.light.CORE") as mock_core:
        mock_core.config = {
            "light": [make_light_config(blind_address=0x111111, name="Light1")]
        }
        result = _final_validate(make_light_config())
        assert result is not None


def test_two_lights_different_addresses_pass():
    with patch("components.elero.light.CORE") as mock_core:
        mock_core.config = {
            "light": [
                make_light_config(blind_address=0x111111, name="Light1"),
                make_light_config(blind_address=0x222222, name="Light2"),
            ]
        }
        result = _final_validate(make_light_config())
        assert result is not None


def test_duplicate_light_address_raises():
    with patch("components.elero.light.CORE") as mock_core:
        mock_core.config = {
            "light": [
                make_light_config(blind_address=0x111111, name="Light1"),
                make_light_config(blind_address=0x111111, name="Light2"),
            ]
        }
        with pytest.raises(Invalid, match="Duplicate blind_address"):
            _final_validate(make_light_config())


def test_cross_platform_duplicate_raises():
    with patch("components.elero.light.CORE") as mock_core:
        mock_core.config = {
            "light": [make_light_config(blind_address=0x111111, name="MyLight")],
            "cover": [make_cover_config(blind_address=0x111111, name="MyCover")],
        }
        with pytest.raises(Invalid, match="light.*cover|cover.*light"):
            _final_validate(make_light_config())


def test_cross_platform_different_addresses_pass():
    with patch("components.elero.light.CORE") as mock_core:
        mock_core.config = {
            "light": [make_light_config(blind_address=0x111111, name="MyLight")],
            "cover": [make_cover_config(blind_address=0x222222, name="MyCover")],
        }
        result = _final_validate(make_light_config())
        assert result is not None


def test_core_config_none_passes():
    with patch("components.elero.light.CORE") as mock_core:
        mock_core.config = None
        result = _final_validate(make_light_config())
        assert result is not None


def test_no_light_section_passes():
    with patch("components.elero.light.CORE") as mock_core:
        mock_core.config = {}
        result = _final_validate(make_light_config())
        assert result is not None


def test_non_elero_lights_ignored():
    with patch("components.elero.light.CORE") as mock_core:
        mock_core.config = {
            "light": [
                {"platform": "rgb", "blind_address": 0x111111, "name": "RGB"},
                make_light_config(blind_address=0x111111, name="Elero"),
            ]
        }
        result = _final_validate(make_light_config())
        assert result is not None


def test_non_elero_covers_ignored_in_cross_check():
    with patch("components.elero.light.CORE") as mock_core:
        mock_core.config = {
            "light": [make_light_config(blind_address=0x111111, name="MyLight")],
            "cover": [{"platform": "template", "blind_address": 0x111111, "name": "Other"}],
        }
        result = _final_validate(make_light_config())
        assert result is not None


def test_light_without_address_skipped():
    with patch("components.elero.light.CORE") as mock_core:
        mock_core.config = {
            "light": [
                {"platform": "elero", "name": "NoAddr"},
                make_light_config(blind_address=0x111111, name="HasAddr"),
            ]
        }
        result = _final_validate(make_light_config())
        assert result is not None


def test_no_cover_section_passes():
    with patch("components.elero.light.CORE") as mock_core:
        mock_core.config = {
            "light": [make_light_config(blind_address=0x111111, name="Light1")]
        }
        result = _final_validate(make_light_config())
        assert result is not None


def test_cross_platform_error_mentions_both():
    with patch("components.elero.light.CORE") as mock_core:
        mock_core.config = {
            "light": [make_light_config(blind_address=0xABCDEF, name="LightX")],
            "cover": [make_cover_config(blind_address=0xABCDEF, name="CoverY")],
        }
        with pytest.raises(Invalid) as exc_info:
            _final_validate(make_light_config())
        msg = str(exc_info.value)
        assert "LightX" in msg
        assert "CoverY" in msg


def test_multiple_lights_and_covers_all_unique_pass():
    with patch("components.elero.light.CORE") as mock_core:
        mock_core.config = {
            "light": [
                make_light_config(blind_address=0x111111, name="L1"),
                make_light_config(blind_address=0x222222, name="L2"),
                make_light_config(blind_address=0x333333, name="L3"),
            ],
            "cover": [
                make_cover_config(blind_address=0x444444, name="C1"),
                make_cover_config(blind_address=0x555555, name="C2"),
            ],
        }
        result = _final_validate(make_light_config())
        assert result is not None
