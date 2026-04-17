#pragma once
// Elero command dispatch decision predicates — extracted as pure functions for testability.

#include <cstdint>
#include <algorithm>

namespace esphome {
namespace elero {
namespace dispatch_logic {

// Returns true if the command queue has been sitting without a successful drain
// for longer than max_age_ms (stale commands should be cleared).
inline bool should_clear_stale_queue(uint32_t now, uint32_t last_drain_ms, uint32_t max_age_ms) {
  return (now - last_drain_ms) >= max_age_ms;
}

// Returns true if this cover is in stop-verification and the front command
// should be deferred (i.e., it is not a stop or check command).
inline bool should_defer_for_stop(bool stop_urgent_self, uint8_t front_cmd, uint8_t stop_cmd,
                                  uint8_t check_cmd) {
  if (!stop_urgent_self)
    return false;
  return front_cmd != stop_cmd && front_cmd != check_cmd;
}

// Returns the dispatch delay including exponential backoff on retries.
// Backoff: base + (10 << min(retries, 3)) → +20ms, +40ms, +80ms for retries 1-3+.
inline uint32_t calculate_dispatch_delay(uint32_t base_delay, uint8_t send_retries) {
  if (send_retries == 0)
    return base_delay;
  uint8_t shift = std::min(send_retries, static_cast<uint8_t>(3));
  return base_delay + (10u << shift);
}

// Returns true if a command is ready to dispatch.
// Stop commands bypass delay entirely (time-critical).
inline bool is_dispatch_ready(bool is_stop_cmd, uint32_t now, uint32_t last_command,
                              uint32_t delay) {
  if (is_stop_cmd)
    return true;
  return (now - last_command) >= delay;
}

// Returns true if retry count exceeds the maximum (command should be dropped).
inline bool should_drop_after_retries(uint8_t send_retries, uint8_t max_retries) {
  return send_retries >= max_retries;
}

}  // namespace dispatch_logic
}  // namespace elero
}  // namespace esphome
