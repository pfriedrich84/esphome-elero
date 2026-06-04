# Companion-managed and YAML-only workflows

This document explains the intended user workflows once the Elero Companion moves to ESPHome Native API helpers.

## Two supported modes

`esphome-elero` supports two official operating modes.

### Mode A: YAML-only

YAML-only is the baseline and recovery mode.

A user can configure everything in ESPHome YAML, flash the device, and use the normal Home Assistant ESPHome integration. No Companion, HACS, `/elero` web UI, or REST endpoint is required.

### Mode B: Companion-managed

Companion-managed mode is a convenience mode.

A user configures Elero covers/lights in the Companion UI and then manually pushes the complete managed configuration to the ESP32. The ESP32 stores the last accepted registry locally and uses it after reboot.

## Permanent rule: YAML-only must always work

The Elero ESPHome component must remain fully usable without the Companion.

A user must always be able to:

- configure the hub in ESPHome YAML,
- configure covers and lights in ESPHome YAML,
- flash the ESPHome device,
- control covers and lights through the normal Home Assistant ESPHome integration,
- receive status feedback and diagnostics,
- recover the system from YAML and logs only.

Companion-managed mode must not break YAML-only mode.

## Target architecture

```text
YAML-only mode
  ESPHome YAML is the durable source of truth.
  Home Assistant receives normal ESPHome cover/light entities.

Companion-managed mode
  Companion is the configuration UI.
  Companion stores editable drafts in Home Assistant.
  User manually pushes the full registry to the ESP32.
  ESP32 stores the last accepted registry and performs RF execution.
  Companion creates the dynamic Home Assistant cover/light entities.

ESPHome Elero component
  RF correctness, CC1101 access, counters, queues, retries, diagnostics,
  polling, status dispatch, and command execution.
```

The existing `/elero` web UI and REST endpoints may remain temporarily, but the long-term target is Companion communication through ESPHome Native API helpers only.

## YAML-only setup flow

1. Create an ESPHome YAML file with `api:`, `elero:`, and YAML `cover:` / `light:` entries.
2. Flash the ESPHome device.
3. Add the ESPHome device to Home Assistant through the normal ESPHome integration.
4. Control the entities as standard Home Assistant covers/lights.

The Companion is optional in this mode.

## Companion-managed setup flow

1. Create a minimal ESPHome YAML file with `api:`, `elero:`, and explicit managed-mode opt-in.
2. Flash the ESPHome device.
3. Add the ESPHome device to Home Assistant through the normal ESPHome integration.
4. Install the Elero Companion through HACS.
5. Start the Elero Companion Config Flow.
6. Select or link the existing ESPHome Elero hub.
7. Companion validates that the hub exposes the expected Native API helper surface and managed-registry support.
8. Companion creates one hub-level device with diagnostics and helper actions.

Example minimal YAML shape:

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

## Companion-managed discovery and configuration flow

1. Open the Elero Companion integration in Home Assistant.
2. Start discovery from the Companion.
3. Press buttons on the physical Elero remote.
4. The ESP firmware performs the RF scan and reports discovered candidates.
5. Companion shows candidates with address, remote address, channel, RSSI, last state, and duplicate status.
6. Choose whether the candidate is a cover or light.
7. Enter a friendly name and optional settings such as travel time, tilt support, poll interval, group membership, or dim duration.
8. Companion stores the changes as an editable draft only.
9. Companion shows pending changes and the currently active ESP32 registry revision.
10. User presses **Push to ESP32**.
11. Companion sends the complete managed registry to the ESP32 through ESPHome Native API.
12. ESP32 validates the registry.
13. If valid, ESP32 accepts and persists the registry atomically.
14. If invalid, ESP32 rejects the update and keeps the previous registry active.
15. Companion creates or updates dynamic Home Assistant cover/light entities for the accepted registry.

## Manual push rule

Companion-managed changes must not silently change the active ESP32 RF configuration.

Required behaviour:

- Editing in the Companion creates a draft only.
- The user explicitly presses **Push to ESP32**.
- Companion shows what will change before pushing.
- ESP32 validates the complete registry before accepting it.
- ESP32 either accepts the whole registry or keeps the previous registry unchanged.
- Companion shows whether local changes are pending and which registry revision is active on the ESP32.

## Control path

### YAML-only mode

Normal control uses standard ESPHome entities exposed through the normal ESPHome integration:

- `cover.open_cover`
- `cover.close_cover`
- `cover.stop_cover`
- `light.turn_on`
- `light.turn_off`

### Companion-managed mode

Control uses dynamic Companion entities. The Companion sends commands to the ESP32 through ESPHome Native API, and the ESP32 performs RF execution:

```text
Home Assistant managed cover/light entity
  -> Elero Companion
    -> ESPHome Native API helper
      -> ESP32 RF queue/counter/retry logic
        -> CC1101 / Elero RF
```

RF correctness remains inside the ESP firmware.

## Recovery and migration

Users should be able to export Companion-managed configuration as YAML for backup, recovery, or migration to YAML-only mode.

If the Companion is unavailable, the ESP32 should still boot with the last accepted managed registry. If a user wants to return to YAML-only mode, they can export or manually recreate YAML entries and flash a YAML-only configuration.

## Legacy bridge

During migration, the legacy `/elero` web UI and `/elero/api/*` endpoints can be used as a fallback.

New user-facing workflows should be designed for Native API helpers first. REST should not become the long-term integration path.
