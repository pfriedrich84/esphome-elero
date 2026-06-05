# Elero managed mode spike

This spike adds the first firmware-side surface for a future Home Assistant Companion-managed setup while preserving YAML-only operation.

## Implemented

- New optional ESPHome component:

  ```yaml
  api:
    encryption:
      key: !secret api_key
    custom_services: true

  elero:
    cs_pin: GPIO12
    gdo0_pin: GPIO03

  elero_managed:
    enabled: true
    max_devices: 32
    preallocated_cover_slots: 0
    preallocated_light_slots: 0
  ```

- `elero_managed` depends on the existing `elero` hub. Existing YAML-defined `cover:` and `light:` entities are unchanged and do not require managed mode, Companion, HACS, `/elero`, or REST.
- Minimal managed registry model in firmware with:
  - `schema_version`
  - `registry_revision`
  - `hub_id` derived from the ESP32 eFuse MAC address
  - `devices[]`
  - FNV-1a checksum integrity field
- Device records are prepared for cover/light type, name, Elero RF addresses/channel, pck/hop/payload fields, durations, poll interval, capabilities, and enabled state.
- Optional `preallocated_cover_slots` and `preallocated_light_slots` YAML settings are present for the preallocated-slot materialization spike. They default to `0`, so current firmware still does not create managed entities. When non-zero, registry validation rejects pushes whose enabled managed covers/lights exceed the configured slot capacity.
- Firmware helper methods are present for the Native API spike surface:
  - `get_elero_info()`
  - `get_elero_managed_registry()`
  - `validate_elero_managed_registry()`
  - `push_elero_managed_registry()`
- `clear_elero_managed_registry()`
- Accepted registries are saved with ESPHome preferences and loaded at boot. Invalid pushes/clears are rejected before replacing the active registry.

The helpers are exposed as ESPHome Native API user services in this pass:

- `get_elero_info`
- `get_elero_managed_registry`
- `validate_elero_managed_registry(registry_json)`
- `push_elero_managed_registry(registry_json)`
- `clear_elero_managed_registry(registry_revision, confirm)`

ESPHome requires `api.custom_services: true` for these services. Because ESPHome user-defined API services are command-style calls rather than direct request/response RPCs, `elero_managed` publishes command status and hub metadata to enabled diagnostic text sensors by default instead of exposing one large JSON response entity.

Diagnostic text sensors are auto-created with `entity_category: diagnostic` and remain enabled by default:

- `Elero Managed Last Call`
- `Elero Managed Last OK`
- `Elero Managed Last Error`
- `Elero Managed Enabled`
- `Elero Managed Schema Version`
- `Elero Managed Component Version`
- `Elero Managed Hub ID`
- `Elero Managed Hub MAC`
- `Elero Managed Max Devices`
- `Elero Managed Registry Revision`
- `Elero Managed Device Count`
- `Elero Managed Entity Materialization`

An optional raw response mailbox can still be configured with `response_text_sensor` for debugging full JSON payloads, but it is no longer auto-created.

`hub_id` is ESP32-owned. The Companion should discover it with `get_elero_info()`, keep it with the draft registry, and echo it in pushes. The ESP32 rejects registries whose `hub_id` does not match the local eFuse-MAC-derived ID.

Registry pushes and clears are revision-guarded. The candidate `registry_revision` must match the ESP32's currently active revision and its checksum must be computed over that revision. On a successful push, the ESP32 persists the candidate as the next revision (`active + 1`) and refreshes the checksum. A clear request must include both the active revision and `confirm: "CLEAR"`; a successful clear persists an empty registry as the next revision. This rejects stale Companion drafts instead of silently overwriting or clearing a newer accepted registry.

## Still a spike / blocker

ESPHome entities are normally materialized during Python code generation from YAML. This pass does **not** hot-add Home Assistant-visible `cover`/`light` entities from the persisted registry at runtime.

Current behavior after Push to ESP32:

- Registry validation and persistence can be exercised in firmware code.
- The accepted registry survives reboot.
- Home Assistant entity materialization from that registry is not implemented yet.
- A restart/reconnect alone is not sufficient in this pass to create managed entities, because no generated entity objects are created from the stored registry yet.

Next investigation should decide whether preallocated C++ cover/light entity slots can be safely registered with ESPHome/Native API at boot from stored registry data. If ESPHome cannot expose those as normal Native API entities, document that limitation before considering any Companion command-proxy path.

## Managed entity materialization investigation

Candidate strategies to test, in preferred order:

1. **Boot-time C++ materialization from the persisted registry.** Allocate/register managed cover/light objects during `elero_managed::setup()` before Home Assistant receives the Native API entity list. This keeps Companion out of the command path and may avoid exposing unused placeholder entities, but must prove ESPHome registration APIs can be called safely outside Python codegen.
2. **Compile-time preallocated slots.** Generate configured cover/light slots and bind active slots to the stored registry at boot. This is likely closest to ESPHome's normal entity model, but may expose placeholder/disabled entities or unstable names unless carefully managed. The pure `managed_materialization::build_preallocated_slot_plan()` seam now models deterministic type-specific slot assignment and insufficient-capacity reporting for this strategy, and the `elero_managed` YAML surface now includes explicit cover/light slot capacities.
3. **Restart-required refresh after push.** Accept/persist registry changes immediately, then require ESP restart or Home Assistant reconnect so the Native API entity list is rebuilt from boot-time materialization.
4. **Companion command adapter fallback.** Only consider this if normal ESPHome Native API entity materialization is proven infeasible; it would violate the preferred direct command path and needs explicit documentation before implementation.

Accepted decision: use compile-time preallocated slots as the primary strategy; see [`adr/2026-06-05-managed-entity-materialization.md`](adr/2026-06-05-managed-entity-materialization.md). Current ESPHome API inspection makes strategy 2 the safer implementation path: `App.register_cover()` / `App.register_light()` are public, but component registration for setup/loop participation is codegen/friend-oriented (`register_component_`), and dynamic lights also need the normal generated `LightState`/output pairing. Runtime-created entities may be possible with custom lifecycle forwarding, but that would be more fragile than preallocated codegen entities. The next firmware spike should therefore continue with compile-time preallocated slots first, while keeping a small boot-time dynamic registration experiment as a fallback/proof-of-impossibility task.

## Native API smoke testing

For a firmware flashed with `api.custom_services: true`, the Home Assistant ESPHome integration exposes the helper actions as services named like:

- `esphome.<node>_get_elero_info`
- `esphome.<node>_get_elero_managed_registry`
- `esphome.<node>_validate_elero_managed_registry`
- `esphome.<node>_push_elero_managed_registry`
- `esphome.<node>_clear_elero_managed_registry`

The repository includes a dependency-free helper to compute the managed-registry checksum, call the validation/push services through the Home Assistant REST API, and poll the managed diagnostic text sensors:

```bash
HA_TOKEN=<long-lived-access-token> \
  python3 scripts/managed_native_api_smoke.py \
    --ha-url http://homeassistant.local:8123 \
    --node-name test-managed-minimal \
    --hub-id <Elero Managed Hub ID sensor value> \
    --current-revision <Elero Managed Registry Revision sensor value> \
    --revision-entity text_sensor.<registry_revision_entity_id> \
    --device-count-entity text_sensor.<device_count_entity_id> \
    --push
```

Run it once without `--push` for a validation-only dry run. Add `--check-stale-revision --last-ok-entity text_sensor.<last_ok_entity_id>` to verify that revision-guarded validation rejects a draft whose `registry_revision` does not match the active ESP32 registry. Use `--registry-json-file draft.json --refresh-checksum` to smoke-test a Companion-generated complete registry instead of the default one-device sample. After a push, reboot or OTA the ESP and verify persistence with `--verify-only --expect-revision <revision> --expect-device-count <count>` using the `Elero Managed Registry Revision` and `Elero Managed Device Count` diagnostic text sensors. Use `--clear --current-revision <revision>` to cleanup a smoke-test registry through the revision-guarded clear service. If `response_text_sensor` is explicitly configured, pass `--raw-response-entity` to print the raw JSON mailbox state; otherwise the raw response entity is intentionally absent.

## Validation

Relevant checks:

```bash
ruff check components/
ruff format --check components/
pytest tests/python/ -v --tb=short
cmake -S tests -B tests/build -DCMAKE_BUILD_TYPE=Debug
cmake --build tests/build --parallel
cd tests/build && ctest --output-on-failure -V
esphome compile tests/configs/minimal.yaml
esphome compile tests/configs/managed_minimal.yaml
python3 scripts/check_markdown_links.py
```
