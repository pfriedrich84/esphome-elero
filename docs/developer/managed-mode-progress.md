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
- Unused slots are marked failed/unavailable during setup.

Not implemented yet:

- Runtime hot-binding immediately after push without reboot.
- Dynamic entity names from registry device names; slot entity names are currently stable placeholder names.
- Full cover position/dimming parity with YAML entities; managed slots initially provide direct RF command routing and basic state updates.
- Companion migration from REST to Native API/service-call-first.

## Next implementation target

Implement both managed cover and managed light preallocated slots in one pass:

1. Generate up to configured cover/light slot entities from `elero_managed` codegen.
2. Bind persisted managed registry records to slots at boot.
3. Ensure active slots register with the existing Elero hub mappings and use existing RF queue/counter behavior.
4. Make unused slots clearly unavailable/disabled where ESPHome supports it.
5. Validate with a managed `type: "light"` registry first, then cover.

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
