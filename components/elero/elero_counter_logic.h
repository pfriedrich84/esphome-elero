#pragma once

#include <cstdint>

namespace esphome {
namespace elero {
namespace counter_logic {

static constexpr uint32_t COUNTER_RESYNC_GAP_MS = 30000;

inline bool is_stale_counter(uint8_t last_seen, uint8_t current) {
  uint8_t forward_distance = static_cast<uint8_t>(current - last_seen);
  return forward_distance == 0 || forward_distance > 127;
}

inline bool should_resync_counter(uint32_t last_seen_ms, uint32_t now_ms,
                                  uint32_t resync_gap_ms = COUNTER_RESYNC_GAP_MS) {
  return static_cast<uint32_t>(now_ms - last_seen_ms) >= resync_gap_ms;
}

struct CounterDecision {
  bool accept;
  uint32_t next_activity_ms;
};

inline CounterDecision evaluate_status_counter(uint8_t last_seen, uint8_t current,
                                               uint32_t last_activity_ms, uint32_t now_ms,
                                               bool has_last_activity = true,
                                               uint32_t resync_gap_ms = COUNTER_RESYNC_GAP_MS) {
  const bool stale = is_stale_counter(last_seen, current);
  const bool accept = !stale || !has_last_activity || should_resync_counter(last_activity_ms, now_ms, resync_gap_ms);
  return {accept, now_ms};
}

}  // namespace counter_logic
}  // namespace elero
}  // namespace esphome
