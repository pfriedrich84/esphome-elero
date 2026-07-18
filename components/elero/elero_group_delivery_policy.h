#pragma once

// Dependency-light native/fallback policy for Elero groups.

#include "elero_command_delivery.h"

#include <vector>

namespace esphome {
namespace elero {
namespace group_delivery_policy {

enum class Route : uint8_t { NONE, NATIVE, MEMBERS };

inline bool native_profiles_compatible(const std::vector<CommandDeliveryConfig> &configs) {
  if (configs.size() < 2 || configs.size() > ELERO_MAX_DESTS)
    return false;
  const auto &first = configs.front();
  for (size_t i = 1; i < configs.size(); i++) {
    const auto &other = configs[i];
    if (!command_profile::can_share_native_group(first.profile, other.profile) ||
        first.mapping.open != other.mapping.open || first.mapping.close != other.mapping.close ||
        first.mapping.stop != other.mapping.stop || first.mapping.tilt != other.mapping.tilt)
      return false;
  }
  return true;
}

inline Route initial_route(const std::vector<CommandDeliveryConfig> &configs,
                           const CommandIntent &) {
  return native_profiles_compatible(configs) ? Route::NATIVE : Route::MEMBERS;
}

inline Route after_native_submit(IntentSubmitResult result) {
  return result == IntentSubmitResult::REJECTED ? Route::MEMBERS : Route::NONE;
}

inline Route after_native_outcome(const DeliveryOutcome &outcome) {
  return outcome.event == DeliveryEvent::DROPPED && !outcome.had_partial_delivery
             ? Route::MEMBERS
             : Route::NONE;
}

}  // namespace group_delivery_policy
}  // namespace elero
}  // namespace esphome
