"""Tests for elero_managed schema helpers."""

from esphome.config_validation import Invalid

from components.elero_managed import _validate_preallocated_slots


def test_preallocated_slots_within_max_devices_passes():
    config = {
        "max_devices": 4,
        "preallocated_cover_slots": 2,
        "preallocated_light_slots": 2,
    }

    assert _validate_preallocated_slots(config) is config


def test_preallocated_slots_default_to_zero_when_absent():
    config = {"max_devices": 1}

    assert _validate_preallocated_slots(config) is config


def test_preallocated_slots_cannot_exceed_max_devices():
    config = {
        "max_devices": 2,
        "preallocated_cover_slots": 2,
        "preallocated_light_slots": 1,
    }

    try:
        _validate_preallocated_slots(config)
    except Invalid as err:
        assert "preallocated_cover_slots + preallocated_light_slots" in str(err)
    else:
        raise AssertionError("Expected Invalid")
