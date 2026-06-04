# Home Assistant RF platform and HACS companion plan

Issue: [#179](https://github.com/pfriedrich84/esphome-elero/issues/179)

This document is an implementation plan and RFC for future Home Assistant integration work. It keeps two related ideas separate:

1. A **HACS companion integration** for better Home Assistant UX around the existing ESPHome Elero hub.
2. A future **Home Assistant `radio_frequency` consumer** spike that tests whether Elero RF packet generation can move into Home Assistant.

The current ESPHome external component remains the stable and recommended integration path until a spike proves another path is safer and more useful.

## Current baseline

`esphome-elero` is not just a generic RF sender. The ESPHome firmware currently owns:

- CC1101 setup, frequency configuration, packet-mode TX/RX, watchdog, and recovery.
- Elero RF packet parsing, validation, CRC/LQI/RSSI handling, and status dispatch.
- Rolling command counters, command queues, retry behaviour, and stale-counter safety.
- Bidirectional status feedback, polling, dead-reckoned position/brightness state, and runtime adopted blinds.
- ESPHome/Home Assistant `cover`, `light`, sensor, text sensor, button, switch, and group-cover entities.
- The optional `/elero` web UI with RF discovery, packet dump, runtime adoption, control, diagnostics, and YAML export.

That means any Home Assistant side integration must avoid taking ownership of RF correctness too early.

## Strategic direction

Use this ownership model:

```text
ESPHome component owns RF correctness.
HACS companion owns Home Assistant user experience.
HA radio_frequency spike owns future RF abstraction research.
```

The most valuable near-term path is a **HACS companion integration** that connects to an already running Elero ESPHome hub and improves onboarding, diagnostics, import, and support workflows. It should not create duplicate cover/light entities by default.

## Architecture options

### Option A: ESPHome external component remains primary

Home Assistant receives normal ESPHome entities through the ESPHome Native API.

Benefits:

- Matches the current codebase and tested runtime model.
- Keeps RF timing, ACK/status handling, discovery, polling, and CC1101 recovery close to the hardware.
- Avoids maintaining a second command stack.
- Works independently of new Home Assistant RF platform maturity.

Costs:

- Setup is still YAML-heavy.
- Runtime adopted blinds need a manual path to become persistent YAML.
- Advanced diagnostics live mostly in the ESP web UI instead of Home Assistant.

Decision: keep as the default path.

### Option B: HACS companion integration

A Home Assistant custom integration distributed through HACS connects to the Elero hub's `/elero/api/*` endpoints.

Initial role:

- Config flow for host/auth setup.
- Device and diagnostics view for the Elero hub.
- Repairs for common hub/radio/configuration problems.
- Discovery/adoption overview.
- YAML export/copy/download workflow.
- Links into the ESP `/elero` web UI.

Initial non-role:

- No RF packet encoding in Home Assistant.
- No `radio_frequency` dependency.
- No duplicate cover/light entities by default.
- No replacement for ESPHome YAML or ESPHome Native API entities.

This is the preferred first implementation track because it creates user-visible value while preserving the current RF safety boundary.

### Option C: Optional HACS proxy entities

A later opt-in mode could create Home Assistant cover/light entities that send commands to the ESP web API rather than to the ESPHome Native API.

Potential use cases:

- Runtime adopted blinds before users convert them to persistent ESPHome YAML.
- Users who intentionally disable or avoid ESPHome entity exposure for selected devices.
- Testing Home Assistant side UX without changing RF packet ownership.

Risks:

- Duplicate entities if ESPHome already exposes the same covers/lights.
- Different availability/state semantics between ESPHome Native API and REST polling.
- More support complexity.

Guardrails before implementation:

- Must be explicitly opt-in.
- Must detect configured ESPHome entities where practical and warn before creating duplicates.
- Must document command ownership clearly.
- Must not bypass ESP-side queue, counter, and validation behaviour.

### Option D: Home Assistant `radio_frequency` consumer

A future Home Assistant integration generates Elero RF commands and sends them via a `radio_frequency` transmitter entity, for example a generic ESPHome/CC1101 transmitter if one exists.

Potential benefits:

- Native Home Assistant config flow for Elero devices.
- Alignment with Home Assistant's new RF transmitter/consumer architecture.
- A possible path toward non-Elero-specific ESPHome RF proxy firmware.

Open risks:

- Elero may depend on CC1101 packet/register behaviour that a generic RF command abstraction cannot express.
- Bidirectional ACK/status, polling, discovery, stale-counter handling, and runtime adoption may not fit a send-only abstraction.
- Rolling counters and command intent retry behaviour would need a Home Assistant side owner.
- Duplicating protocol encoding in Python risks drift from the firmware implementation.

Decision: keep this as an active research spike, not the first implementation.

## HACS companion MVP

### Goals

The MVP should make an existing Elero hub easier to operate from Home Assistant without changing the RF control path.

Required user-facing capabilities:

- Add integration through Config Flow with host/IP and optional HTTP Basic Auth credentials.
- Validate connectivity against the Elero web API.
- Create one Home Assistant device for the Elero hub.
- Show hub information: name, frequency, configured cover/light counts, runtime adopted counts, web UI enabled/disabled state where available.
- Expose diagnostics through Home Assistant diagnostics download.
- Raise Repairs for actionable issues.
- Fetch YAML export and present a copy/download workflow.
- Link users to `http://<host>/elero` for full ESP web UI workflows.

### Non-goals

- Do not create cover/light entities by default.
- Do not implement Elero RF packet encoding in Python.
- Do not send commands through Home Assistant `radio_frequency`.
- Do not replace the ESPHome external component or ESPHome Native API integration.
- Do not persist user secrets in logs, diagnostics, or issue reports.

### Candidate Home Assistant platforms

MVP candidates:

- `config_flow`: configure host/auth and validate the Elero hub.
- `diagnostics`: export sanitized hub/API status for support.
- `repairs`: surface actionable setup and radio-health problems.
- `button`: optional helper buttons such as refresh diagnostics, download YAML, or open web UI if Home Assistant UX supports it cleanly.
- `sensor` / `binary_sensor`: optional hub-level diagnostics only, not per-blind control entities.

Post-MVP candidates:

- `cover` / `light`: optional proxy entities only after duplicate detection and user opt-in are designed.
- `update`: only if there is a reliable, documented way to map ESPHome external component versions to update advice. Do not imply OTA control unless implemented safely.

## ESP web API requirements

The companion integration should use stable, documented ESP endpoints. Existing useful endpoints include:

| Endpoint | Use in companion |
|---|---|
| `GET /elero/api/info` | Connectivity, hub metadata, counts, web UI status. |
| `GET /elero/api/status` | Combined status, configured/runtime devices, diagnostics, packet/log snippets depending on tab parameters. |
| `GET /elero/api/configured` | Configured covers/lights inventory. |
| `GET /elero/api/runtime` | Runtime adopted blind/light inventory. |
| `GET /elero/api/discovered` | RF discovery results for import guidance. |
| `GET /elero/api/yaml` | YAML export helper. |
| `GET /elero/api/frequency` | Frequency display and mismatch diagnostics. |
| `GET /elero/api/packets` | Packet dump overview. |
| `GET /elero/api/packets/download` | Support bundle packet dump. |
| `GET /elero/api/logs` | Optional support log capture display. |
| `GET /elero/api/ui/status` | Web UI enabled/disabled state. |
| `POST /elero/api/diagnostics/reset` | Optional user-triggered reset of monotonic diagnostics. |

Before building the HACS integration, review these endpoints for:

- authentication behaviour,
- stable JSON field names,
- secret-free diagnostics,
- same-origin/CORS policy implications for Home Assistant requests,
- payload sizes suitable for Home Assistant polling,
- version fields needed for compatibility checks.

If the current API is not stable enough, add an ESP-side API version endpoint before relying on it from HACS.

## Repairs and diagnostics plan

Candidate Repairs:

- Elero hub unreachable.
- Authentication failed.
- Web UI/API disabled.
- CC1101 SPI failure or chip not found.
- Radio watchdog recovery count increasing.
- RX packet drops dominated by CRC/bounds errors.
- No configured covers/lights.
- Runtime adopted devices exist but are not persisted in YAML.
- Frequency appears non-standard and user has not acknowledged it.

Diagnostics bundle should include sanitized data only:

- integration version,
- Home Assistant version,
- hub API version if available,
- hub info/status JSON,
- frequency registers/MHz,
- configured/runtime counts,
- diagnostics counters,
- recent packet-dump metadata if explicitly requested.

Never include Wi-Fi secrets, OTA/API keys, Home Assistant tokens, raw credentials, or unredacted user-provided secrets.

## Duplicate entity policy

The companion MVP must avoid duplicates by not creating per-blind control entities.

If optional proxy entities are later added:

1. Make proxy mode opt-in at integration or device level.
2. Show a warning that ESPHome may already expose the same covers/lights.
3. Prefer stable unique IDs derived from hub identity plus blind address and device type.
4. Provide an import/conversion path from runtime adopted device to ESPHome YAML.
5. Document that ESPHome Native API entities remain the recommended control path.

## HA `radio_frequency` research plan

This remains a separate spike from the HACS companion MVP.

Research questions:

- Which Home Assistant Core APIs does `radio_frequency` provide in the target release?
- Which integrations expose compatible `RadioFrequencyTransmitterEntity` instances?
- Can ESPHome with CC1101 appear as such a transmitter?
- Which frequency, modulation, raw timing, and command models are supported?
- Can one known Elero UP/DOWN/STOP command be represented as a `RadioFrequencyCommand` without CC1101-specific packet-mode assumptions?
- Can ACK/status/polling be represented, or is the platform send-only for this use case?
- Would Elero protocol encoding belong in `rf-protocols`, a standalone Python library, or the HA integration?

Spike steps:

1. Review Home Assistant developer documentation and source for the `radio_frequency` platform.
2. Build or identify a minimal ESPHome/CC1101 RF transmitter fixture.
3. Confirm a transmitter entity appears in Home Assistant.
4. Send a dummy RF command and verify physical output with SDR, logs, or a second CC1101.
5. Extract a known Elero command packet from the existing firmware/protocol notes.
6. Attempt to model it as a Home Assistant RF command.
7. Document either the fit or the first blocking mismatch.

Go criteria for a future HA RF consumer:

- Elero commands can be safely represented by the HA RF command model.
- A compatible transmitter exists and works reliably with CC1101 hardware.
- Rolling counters and retries have a clear owner.
- Users can avoid duplicate ESPHome and HA entities.
- Loss of firmware-near ACK/status/discovery behaviour is acceptable or solved.

No-go criteria:

- Elero needs CC1101 packet/register behaviour outside the RF abstraction.
- The platform is effectively send-only and cannot preserve the main bidirectional value of this project.
- Counter/retry semantics would become less safe than the current firmware path.
- The implementation would duplicate too much protocol logic without tests or shared libraries.

## Suggested implementation phases

### Phase 0: Documentation and API audit

- Keep this RFC updated.
- Add a README note that ESPHome remains the standard path and HACS/HA RF work is future/optional.
- Audit `/elero/api/*` for stable companion consumption.
- Decide whether an explicit API version field is needed.

### Phase 1: HACS companion skeleton

Repository strategy: keep the HACS companion in this repository under `custom_components/elero_companion/` while the ESPHome external component remains under `components/`. This keeps firmware API changes and companion integration changes reviewable in one place. HACS uses `custom_components/elero_companion/`; ESPHome uses `components/elero*`, so the Home Assistant Python files are not compiled into ESP32 firmware images.

Skeleton scope:

- `hacs.json` at repository root.
- `custom_components/elero_companion/manifest.json`, `config_flow.py`, `const.py`, `coordinator.py`, `api.py`, `diagnostics.py`, `repairs.py`.
- Config Flow validates host/auth.
- DataUpdateCoordinator polls a low-cost status endpoint.
- Device registry entry for the Elero hub.
- No per-blind control entities.

### Phase 2: Companion MVP

- Diagnostics download.
- Repairs for the first actionable failures.
- YAML export flow.
- Hub-level diagnostics sensors if they add value.
- User docs for installation and boundaries.

### Phase 3: Optional proxy design

- Design duplicate detection and opt-in UX.
- Decide if proxy entities should be limited to runtime adopted devices.
- Add tests for unique IDs, unload/reload, auth failures, and stale hub data.
- Only then implement proxy covers/lights.

### Phase 4: HA RF spike

- Run the `radio_frequency` research plan.
- Document outcome here or in a follow-up ADR.
- Only start a HA RF consumer implementation after the go criteria are met.

## Open decisions

- Should the HACS companion eventually move to a separate repository after the API stabilizes, or remain in this monorepo permanently?
- What stable hub identity should unique IDs use: MAC, ESPHome name, API-provided ID, or configured hostname?
- Should `/elero/api/info` expose an explicit API schema version?
- Should runtime adopted devices be shown only as diagnostics/import candidates, or should they become the first proxy-entity use case?
- Which Home Assistant Repairs are high-signal enough to avoid noisy warnings?
- How much packet/log data should diagnostics include by default versus only on explicit user action?

## Current recommendation

Proceed in this order:

1. Document and audit the companion API boundary.
2. Build a HACS companion MVP focused on setup, diagnostics, Repairs, YAML export, and web UI linking.
3. Keep per-blind control entities out of the MVP.
4. Treat optional proxy entities and HA `radio_frequency` consumer support as separate, later decisions.

This gives users a better Home Assistant experience while preserving the RF correctness and bidirectional behaviour that already work in the ESPHome firmware.
