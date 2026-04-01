#pragma once
// Elero poll timer logic — extracted as pure functions for testability.

#include <cstdint>

namespace esphome {
namespace elero {
namespace poll_logic {

// Returns true if enough time has elapsed since the last poll.
inline bool should_poll_now(uint32_t now, uint32_t last_poll, uint32_t interval) {
  return (now - last_poll) > interval;
}

// Returns true if an immediate poll is allowed (rate-limited).
inline bool should_immediate_poll(uint32_t now, uint32_t last_immediate_ms, uint32_t min_interval) {
  return (now - last_immediate_ms) >= min_interval;
}

// Returns true if the post-movement poll is due.
inline bool should_post_movement_poll(uint32_t now, uint32_t post_movement_poll_at) {
  return post_movement_poll_at > 0 && now >= post_movement_poll_at;
}

// Calculates the post-movement poll timestamp.
// Returns 0 if durations are not configured (no dead-reckoning).
// direction: 1=opening, -1=closing
inline uint32_t calculate_post_movement_poll_time(int direction, uint32_t movement_start,
                                                  uint32_t open_duration, uint32_t close_duration,
                                                  uint32_t post_movement_delay) {
  uint32_t duration = (direction == 1) ? open_duration : close_duration;
  if (duration == 0)
    return 0;
  return movement_start + duration + post_movement_delay;
}

// Returns true if stop verification should trigger (time reached and retries remaining or exhausted).
// Caller must check retries < max_retries separately to decide re-send vs exhaustion.
inline bool should_stop_verify(uint32_t now, uint32_t stop_verify_at) {
  return stop_verify_at > 0 && now >= stop_verify_at;
}

// Calculates the effective poll interval.
// When moving without position tracking (no durations), uses the shorter moving interval + stagger.
// Otherwise returns the base interval.
// operation: 0=idle, 1=opening, -1=closing
inline uint32_t calculate_poll_interval(int operation, uint32_t base_interval,
                                        uint32_t open_duration, uint32_t close_duration,
                                        uint32_t moving_interval, uint32_t poll_offset,
                                        uint32_t movement_start, uint32_t now,
                                        uint32_t timeout_movement) {
  if (operation != 0 && (open_duration == 0 || close_duration == 0) &&
      (now - movement_start) < timeout_movement) {
    return moving_interval + (poll_offset % moving_interval);
  }
  return base_interval;
}

}  // namespace poll_logic
}  // namespace elero
}  // namespace esphome
