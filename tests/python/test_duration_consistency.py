"""Tests for cover _validate_duration_consistency validator."""

import pytest
from conftest import make_time_period
from esphome.config_validation import Invalid

from components.elero.cover import _validate_duration_consistency


def _config(open_ms=None, close_ms=None):
    """Build a config dict with optional duration TimePeriod mocks."""
    c = {}
    c["open_duration"] = make_time_period(open_ms) if open_ms is not None else None
    c["close_duration"] = make_time_period(close_ms) if close_ms is not None else None
    return c


def test_both_zero_passes():
    result = _validate_duration_consistency(_config(0, 0))
    assert result is not None


def test_both_none_passes():
    result = _validate_duration_consistency(_config(None, None))
    assert result is not None


def test_both_nonzero_passes():
    result = _validate_duration_consistency(_config(25000, 22000))
    assert result is not None


def test_open_only_raises():
    with pytest.raises(Invalid, match="open_duration"):
        _validate_duration_consistency(_config(25000, 0))


def test_close_only_raises():
    with pytest.raises(Invalid, match="close_duration"):
        _validate_duration_consistency(_config(0, 22000))


def test_open_nonzero_close_none_raises():
    with pytest.raises(Invalid):
        _validate_duration_consistency(_config(5000, None))


def test_open_none_close_nonzero_raises():
    with pytest.raises(Invalid):
        _validate_duration_consistency(_config(None, 5000))


def test_equal_values_pass():
    result = _validate_duration_consistency(_config(10000, 10000))
    assert result is not None


def test_small_values_pass():
    result = _validate_duration_consistency(_config(1, 1))
    assert result is not None


def test_error_message_contains_values():
    with pytest.raises(Invalid, match="25000") as exc_info:
        _validate_duration_consistency(_config(25000, 0))
    assert "0" in str(exc_info.value)
