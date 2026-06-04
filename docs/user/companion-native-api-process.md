# Companion and YAML-only workflow

This document explains the intended user workflow once the Elero Companion moves to ESPHome Native API helpers.

## Permanent rule: YAML-only must always work

The Elero ESPHome component must remain fully usable without the Companion.

A user must always be able to:

- configure the hub in ESPHome YAML,
- configure covers and lights in ESPHome YAML,
- flash the ESPHome device,
- control covers and lights through the normal Home Assistant ESPHome integration,
- receive status feedback and diagnostics,
- recover the system from YAML and logs only.

The Companion is a comfort and support layer. It must not become required for normal operation.

## Target architecture

```text
ESPHome YAML
  Durable source of truth.

ESPHome Elero component
  RF correctness, CC1101 access, counters, queues, retries, diagnostics,
  and YAML-backed cover/light entities.

Home Assistant ESPHome integration
  Normal entity exposure through ESPHome Native API.

Elero Companion
  Setup assistance, discovery view, diagnostics, Repairs, and YAML generation.
```

The existing `/elero` web UI and REST endpoints may remain temporarily, but the long-term target is Companion communication through ESPHome Native API helpers only.

## Expected setup flow

1. Create an ESPHome YAML file with `api:` and `elero:` configured.
2. Flash the ESPHome device.
3. Add the ESPHome device to Home Assistant through the normal ESPHome integration.
4. Install the Elero Companion through HACS.
5. Start the Elero Companion Config Flow.
6. Select or link the existing ESPHome Elero hub.
7. Companion validates that the hub exposes the expected Native API helper surface.
8. Companion creates one hub-level device with diagnostics and helper actions.

The Companion does not create duplicate cover/light entities for devices that already exist in YAML.

## Discovery and YAML generation flow

1. Open the Elero Companion integration in Home Assistant.
2. Start discovery from the Companion.
3. Press buttons on the physical Elero remote.
4. The ESP firmware performs the RF scan and reports discovered candidates.
5. Companion shows candidates with address, remote address, channel, RSSI, last state, and duplicate status.
6. Choose whether the candidate is a cover or light.
7. Enter a friendly name and optional settings such as travel time, tilt support, poll interval, or dim duration.
8. Companion generates a YAML snippet.
9. Copy the YAML snippet into the ESPHome YAML file.
10. Flash the ESPHome device again.
11. After reboot, Home Assistant receives the new entity through the normal ESPHome integration.
12. Companion marks the candidate as persisted once it sees it as a YAML-backed configured device.

## Runtime adoption rule

Runtime adoption is only a temporary import helper.

A runtime adopted device is not durable configuration. It becomes persistent only after the generated YAML has been added to the ESPHome YAML file and flashed.

## Control path

Normal control uses the standard ESPHome entities:

- `cover.open_cover`
- `cover.close_cover`
- `cover.stop_cover`
- `light.turn_on`
- `light.turn_off`

The Companion should not implement a second command stack. RF command correctness remains inside the ESP firmware.

## Legacy bridge

During migration, the legacy `/elero` web UI and `/elero/api/*` endpoints can be used as a fallback.

New user-facing workflows should be designed for Native API helpers first. REST should not become the long-term integration path.
