#pragma once
#include <cstdint>

namespace esphome { namespace elero { namespace counter_logic {
static constexpr uint32_t COUNTER_RESYNC_GAP_MS = 30000;
inline bool is_stale_counter(uint8_t last_seen, uint8_t current) {
  const uint8_t distance = static_cast<uint8_t>(current - last_seen);
  return distance == 0 || distance > 127;
}
inline bool should_resync_counter(uint32_t last_seen_ms, uint32_t now_ms,
                                  uint32_t gap_ms = COUNTER_RESYNC_GAP_MS) {
  return static_cast<uint32_t>(now_ms - last_seen_ms) >= gap_ms;
}

struct CounterState {
  bool initialized{false};
  uint8_t accepted{0};
  uint32_t accepted_at_ms{0};
  uint32_t activity_at_ms{0};  // telemetry only, never moves the accepted frontier
  uint8_t candidate{0}, candidate_count{0};
  uint32_t candidate_first_ms{0}, candidate_last_ms{0};
};
struct CounterDecision { bool accept{false}; bool resynced{false}; };

inline CounterDecision evaluate_status_counter(CounterState &state, uint8_t current, uint32_t now) {
  state.activity_at_ms = now;
  auto accept = [&](bool resync) {
    state.initialized = true;
    state.accepted = current;
    state.accepted_at_ms = now;
    state.candidate_count = 0;
    return CounterDecision{true, resync};
  };
  if (!state.initialized || !is_stale_counter(state.accepted, current)) return accept(false);
  if (current == state.accepted) return {};  // a duplicate never authorizes resync
  if (state.candidate_count != 0 && current == state.candidate) return {};

  const uint8_t step = static_cast<uint8_t>(current - state.candidate);
  const bool advances = state.candidate_count != 0 && step >= 1 && step <= 16 &&
      now - state.candidate_last_ms <= 10000 && now - state.candidate_first_ms <= 30000;
  if (advances) {
    if (state.candidate_count < 3) ++state.candidate_count;
  } else {
    state.candidate_count = 1;
    state.candidate_first_ms = now;
  }
  state.candidate = current;
  state.candidate_last_ms = now;
  if (state.candidate_count >= 3 && now - state.candidate_first_ms >= 1000 &&
      should_resync_counter(state.accepted_at_ms, now))
    return accept(true);
  return {};
}
}}}
