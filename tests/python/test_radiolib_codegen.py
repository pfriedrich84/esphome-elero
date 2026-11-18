"""RadioLib dependency and build-flag contract for both firmware frameworks."""

import asyncio
from unittest.mock import AsyncMock, Mock

from conftest import make_time_period

from components import elero


def test_radiolib_version_and_cc1101_build_flags(monkeypatch):
    libraries = []
    flags = []
    monkeypatch.setattr(elero.cg, "new_Pvariable", Mock())
    monkeypatch.setattr(elero.cg, "register_component", AsyncMock())
    monkeypatch.setattr(elero.spi, "register_spi_device", AsyncMock())
    monkeypatch.setattr(elero.cg, "gpio_pin_expression", AsyncMock())
    monkeypatch.setattr(elero.cg, "add", Mock())
    monkeypatch.setattr(elero.cg, "add_library", lambda *args: libraries.append(args))
    monkeypatch.setattr(elero.cg, "add_build_flag", flags.append)
    # Listener allocation is unrelated to this dependency contract.
    from esphome.components import logger

    monkeypatch.setattr(logger, "request_log_listener", Mock())

    asyncio.run(
        elero.to_code(
            {
                "id": "test_hub",
                "gdo0_pin": {},
                "freq0": 0x7A,
                "freq1": 0x71,
                "freq2": 0x21,
                "send_repeats": 1,
                "send_delay": make_time_period(0),
                "dedup_window": make_time_period(0),
            }
        )
    )

    assert ("jgromes/RadioLib", "8.0.0") in libraries
    assert ("SPI", None) in libraries  # Arduino framework SPI include path
    assert "-DRADIOLIB_EXCLUDE_CC1101=1" not in flags
    assert "-DRADIOLIB_EXCLUDE_LR2021=1" in flags
    assert "-DRADIOLIB_EXCLUDE_ADSB=1" in flags
    assert "-DRADIOLIB_EXCLUDE_SSDV=1" in flags
    assert "-DRADIOLIB_EXCLUDE_LORAWAN=1" in flags
    assert "-DRADIOLIB_EXCLUDE_SX126X=1" in flags
    assert "-DRADIOLIB_EXCLUDE_SX127X=1" in flags
