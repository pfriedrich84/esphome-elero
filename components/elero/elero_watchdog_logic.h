#pragma once
// Elero watchdog escalation helpers — pure logic for testability and consistency.

#include "cc1101.h"
#include <cstdint>

#define ELERO_HAS_WATCHDOG_ACTION_HELPER 1

namespace esphome {
namespace elero {
namespace watchdog_logic {

struct EscalationState {
  uint8_t flush_count{0};
  uint8_t reset_count{0};
  uint32_t window_start_ms{0};
};

enum class RecoveryAction : uint8_t {
  RESTART_RX,
  FLUSH_RX,
  FLUSH_AND_RX,
  RESET,
  FAIL,
};

inline void reset_on_healthy(EscalationState &state, uint32_t now_ms) {
  state.flush_count = 0;
  state.reset_count = 0;
  state.window_start_ms = now_ms;
}

inline void reset_if_window_expired(EscalationState &state, uint32_t now_ms,
                                    uint32_t window_ms) {
  if ((now_ms - state.window_start_ms) >= window_ms) {
    state.flush_count = 0;
    state.reset_count = 0;
    state.window_start_ms = now_ms;
  }
}

inline RecoveryAction choose_recovery_action(uint8_t marc, uint8_t flush_count,
                                             uint8_t reset_count, uint8_t max_flushes,
                                             uint8_t max_resets) {
  if (marc == CC1101_MARCSTATE_IDLE)
    return RecoveryAction::RESTART_RX;
  if (flush_count < max_flushes)
    return marc == CC1101_MARCSTATE_RXFIFO_OFLOW ? RecoveryAction::FLUSH_RX
                                                 : RecoveryAction::FLUSH_AND_RX;
  if (reset_count < max_resets)
    return RecoveryAction::RESET;
  return RecoveryAction::FAIL;
}

}  // namespace watchdog_logic
}  // namespace elero
}  // namespace esphome
