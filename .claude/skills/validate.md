---
name: validate
description: "Validate ESPHome YAML configs without full compilation. Faster than /compile — catches schema errors quickly."
user-invocable: true
---

# Validate ESPHome Config

Run ESPHome schema validation (no compilation) to quickly catch YAML errors.

## Task

1. Determine which config to validate:
   - No argument: `tests/configs/compile_test.yaml`
   - `all`: validate all YAML configs in project
   - Any `.yaml` path: validate that specific file

2. Run: `esphome config <config>`

3. If validation **passes**: confirm clean, show resolved config summary

4. If validation **fails**, categorize the error:
   - **Schema error**: missing required field, wrong type, invalid range
   - **Cross-component**: duplicate blind_address, missing hub dependency
   - **Strapping pin**: GPIO12 on ESP32 (fatal), GPIO0/2/5/15 (warning)
   - **Duration consistency**: open_duration set but close_duration missing (or vice versa)
   - Suggest specific fix for each error

## Speed

`esphome config` only validates YAML + Python schemas. No C++ compilation.
Much faster than `/compile` — use this first when iterating on YAML changes.

## Arguments

`$ARGUMENTS` — config file path, `all`, or empty (defaults to `tests/configs/compile_test.yaml`)

## Files validated when `all`:

- `tests/configs/compile_test.yaml`
- `example.yaml`
- `tests/configs/minimal.yaml`
- `tests/configs/multi_cover.yaml`
- `tests/configs/light_only.yaml`
- `tests/configs/no_web.yaml`
- `tests/configs/no_auto_sensors.yaml`
- `tests/configs/custom_frequency.yaml`
