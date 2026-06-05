# Managed mode progress handoff

Updated: 2026-06-05

## Verified on real ESP32 / Home Assistant

- Firmware compiles and OTA/flashing works with `elero_managed` enabled.
- Split diagnostic text sensors are exposed and update correctly.
- `response_text_sensor` is not auto-created; raw JSON mailbox remains opt-in.
- Native API user services exposed through Home Assistant work:
  - `get_elero_info`
  - `get_elero_managed_registry`
  - `validate_elero_managed_registry`
  - `push_elero_managed_registry`
  - `clear_elero_managed_registry`
- Registry validation/push works with pretty JSON passed from Home Assistant service YAML.
- Managed light slot was verified on real hardware: persisted registry record bound at boot and HA light service calls routed RF through the ESPHome/Elero command path.
- Revision guard works for accepted pushes.
- Accepted registry persists across ESP reboot.
- Clear with `registry_revision` and `confirm: "CLEAR"` works.
- Tested values from hardware session:
  - node: `lilygo_elero`
  - hub_id: `2823578228`
  - hub_mac: `98:a3:16:e2:50:e0`
  - max_devices: `10`
  - preallocated cover/light slots configured as `8` / `2`
  - `Elero Managed Entity Materialization` before slot binding: `preallocated_slots_planned_restart_required`
  - `Elero Managed Entity Materialization` after slot binding implementation: `preallocated_slots_bound_at_boot_restart_required`

## Accepted design decision

Use compile-time preallocated ESPHome slots as the primary managed entity materialization strategy.

- ADR: `docs/developer/adr/2026-06-05-managed-entity-materialization.md`
- Dynamic boot-time C++ entity registration remains exploratory only.
- User specifically wants both covers and lights implemented together because lights are the best real test case.

## Current code state before slot binding

Implemented:

- Managed registry model and preference persistence.
- Revision-guarded validate/push/clear.
- Slot capacity YAML:
  - `preallocated_cover_slots`
  - `preallocated_light_slots`
- Pure slot planner:
  - `components/elero/elero_managed_materialization.h`
- Tests for registry revision guard, schema slot limits, and slot planning.
- Smoke helper:
  - `scripts/managed_native_api_smoke.py`

Implemented in current follow-up work:

- Generated preallocated managed cover/light slot entities from `elero_managed` codegen.
- Boot-time binding of persisted registry devices into those slots.
- Unused slots are intentionally left unbound and ignore commands instead of marking the shared component failed.
- Managed light slots suppress duplicate writes caused by HA/ESPHome light transitions and default generated managed light transitions are `0s`.
- Companion now has a Native API service-call adapter and companion-level services for get/validate/push/clear managed registry operations. Push/clear create a restart-required notification because slot binding happens at ESP boot.

Not implemented yet:

- Runtime hot-binding immediately after push without reboot.
- Dynamic entity names from registry device names; slot entity names are currently stable placeholder names.
- Full cover position/dimming parity with YAML entities; managed slots initially provide direct RF command routing and basic state updates.
- Full Companion UI/device workflow for authoring managed registries; the current Companion migration exposes Native API-backed services but does not yet build registries from discovered devices.

## Next implementation target

Continue Companion-managed workflow integration:

1. Add a Companion registry builder that turns discovered/adopted devices into managed registry JSON and computes the firmware checksum.
2. Add UX/actions around validate -> push -> restart-required guidance.
3. Add options/migration support for configuring `esphome_node` on existing Companion entries.
4. Improve firmware slot parity for cover position/dimming behavior and registry-name visibility where ESPHome allows it.

## Validation commands

```bash
export PATH="$HOME/.local/bin:$PATH"
ruff check components/ scripts/managed_native_api_smoke.py tests/python/test_managed_schema.py
ruff format --check components/ tests/python/test_managed_schema.py
pytest tests/python/ -v --tb=short
cmake --build tests/build --parallel
cd tests/build && ctest --output-on-failure -V
esphome compile tests/configs/managed_minimal.yaml
python3 scripts/check_markdown_links.py
git diff --check
```
