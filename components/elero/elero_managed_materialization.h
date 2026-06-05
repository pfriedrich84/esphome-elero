#pragma once

#include "elero_managed_registry.h"
#include <cstddef>
#include <vector>

namespace esphome {
namespace elero {
namespace managed_materialization {

struct SlotBinding {
  ManagedDeviceType type{ManagedDeviceType::COVER};
  size_t device_index{0};
  size_t slot_index{0};
  uint32_t blind_address{0};
};

struct Plan {
  std::vector<SlotBinding> bindings;
  size_t enabled_cover_count{0};
  size_t enabled_light_count{0};
  size_t cover_slot_count{0};
  size_t light_slot_count{0};
  size_t unbound_cover_count{0};
  size_t unbound_light_count{0};

  bool fits() const { return this->unbound_cover_count == 0 && this->unbound_light_count == 0; }
  size_t used_slot_count() const { return this->bindings.size(); }
};

inline Plan build_preallocated_slot_plan(const ManagedRegistry &registry, size_t cover_slot_count,
                                         size_t light_slot_count) {
  Plan plan{};
  plan.cover_slot_count = cover_slot_count;
  plan.light_slot_count = light_slot_count;

  size_t next_cover_slot = 0;
  size_t next_light_slot = 0;
  for (size_t i = 0; i < registry.devices.size(); i++) {
    const auto &device = registry.devices[i];
    if (!device.enabled)
      continue;

    if (device.type == ManagedDeviceType::LIGHT) {
      plan.enabled_light_count++;
      if (next_light_slot < light_slot_count) {
        plan.bindings.push_back({ManagedDeviceType::LIGHT, i, next_light_slot++, device.blind_address});
      } else {
        plan.unbound_light_count++;
      }
    } else {
      plan.enabled_cover_count++;
      if (next_cover_slot < cover_slot_count) {
        plan.bindings.push_back({ManagedDeviceType::COVER, i, next_cover_slot++, device.blind_address});
      } else {
        plan.unbound_cover_count++;
      }
    }
  }
  return plan;
}

}  // namespace managed_materialization
}  // namespace elero
}  // namespace esphome
