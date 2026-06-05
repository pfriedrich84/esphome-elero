# Managed entity materialization uses preallocated ESPHome slots

## Status

Accepted.

## Context

Companion-managed mode needs persisted managed registry entries to become Home Assistant-visible cover/light entities through the normal ESPHome Native API path.

Two implementation strategies were considered:

1. **Boot-time dynamic C++ registration**: load the persisted registry, allocate `EleroCover` / `EleroLight` objects in C++, and register them with ESPHome at boot.
2. **Compile-time preallocated slots**: Python codegen creates a bounded number of managed cover/light slots from YAML; firmware binds accepted registry entries to those slots at boot.

Dynamic registration is attractive because only real managed devices would appear and users would not need to size slot capacity ahead of time. However, ESPHome entity/component lifecycle is primarily codegen-driven. The public API exposes entity registration such as `App.register_cover()` / `App.register_light()`, but component setup/loop registration is codegen/friend-oriented, and light entities normally require generated `LightState` and `LightOutput` wiring. A runtime-only approach risks entities appearing without reliable setup/loop participation, command routing, stable object IDs, or correct light state wiring.

## Decision

Use **compile-time preallocated slots** as the primary managed entity materialization strategy.

Managed-mode YAML owns explicit capacity planning:

```yaml
elero_managed:
  enabled: true
  max_devices: 32
  preallocated_cover_slots: 16
  preallocated_light_slots: 16
```

Firmware validates accepted registries against the configured slot capacity when either slot count is non-zero. Registry pushes that exceed available cover/light slots are rejected before persistence.

The pure planning seam is `managed_materialization::build_preallocated_slot_plan(registry, cover_slots, light_slots)`. It deterministically assigns enabled managed devices to type-specific slots in registry order and reports unbound devices when capacity is insufficient.

Boot-time dynamic C++ registration remains an exploratory fallback/proof spike only. It should not replace the preallocated-slot path unless it proves normal ESPHome setup/loop participation, Native API entity listing, command routing, light state wiring, and stable object IDs.

## Consequences

- Managed entity capacity is explicit and compile-time bounded.
- Changing managed capacity requires YAML edit/recompile/OTA.
- Placeholder/disabled slot behavior still needs careful implementation before user-facing release.
- Registry updates remain atomic and revision-guarded.
- Companion must surface slot-capacity errors clearly and may guide users to increase YAML slot counts.
- Companion remains out of the normal command path after entities are materialized.

## Validation

Initial validation for this decision:

- C++ unit tests cover slot planning and insufficient-capacity reporting.
- Python schema tests cover slot-capacity validation.
- `esphome compile tests/configs/managed_minimal.yaml` passes with `preallocated_cover_slots: 16` and `preallocated_light_slots: 16`.
- Full C++ unit suite passes locally.
