"""Shared fixtures for esphome-elero Python tests."""

import sys
from pathlib import Path
from unittest.mock import Mock, patch

import pytest

# Add repo root to sys.path so we can import from components/
REPO_ROOT = Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))


@pytest.fixture
def mock_core_esp32():
    """Mock CORE for original ESP32 variant."""
    with patch("esphome.core.CORE") as mock:
        mock.data = {"esp32": {"variant": "ESP32"}}
        mock.config = None
        yield mock


@pytest.fixture
def mock_core_esp32s3():
    """Mock CORE for ESP32-S3 variant."""
    with patch("esphome.core.CORE") as mock:
        mock.data = {"esp32": {"variant": "ESP32S3"}}
        mock.config = None
        yield mock


def make_time_period(ms):
    """Create a mock TimePeriod object with .total_milliseconds."""
    tp = Mock()
    tp.total_milliseconds = ms
    return tp


def make_cover_config(**overrides):
    """Create a minimal elero cover config dict."""
    config = {
        "platform": "elero",
        "name": "Test Cover",
        "blind_address": 0xAA0001,
        "channel": 1,
        "remote_address": 0xBB0001,
    }
    config.update(overrides)
    return config


def make_light_config(**overrides):
    """Create a minimal elero light config dict."""
    config = {
        "platform": "elero",
        "name": "Test Light",
        "blind_address": 0xCC0001,
        "channel": 2,
        "remote_address": 0xBB0001,
    }
    config.update(overrides)
    return config
