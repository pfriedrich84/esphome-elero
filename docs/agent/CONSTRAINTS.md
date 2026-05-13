# Agent Constraints

Hard constraints for work in `esphome-elero`. These constraints override generic best practices.

## Product and compatibility

- The repository must remain an ESPHome external component consumable via `external_components` from GitHub.
- Preserve ESPHome/Home Assistant entity semantics for covers, lights, sensors, text sensors, buttons, switches, and group covers.
- YAML-facing changes require documentation and compile-fixture coverage where practical.
- Keep user examples free of real Wi-Fi, OTA, API, Home Assistant, or RF capture secrets.

## Hardware and runtime

- Target hardware is ESP32/ESP32-S3 with one CC1101 radio on SPI.
- After setup, CC1101 SPI access must stay on the radio task/Core 0; do not introduce cross-core SPI access.
- Real RF behavior depends on hardware and user environment. Do not claim hardware validation unless it was actually performed.
- Avoid unsafe ESP32 strapping-pin guidance; wiring docs should call out boot/SPI risks where relevant.

## RF/protocol safety

- Do not weaken packet length/bounds validation, destination count checks, CRC/LQI handling, stale-counter handling, deduplication intent, or queue overflow protection.
- Command counters must not be advanced/reused in ways that can create stale or duplicated command intents after failed transmission.
- Native group RF packets are only valid when member command profiles are compatible; otherwise group covers must fall back safely.

## Web UI and API

- Do not hand-edit `components/elero_web/elero_web_ui.h`; rebuild it from `components/elero_web/frontend/`.
- Preserve HTTP Basic Auth behavior, disabled-UI HTTP 503 behavior, JSON escaping, and same-origin/CORS restrictions.

## Tooling constraints

- Python validation depends on ESPHome being installed; local environments may lack it even when CI has it.
- Prefer dependency-free helper scripts for repository-local validation unless a dependency already exists.
