## 2026-04-01 — First Official Release

The first stable release of `esphome-elero`, a custom ESPHome external component for controlling Elero wireless motor blinds and lights via a CC1101 868 MHz RF transceiver on ESP32.

### Highlights

- **Dual-core RF architecture** — dedicated radio task on Core 0 for reliable TX/RX, ESPHome main loop on Core 1. FreeRTOS queues connect the two cores with zero SPI contention.
- **Cover platform** — open/close/stop/tilt commands, dead-reckoning position tracking via `open_duration`/`close_duration`, configurable polling, auto-stop at target position.
- **Light platform** — on/off and brightness control for Elero wireless dimmers with dead-reckoning brightness tracking.
- **Web UI** — built-in dashboard at `http://<device-ip>/elero` for RF blind discovery, runtime adoption, cover/light control, YAML config generation, and RF diagnostics.
- **RF discovery** — scan for nearby Elero blinds, view addresses and signal strength, adopt into runtime control without reflashing.
- **Position tracking** — dead-reckoning position estimation with configurable open/close durations, auto-stop at target, and stop-verify loop.
- **Priority TX queue** — stop commands bypass the normal queue for immediate delivery, critical for multi-cover group operations.
- **Radio health monitoring** — 5-second watchdog detects and recovers from RXFIFO overflow, stuck IDLE, and unexpected CC1101 states.
- **Optional HTTP Basic Auth** — protect the web UI with username/password.
- **ESP32-S3 support** — tested on ESP32 (Arduino) and ESP32-S3 (esp-idf), including shared SPI bus with TFT displays (e.g., LilyGo T-Embed CC1101).

### Supported Hardware

- **ESP32** or **ESP32-S3** (Arduino or esp-idf framework)
- **CC1101 868 MHz** RF transceiver via SPI
- **Elero motors** (rollers, shutters, awnings) and **Elero light receivers** (dimmers)

### CI & Quality

- 8-config compile matrix (minimal, full, multi-cover, light-only, ESP32-S3, custom frequency, etc.)
- C++ unit tests (GoogleTest) for crypto, utilities, and web helpers
- Python schema validation tests (pytest) for cover/light configuration
- Ruff linting for all Python code
- Auto-release with `yyyy-mm-xx` versioning on merge to main

### Installation

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/pfriedrich84/esphome-elero
      ref: 2026-04-01
```

See [README.md](https://github.com/pfriedrich84/esphome-elero/blob/main/README.md) for full setup instructions.
