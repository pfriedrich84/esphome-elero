# Native API Companion Decision

Issue: [#179](https://github.com/pfriedrich84/esphome-elero/issues/179)

This document records the updated Home Assistant Companion direction for `esphome-elero`.

## Decision

The long-term Companion path is **ESPHome Native API only**.

The existing ESP-hosted `/elero` web UI and `/elero/api/*` REST endpoints may stay temporarily as a migration bridge, but they are not the target architecture.

`esphome-elero` supports two official operating modes:

1. **YAML-only mode**: users configure all covers/lights in ESPHome YAML. This mode must always work without HACS, without the Companion, without `/elero`, and without REST endpoints.
2. **Companion-managed mode**: users configure Elero devices in the Companion, then manually push a complete managed configuration to the ESP32 through ESPHome Native API. The ESP32 stores the last accepted managed configuration locally and uses it for RF execution and entity materialization.

YAML-only remains the baseline and recovery path. Companion-managed mode is an additional convenience mode, not a prerequisite for the component to work.

## Ownership model

```text
YAML-only mode
  ESPHome YAML owns durable user configuration and standard cover/light entities.

Companion-managed mode
  Companion owns the user-facing configuration UI and draft editing.
  User manually pushes the complete registry to the ESP32.
  ESP32 owns the last accepted managed device registry, RF execution,
  and the managed cover/light entities it exposes through ESPHome Native API.

ESPHome Elero component
  Owns CC1101 access, RF correctness, packet parsing, counters, queues,
  retries, polling, status dispatch, diagnostics, and managed/YAML RF execution.

Home Assistant ESPHome integration
  Owns entity exposure and standard command handling for YAML-backed entities
  and, if feasible, ESP32-materialized managed entities.

Elero Companion
  Owns setup assistance, discovery views, diagnostics, Repairs,
  managed-device configuration, manual push-to-ESP, YAML export,
  and managed registry status.

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
- Reboot-safe persistent YAML configuration.

A Companion-managed implementation must not break or weaken this mode.

## Companion-managed mode

Companion-managed mode allows users to configure covers/lights in Home Assistant instead of editing each device into YAML.

The minimal YAML contains only the hub and an explicit managed-mode opt-in, for example:

```yaml
api:
  encryption:
    key: !secret api_key

elero:
  cs_pin: GPIO12
  gdo0_pin: GPIO03

elero_managed:
  enabled: true
  max_devices: 32
```

In this mode:

- The Companion stores editable drafts in Home Assistant.
- The user must explicitly press a manual **Push to ESP32** action before the ESP32 configuration changes.
- The Companion sends the complete managed-device registry to the ESP32 through ESPHome Native API.
- The ESP32 validates the registry, accepts or rejects it atomically, persists the last accepted registry locally, and reports the active revision.
- The ESP32 uses the managed registry for RF execution, counters, polling, retries, queueing, status dispatch, reboot recovery, and managed entity materialization.
- The preferred target is that managed covers/lights are exposed by the ESP32 as normal ESPHome Native API cover/light entities, so Home Assistant commands go directly through the normal ESPHome integration.
- The normal ESPHome integration continues to expose YAML-backed entities when YAML-only mode is used.

The ESP32 should not fetch configuration from Home Assistant automatically on boot. It should boot with the last accepted local managed registry and later accept a new registry only after a manual Companion push.

## Direct command path target

The target command path for Companion-managed devices is direct:

```text
Home Assistant cover/light service
  -> Home Assistant ESPHome integration
    -> ESPHome Native API
      -> ESP32 managed cover/light entity
        -> Elero RF queue/counter/retry logic
          -> CC1101 / Elero RF
```

The Companion should not be in the normal command path after a managed registry has been accepted by the ESP32.

Implementation note: managed entities should preferably be materialized on the ESP32 from the last accepted registry at boot. If hot-adding or removing Native API entities after a push is not reliable, the push flow may require an ESP restart/reconnect so Home Assistant receives a fresh entity list from the ESPHome integration.

Fallback note: a thin Companion command adapter may be considered only if direct ESPHome Native API entity materialization proves infeasible, but it is not the preferred target architecture.

## Managed registry requirements

The managed registry should include at least:

- `schema_version`
- `registry_revision`
- `hub_id`
- `devices[]`
- `checksum` or equivalent integrity check

Each device entry should include:

- device type: `cover` or `light`
- display name
- blind/device address
- remote address
- channel
- protocol fields such as payload, packet info, hop, and command bytes where needed
- capabilities such as tilt or dimming
- timing values such as open/close duration or dim duration
- poll interval
- disabled/enabled state

Registry updates must be validated before acceptance. Invalid updates must leave the previous accepted registry active.

## Companion Native API helper surface

The Companion should use explicit ESPHome Native API helper actions/responses instead of the legacy REST API.

Candidate helpers:

| Helper | Purpose |
|---|---|
| `get_elero_info` | Hub metadata, component version, configured/runtime/managed counts. |
| `get_elero_status` | Combined status, scan state, radio health, diagnostics. |
| `get_elero_configured` | YAML-backed configured covers/lights. |
| `get_elero_managed_registry` | Active managed registry revision and device inventory. |
| `validate_elero_managed_registry` | Validate a candidate registry without activating it. |
| `push_elero_managed_registry` | Manually push a complete managed registry to the ESP32. |
| `clear_elero_managed_registry` | Remove managed registry after explicit confirmation. |
| `get_elero_runtime` | Runtime adopted devices. |
| `get_elero_discovered` | Discovery candidates. |
| `start_elero_scan` | Start RF discovery. |
| `stop_elero_scan` | Stop RF discovery. |
| `adopt_elero_device` | Temporarily adopt a discovered device for inspection or managed/YAML conversion. |
| `remove_elero_runtime` | Remove a runtime adopted device. |
| `get_elero_yaml` | Generate YAML export/snippet data for recovery or YAML-only migration. |
| `get_elero_frequency` | Frequency and mismatch diagnostics. |
| `set_elero_frequency` | Optional guarded frequency tuning. |
| `get_elero_packets` | Packet dump metadata/support data. |
| `clear_elero_packets` | Clear packet dump buffer. |
| `get_elero_logs` | Support log data, if implemented. |
| `clear_elero_logs` | Clear support log buffer, if implemented. |
| `reset_elero_diagnostics` | Reset monotonic diagnostics, if implemented. |

Native API responses should use stable field names and include a schema or component version so the Companion can detect incompatible firmware.

## Companion user process

Target flow for Companion-managed mode:

1. User creates and flashes a minimal ESPHome YAML configuration with `api:`, `elero:`, and `elero_managed:` enabled.
2. Home Assistant discovers or connects to the ESPHome device through the normal ESPHome integration.
3. User installs the Elero Companion through HACS.
4. Companion Config Flow links to the existing ESPHome Elero hub and validates `get_elero_info` plus managed-registry support.
5. Companion creates one hub-level device and diagnostics.
6. User starts discovery from the Companion.
7. The ESP firmware performs the RF scan. The user presses the physical Elero remote.
8. Companion displays discovered candidates with address, remote address, channel, RSSI, last state, and duplicate status.
9. User configures the device directly in the Companion: name, type, travel time, tilt/dimming, poll interval, groups, and enabled state.
10. Companion stores the candidate in its editable managed configuration draft.
11. User presses **Push to ESP32**.
12. Companion sends the complete managed registry to the ESP32.
13. ESP32 validates and persists the registry atomically.
14. ESP32 materializes the accepted managed devices as ESPHome Native API cover/light entities, preferably after restart/reconnect if required.
15. Home Assistant receives those entities through the normal ESPHome integration.
16. Commands from Home Assistant go directly to the ESP32 managed entities through the normal ESPHome Native API path.

Target flow for YAML-only mode:

1. User configures covers/lights directly in ESPHome YAML.
2. User flashes the ESPHome device.
3. Home Assistant receives the entities through the normal ESPHome integration.
4. Companion is optional and may still show diagnostics or YAML export, but it is not required.

## Manual push rule

Companion-managed configuration changes must not silently alter the active ESP32 RF configuration.

Required push semantics:

- Editing in the Companion creates a draft only.
- The user explicitly presses **Push to ESP32**.
- Companion shows what will change before pushing.
- ESP32 validates the complete registry before accepting it.
- ESP32 either accepts the entire registry or keeps the previous registry unchanged.
- Companion shows the active ESP32 registry revision and whether local Companion edits are pending.
- If entity-list changes require restart/reconnect, the Companion must state that clearly and guide the user through it.

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

## Guardrails

- YAML-only mode must always remain supported.
- Companion-managed mode must be explicit opt-in in YAML and in Companion UI.
- Managed registry updates require a manual push action.
- Companion-managed entities must not duplicate YAML-backed ESPHome entities without warning.
- RF packet encoding must remain in ESP firmware, not Python.
- Normal managed-device commands should go directly through ESPHome Native API entities after registry acceptance.
- Diagnostics must avoid exposing private environment data.
- Users should be able to export managed configuration as YAML for recovery or migration to YAML-only mode.

## Implementation order

1. Preserve YAML-only operation as the permanent baseline.
2. Add a minimal Native API helper spike, starting with `get_elero_info`.
3. Add managed-mode YAML opt-in and a minimal managed registry model.
4. Add `validate_elero_managed_registry` and `push_elero_managed_registry` helpers.
5. Spike ESP32 materialization of managed registry entries as ESPHome Native API cover/light entities.
6. Refactor the Companion skeleton toward Native API first.
7. Add Companion-managed draft editing and manual **Push to ESP32**.
8. Add managed entity refresh/reconnect/restart handling if required after push.
9. Keep legacy `/elero` REST only as temporary fallback during migration.
10. Deprecate and later remove `/elero` web UI and REST after Native API parity exists.
