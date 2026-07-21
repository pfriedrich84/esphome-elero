#pragma once
// Elero cover position logic — extracted as pure functions for testability.

#include <algorithm>
#include <cmath>
#include <cstdint>

#define ELERO_HAS_COVER_POLL_HELPER 1
#define ELERO_HAS_COVER_PUBLISH_HELPER 1
#define ELERO_HAS_COVER_COOLDOWN_HELPER 1

namespace esphome {
namespace elero {
namespace cover_logic {

inline bool should_poll(uint32_t now, uint32_t last_poll_ms, uint32_t poll_interval_ms) {
  return (now - last_poll_ms) >= poll_interval_ms;
}

inline bool should_publish_position(uint32_t now, uint32_t last_publish_ms,
                                    uint32_t publish_interval_ms) {
  return (now - last_publish_ms) >= publish_interval_ms;
}

inline bool command_cooldown_active(uint32_t now, uint32_t cooldown_until) {
  return cooldown_until != 0 && static_cast<int32_t>(now - cooldown_until) < 0;
}

// Endpoint targets represent OPEN/CLOSE commands in ESPHome's CoverCall and
// must always reach RF even when the estimated position already matches. Only
// an intermediate set-position target may be suppressed as redundant.
inline bool is_redundant_intermediate_target(float current, float target,
                                             float tolerance = 0.01f) {
  return target > 0.0f && target < 1.0f && std::abs(target - current) < tolerance;
}

// Returns the updated position after dead-reckoning.
// direction: +1 opening, -1 closing (0 is treated as no-op, returns position unchanged)
// action_duration_ms: open_duration or close_duration in ms
// elapsed_ms: time since last recompute
// timeout_ms: max plausible elapsed time (ELERO_TIMEOUT_MOVEMENT)
// Returns position unchanged if duration is zero, elapsed exceeds timeout, or direction is 0.
inline float recompute_position(float position, int direction, float action_duration_ms,
                                uint32_t elapsed_ms, uint32_t timeout_ms) {
  if (direction == 0 || action_duration_ms == 0.0f)
    return position;
  if (elapsed_ms > timeout_ms)
    return position;
  float delta = static_cast<float>(direction) * static_cast<float>(elapsed_ms) / action_duration_ms;
  return std::clamp(position + delta, 0.0f, 1.0f);
}

// Calculates the margin for target comparison based on TX latency compensation.
// compensation_ms: base compensation + (queue_depth * per-entry overhead)
// duration_ms: open or close duration
inline float calculate_margin(uint32_t compensation_ms, uint32_t duration_ms) {
  if (duration_ms == 0)
    return 0.0f;
  return static_cast<float>(compensation_ms) / static_cast<float>(duration_ms);
}

// Returns true if the cover has reached (or passed) its target position.
// Returns false for fully open (target=1.0) or fully closed (target=0.0) — the motor handles its own stop.
// Returns true for idle (operation=0).
// operation: 0=idle, 1=opening, -1=closing
// base_compensation_ms: ELERO_TX_LATENCY_COMPENSATION_MS
// per_queue_entry_ms: additional ms per pending TX queue entry (typically 15)
inline bool is_at_target(float position, float target, int operation, uint32_t open_duration_ms,
                         uint32_t close_duration_ms, uint32_t queue_depth,
                         uint32_t base_compensation_ms, uint32_t per_queue_entry_ms = 15) {
  if (target == 1.0f || target == 0.0f)
    return false;

  uint32_t compensation = base_compensation_ms + (queue_depth * per_queue_entry_ms);
  float margin = 0.0f;

  switch (operation) {
    case 1:  // opening
      margin = calculate_margin(compensation, open_duration_ms);
      return position >= (target - margin);
    case -1:  // closing
      margin = calculate_margin(compensation, close_duration_ms);
      return position <= (target + margin);
    case 0:  // idle
    default:
      return true;
  }
}

}  // namespace cover_logic
}  // namespace elero
}  // namespace esphome
