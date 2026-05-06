---
name: compile
description: "Run ESPHome compile test to verify the component builds. Use after code changes, before commits, or to test specific config variants."
user-invocable: true
---

# ESPHome Compile Test

Canonical repository instructions live in [`../../AGENTS.md`](../../AGENTS.md); this Claude skill only defines the slash-command workflow.

Compile the ESPHome firmware to verify the component builds without errors.

## Task

1. Determine which config to compile:
   - No argument or `default`: `tests/configs/compile_test.yaml`
   - `minimal`: `tests/configs/minimal.yaml`
   - `multi`: `tests/configs/multi_cover.yaml`
   - `light`: `tests/configs/light_only.yaml`
   - `noweb`: `tests/configs/no_web.yaml`
   - `nosensors`: `tests/configs/no_auto_sensors.yaml`
   - `freq`: `tests/configs/custom_frequency.yaml`
   - `all`: compile all configs sequentially
   - Any `.yaml` path: compile that specific file

2. Run: `esphome compile <config>`

3. If compilation **succeeds**:
   - Report binary size and RAM usage from the output
   - Confirm success

4. If compilation **fails**, analyze the error:
   - Missing includes → check the 3-file split (elero.cpp, elero_cc1101.cpp, elero_protocol.cpp)
   - Undefined symbols → check RadioLib v7.1.2 dependency
   - Template errors → check ESPHome version compatibility
   - Schema errors → run `esphome config <file>` first for better error messages
   - Suggest specific fixes

## Arguments

`$ARGUMENTS` — config shortname, file path, or `all`

## Examples

- `/compile` — compile the main test config
- `/compile all` — compile all 7 config variants
- `/compile minimal` — compile the minimal config only
