# Native API Companion Decision

Issue: [#179](https://github.com/pfriedrich84/esphome-elero/issues/179)

This document records the updated Home Assistant Companion direction for `esphome-elero`.

## Decision

The long-term Companion path is **ESPHome Native API only**.

The existing ESP-hosted `/elero` web UI and `/elero/api/*` REST endpoints may stay temporarily as a migration bridge, but they are not the target architecture.

YAML-only operation is a permanent requirement. A user must always be able to run, operate, debug, and recover the Elero hub from ESPHome YAML without HACS, without the Companion, without `/elero`, and without REST endpoints.

## Ownership model

```text
ESPHome YAML
  Owns durable user configuration.

ESPHome Elero component
  Owns CC1101 access, RF correctness, packet parsing, counters, queues,
  retries, polling, status dispatch, diagnostics, and standard ESPHome
  cover/light entities.

Home Assistant ESPHome integration
  Owns normal entity exposure through the ESPHome Native API.

Elero Companion
  Owns user-facing setup assistance, discovery views, diagnostics, Repairs,
  YAML generation/export, and support workflows.

Legacy /elero web UI and REST
  Temporary migration bridge only.
```

## YAML-only baseline

The following must remain fully supported without the Companion:

- ESPHome external component installation.
- `elero:` hub configuration.
- YAML-configured `cover:` and `light:` entities.
- RF send/receive.
- Rolling counters, retry and queue behaviour.
- Polling and bidirectional status feedback.
- Diagnostic sensors and logs where practical.
- Reboot-safe persistent configuration.

The Companion may generate YAML snippets, but the source of truth is the YAML that is compiled and flashed to the ESPHome device.

## Companion Native API helper surface

The Companion should use explicit ESPHome Native API helper actions/responses instead of the legacy REST API.

Candidate helpers:

| Helper | Purpose |
|---|---|
| `get_elero_info` | Hub metadata, component version, configured/runtime counts. |
| `get_elero_status` | Combined status, scan state, radio health, diagnostics. |
| `get_elero_configured` | YAML-backed configured covers/lights. |
| `get_elero_runtime` | Runtime adopted devices. |
| `get_elero_discovered` | Discovery candidates. |
| `start_elero_scan` | Start RF discovery. |
| `stop_elero_scan` | Stop RF discovery. |
| `adopt_elero_device` | Temporarily adopt a discovered device for inspection/YAML generation. |
| `remove_elero_runtime` | Remove a runtime adopted device. |
| `get_elero_yaml` | Generate YAML export/snippet data. |
| `get_elero_frequency` | Frequency and mismatch diagnostics. |
| `set_elero_frequency` | Optional guarded frequency tuning. |
| `get_elero_packets` | Packet dump metadata/support data. |
| `clear_elero_packets` | Clear packet dump buffer. |
| `get_elero_logs` | Support log data, if implemented. |
| `clear_elero_logs` | Clear support log buffer, if implemented. |
| `reset_elero_diagnostics` | Reset monotonic diagnostics, if implemented. |

Native API responses should use stable field names and include a schema or component version so the Companion can detect incompatible firmware.

## Companion user process

Target flow:

1. User creates and flashes a normal ESPHome YAML configuration with `api:` enabled and `elero:` configured.
2. Home Assistant discovers or connects to the ESPHome device through the normal ESPHome integration.
3. User installs the Elero Companion through HACS.
4. Companion Config Flow links to an existing ESPHome Elero hub and validates `get_elero_info` through the Native API helper surface.
5. Companion creates one hub-level device and hub diagnostics. It does not create duplicate cover/light entities for YAML-configured devices.
6. User starts discovery from the Companion.
7. The ESP firmware performs the RF scan. The user presses the physical Elero remote.
8. Companion displays discovered candidates with address, remote address, channel, RSSI, last state, and whether they already exist as YAML-backed entities.
9. User assigns a name and options such as cover/light type, open/close duration, tilt support, poll interval, and optional group membership.
10. Companion generates YAML snippets.
11. User copies the YAML into the ESPHome YAML file and flashes the ESP device.
12. After reboot, Home Assistant receives the new cover/light through the normal ESPHome integration.
13. Companion detects the device as YAML-backed and stops treating it as an unresolved import candidate.

Runtime adoption is allowed only as a temporary inspection/import helper. A device is not persistent until it is represented in YAML and flashed.

## Legacy REST bridge mapping

During migration, existing REST endpoints map to Native API helpers as follows:

| Legacy REST endpoint | Native API helper |
|---|---|
| `GET /elero/api/info` | `get_elero_info` |
| `GET /elero/api/status` | `get_elero_status` |
| `GET /elero/api/configured` | `get_elero_configured` |
| `GET /elero/api/runtime` | `get_elero_runtime` |
| `GET /elero/api/discovered` | `get_elero_discovered` |
| `GET /elero/api/yaml` | `get_elero_yaml` |
| `GET /elero/api/frequency` | `get_elero_frequency` |
| `POST /elero/api/scan/start` | `start_elero_scan` |
| `POST /elero/api/scan/stop` | `stop_elero_scan` |
| `POST /elero/api/discovered/<addr>/adopt` | `adopt_elero_device` |
| `DELETE /elero/api/runtime/<addr>` | `remove_elero_runtime` |
| `GET /elero/api/packets` | `get_elero_packets` |
| `POST /elero/api/packets/clear` | `clear_elero_packets` |
| `GET /elero/api/logs` | `get_elero_logs` |
| `POST /elero/api/logs/clear` | `clear_elero_logs` |
| `POST /elero/api/diagnostics/reset` | `reset_elero_diagnostics` |

Do not add new user-facing workflows to the REST bridge unless they are also designed for Native API helpers.

## Companion guardrails

- The Companion MVP must not create per-blind cover/light entities by default.
- Standard control remains through ESPHome YAML-backed `cover` and `light` entities.
- The Companion must not implement RF packet encoding in Python.
- The Companion must not bypass ESP-side queue, counter, retry, and validation logic.
- Diagnostics must avoid exposing private environment data.
- Any future proxy entities must be opt-in and must warn about duplicates.

## Implementation order

1. Preserve YAML-only operation as the permanent baseline.
2. Add a minimal Native API helper spike, starting with `get_elero_info`.
3. Refactor the Companion skeleton toward Native API first.
4. Keep legacy `/elero` REST only as temporary fallback during migration.
5. Build Companion MVP around setup, diagnostics, Repairs, discovery, YAML generation/export, and import guidance.
6. Keep per-blind control entities out of the MVP.
7. Deprecate and later remove `/elero` web UI and REST after Native API parity exists.
