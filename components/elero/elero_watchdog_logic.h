#pragma once
// Elero watchdog escalation helpers — pure logic for testability and consistency.

#include <cstdint>

namespace esphome {
namespace elero {
namespace watchdog_logic {

struct EscalationState {
  uint8_t flush_count{0};
  uint8_t reset_count{0};
  uint32_t window_start_ms{0};
};

inline void reset_on_healthy(EscalationState &state, uint32_t now_ms) {
  state.flush_count = 0;
  state.reset_count = 0;
  state.window_start_ms = now_ms;
}

inline void reset_if_window_expired(EscalationState &state, uint32_t now_ms,
                                    uint32_t window_ms) {
  if ((now_ms - state.window_start_ms) > window_ms) {
    state.flush_count = 0;
    state.reset_count = 0;
    state.window_start_ms = now_ms;
  }
}

}  // namespace watchdog_logic
}  // namespace elero
}  // namespace esphome
