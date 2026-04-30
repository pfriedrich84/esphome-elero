#pragma once
// Runtime adopted blind behaviour — pure logic shared by hub-managed runtime blinds.

#include <cstdint>

#define ELERO_HAS_RUNTIME_STALE_COUNTER_HELPER 1

namespace esphome {
namespace elero {
namespace runtime_blind_logic {

inline bool is_poll_enabled(uint32_t poll_interval_ms) {
  return poll_interval_ms != 0 && poll_interval_ms != UINT32_MAX;
}

inline bool should_poll_runtime_blind(uint32_t now, uint32_t last_poll_ms,
                                      uint32_t poll_interval_ms, bool queue_empty) {
  return is_poll_enabled(poll_interval_ms) && queue_empty &&
         (now - last_poll_ms) >= poll_interval_ms;
}

inline uint8_t next_command_counter(uint8_t current) {
  return current == 0xFF ? 1 : static_cast<uint8_t>(current + 1);
}

inline bool should_advance_counter_on_stale_clear(uint8_t send_packets_count) {
  return send_packets_count > 0;
}

inline int8_t direction_for_state(uint8_t state, uint8_t top_state, uint8_t bottom_state,
                                  uint8_t start_up_state, uint8_t moving_up_state,
                                  uint8_t start_down_state, uint8_t moving_down_state,
                                  int8_t current_direction) {
  if (state == start_up_state || state == moving_up_state)
    return 1;
  if (state == start_down_state || state == moving_down_state)
    return -1;
  if (state == top_state || state == bottom_state)
    return 0;
  return current_direction;
}

inline bool state_stops_runtime_motion(uint8_t state, uint8_t stopped_state,
                                       uint8_t intermediate_state, uint8_t tilt_state,
                                       uint8_t blocking_state, uint8_t overheated_state,
                                       uint8_t timeout_state) {
  return state == stopped_state || state == intermediate_state || state == tilt_state ||
         state == blocking_state || state == overheated_state || state == timeout_state;
}

inline bool can_recompute_position(int8_t direction, uint32_t open_duration_ms,
                                   uint32_t close_duration_ms, float position) {
  return direction != 0 && open_duration_ms != 0 && close_duration_ms != 0 && position >= 0.0f;
}

inline float recompute_position(float position, int8_t direction, uint32_t elapsed_ms,
                                uint32_t open_duration_ms, uint32_t close_duration_ms) {
  float delta = direction > 0 ? static_cast<float>(elapsed_ms) / static_cast<float>(open_duration_ms)
                              : static_cast<float>(elapsed_ms) / static_cast<float>(close_duration_ms);
  float next = direction > 0 ? position + delta : position - delta;
  if (next > 1.0f)
    return 1.0f;
  if (next < 0.0f)
    return 0.0f;
  return next;
}

}  // namespace runtime_blind_logic
}  // namespace elero
}  // namespace esphome
