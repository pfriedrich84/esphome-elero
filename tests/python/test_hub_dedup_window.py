"""Tests for hub dedup_window validation semantics."""

import pytest
from conftest import make_time_period
from esphome.config_validation import Invalid

from components.elero import CONF_DEDUP_WINDOW, _validate_dedup_window


def _config(window_ms):
    return {CONF_DEDUP_WINDOW: make_time_period(window_ms)}


def test_dedup_window_disabled_zero_allowed():
    result = _validate_dedup_window(_config(0))
    assert result is not None


def test_dedup_window_opt_in_lower_bound_allowed():
    result = _validate_dedup_window(_config(100))
    assert result is not None


def test_dedup_window_rejects_nonzero_below_lower_bound():
    with pytest.raises(Invalid, match=r"0ms \(disabled\) or between 100ms and 60s"):
        _validate_dedup_window(_config(99))


def test_dedup_window_rejects_above_upper_bound():
    with pytest.raises(Invalid, match=r"0ms \(disabled\) or between 100ms and 60s"):
        _validate_dedup_window(_config(60001))
