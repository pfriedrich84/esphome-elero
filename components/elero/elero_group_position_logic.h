#pragma once

// Dependency-light planning helpers for Elero group set-position commands.
// Native multi-destination packets can synchronize only one command byte and
// therefore one movement direction. Per-member auto-stop timing remains owned
// by each cover after the synchronized native start was accepted.

#include "elero_command_delivery.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace esphome {
namespace elero {
namespace group_position_logic {

struct MemberState {
  float position{NAN};
  uint32_t open_duration_ms{0};
  uint32_t close_duration_ms{0};
  bool position_trusted{false};
};

enum class Route : uint8_t {
  REJECT,
  ALREADY_AT_TARGET,
  MEMBER_TARGETS,
  NATIVE_OPEN,
  NATIVE_CLOSE,
};

struct Plan {
  Route route{Route::REJECT};
  CommandIntentKind native_direction{CommandIntentKind::CHECK};
};

inline bool is_intermediate_target(float target) {
  return std::isfinite(target) && target > 0.0f && target < 1.0f;
}

inline bool is_calibrated(const MemberState &member) {
  return member.open_duration_ms > 0 && member.close_duration_ms > 0;
}

inline bool has_known_position(const MemberState &member) {
  return member.position_trusted && std::isfinite(member.position) &&
         member.position >= 0.0f && member.position <= 1.0f;
}

inline bool supports_group_position(const MemberState *members, size_t count) {
  if (members == nullptr || count < 2)
    return false;
  for (size_t i = 0; i < count; i++) {
    if (!is_calibrated(members[i]))
      return false;
  }
  return true;
}

inline Plan plan_intermediate_target(const MemberState *members, size_t count, float target,
                                     bool native_compatible, float tolerance = 0.005f) {
  if (!is_intermediate_target(target) || !supports_group_position(members, count))
    return {};

  bool any_open = false;
  bool any_close = false;
  bool any_move = false;
  bool any_stationary = false;
  for (size_t i = 0; i < count; i++) {
    if (!has_known_position(members[i]))
      return {};
    const float delta = target - members[i].position;
    if (std::fabs(delta) <= tolerance) {
      any_stationary = true;
      continue;
    }
    any_move = true;
    if (delta > 0.0f)
      any_open = true;
    else
      any_close = true;
  }

  if (!any_move)
    return {Route::ALREADY_AT_TARGET, CommandIntentKind::CHECK};

  // A native multi-destination movement is safe only when every moving member
  // needs the same direction and no stationary member would be dragged away
  // from the requested target. Mixed directions require per-member intents.
  if (native_compatible && !any_stationary && any_open != any_close) {
    return {any_open ? Route::NATIVE_OPEN : Route::NATIVE_CLOSE,
            any_open ? CommandIntentKind::OPEN : CommandIntentKind::CLOSE};
  }

  return {Route::MEMBER_TARGETS, CommandIntentKind::CHECK};
}

inline bool uses_native_start(Route route) {
  return route == Route::NATIVE_OPEN || route == Route::NATIVE_CLOSE;
}

}  // namespace group_position_logic
}  // namespace elero
}  // namespace esphome
