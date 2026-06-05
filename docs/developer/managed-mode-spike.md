# Elero managed mode spike

This spike adds the first firmware-side surface for a future Home Assistant Companion-managed setup while preserving YAML-only operation.

## Implemented

- New optional ESPHome component:

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

- `elero_managed` depends on the existing `elero` hub. Existing YAML-defined `cover:` and `light:` entities are unchanged and do not require managed mode, Companion, HACS, `/elero`, or REST.
- Minimal managed registry model in firmware with:
  - `schema_version`
  - `registry_revision`
  - `hub_id` derived from the ESP32 eFuse MAC address
  - `devices[]`
  - FNV-1a checksum integrity field
- Device records are prepared for cover/light type, name, Elero RF addresses/channel, pck/hop/payload fields, durations, poll interval, capabilities, and enabled state.
- Firmware helper methods are present for the Native API spike surface:
  - `get_elero_info()`
  - `get_elero_managed_registry()`
  - `validate_elero_managed_registry()`
  - `push_elero_managed_registry()`
- Accepted registries are saved with ESPHome preferences and loaded at boot. Invalid pushes are rejected before replacing the active registry.

The helpers are exposed as ESPHome Native API user services in this pass:

- `get_elero_info`
- `get_elero_managed_registry`
- `validate_elero_managed_registry(registry_json: string)`
- `push_elero_managed_registry(registry_json: string)`

Because ESPHome user-defined API services are command-style calls rather than direct request/response RPCs, responses are published to an auto-created diagnostic text sensor named `Elero Managed API Result`. Payloads are JSON strings. This is the first Native API path; custom protobuf/native messages remain a future option if response size or correlation needs outgrow text-sensor responses.

`hub_id` is ESP32-owned. The Companion should discover it with `get_elero_info()`, keep it with the draft registry, and echo it in pushes. The ESP32 rejects registries whose `hub_id` does not match the local eFuse-MAC-derived ID.

## Still a spike / blocker

ESPHome entities are normally materialized during Python code generation from YAML. This pass does **not** hot-add Home Assistant-visible `cover`/`light` entities from the persisted registry at runtime.

Current behavior after Push to ESP32:

- Registry validation and persistence can be exercised in firmware code.
- The accepted registry survives reboot.
- Home Assistant entity materialization from that registry is not implemented yet.
- A restart/reconnect alone is not sufficient in this pass to create managed entities, because no generated entity objects are created from the stored registry yet.

Next investigation should decide whether preallocated C++ cover/light entity slots can be safely registered with ESPHome/Native API at boot from stored registry data. If ESPHome cannot expose those as normal Native API entities, document that limitation before considering any Companion command-proxy path.

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
