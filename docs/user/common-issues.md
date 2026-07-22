# Common Issues

This page collects common setup symptoms and the first checks to run before
changing Elero protocol values.

## `SPI Status: FAILED — CC1101 communication broken`

This means the ESP32 cannot communicate with the CC1101 over SPI. RF frequency,
blind addresses, channels, and Elero command settings are not involved yet.

### Lilygo T-Embed CC1101

If you use a Lilygo T-Embed CC1101, see the board-specific notes:
[`boards/lilygo-t-embed-cc1101.md`](boards/lilygo-t-embed-cc1101.md).

Common causes on this board are:

- the onboard CC1101 power rail is not enabled
- the antenna/RF switch pins are not initialized
- the wrong ESP32-S3 board variant or pin mapping is used

Check the Lilygo board-power and antenna-switch setup before changing SPI pins.

### External CC1101 module

For a separate CC1101 module, check:

- VCC is connected to 3.3V, not 5V
- GND is connected
- CLK, MOSI, MISO, and CS match the YAML configuration
- CS is not shared with another SPI device
- unsafe ESP32 strapping pins are avoided for SPI signals

On classic ESP32 boards, avoid GPIO0, GPIO2, GPIO5, GPIO12, and GPIO15 for SPI
signals. GPIO12 is especially risky on classic ESP32 because it controls
VDD_SDIO at boot. ESP32-S3 has different strapping pins; see the board-specific
notes for Lilygo T-Embed CC1101.

## No RF packets when pressing the remote

If SPI communication works but no packets appear in the log when pressing the
real Elero remote, check the CC1101 frequency settings.

Common 868 MHz variants:

| Variant | `freq2` | `freq1` | `freq0` |
|---|---:|---:|---:|
| Standard 868.35 MHz | `0x21` | `0x71` | `0x7a` |
| Alternative 868.95 MHz | `0x21` | `0x71` | `0xc0` |

Try the standard setting first, then the alternative. The frequency can also be
tested at runtime through the web API when `elero_web` is enabled.

## ESP32-S3 compile fails with `Killed signal terminated program cc1plus`

ESP32-S3 builds can run out of memory during parallel compilation. Serialize the
build:

```yaml
esphome:
  compile_process_limit: 1
```

## Weak signal or unreliable control

- Watch the RSSI diagnostic sensor.
- Values above about `-70 dBm` are typically good.
- Reposition the antenna and keep it away from noisy power supplies.
- Prefer an 868 MHz CC1101 module for European Elero installations.
- Check that the ESP32 and CC1101 have a stable 3.3V supply.
