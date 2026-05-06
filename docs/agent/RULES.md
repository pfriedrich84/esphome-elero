# Agent Rules

Core rules for coding agents working on `esphome-elero`.

## Product and RF safety

- Keep this repository an ESPHome external component consumable via `external_components` from GitHub.
- Preserve Home Assistant/ESPHome entity semantics for covers, lights, sensors, text sensors, buttons, switches, and group covers.
- Do not weaken RF packet validation, destination bounds checks, CRC/LQI handling, or queue overflow protection.
- Keep CC1101 SPI access on the radio task/Core 0 after setup; do not introduce cross-core SPI access.
- Preserve radio watchdog and recovery behavior unless a change has a clear reliability benefit and regression coverage.
- Do not edit generated `components/elero_web/elero_web_ui.h` by hand; rebuild it from `components/elero_web/frontend/`.

## Change discipline

- Prefer small, reviewable changes with focused tests.
- Update docs when YAML parameters, REST endpoints, entity behavior, wiring guidance, or web UI behavior changes.
- Keep pure RF/protocol/state logic dependency-light so unit tests can run without ESPHome hardware.
- Run relevant checks before finishing code changes; see [`CHECKS.md`](CHECKS.md).
- Do not expose secrets from ESPHome YAML, Wi-Fi credentials, OTA/API keys, or local device logs.

## Domain invariants

- An Elero hub owns one CC1101 radio and coordinates configured covers/lights plus runtime adopted blinds.
- A Blind or light command intent must become valid Elero RF packets without reusing stale counters after partial transmission failure.
- Native group RF packets are only valid when member command profiles are compatible; otherwise group cover logic must fall back safely.
- Runtime adopted blinds must share the same safety rules as configured blinds: bounded queues, sane polling, and deterministic position/counter behavior.
- Web UI POST/DELETE endpoints must preserve authentication, same-origin/CORS policy, JSON escaping, and disabled-UI behavior.
