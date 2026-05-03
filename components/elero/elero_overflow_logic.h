#pragma once
// Elero RX overflow helpers — pure logic for escalation and testability.

#include <cstdint>

namespace esphome {
namespace elero {
namespace overflow_logic {

inline uint8_t next_overflow_count(uint32_t now_ms, uint32_t last_overflow_ms,
                                   uint8_t current_count,
                                   uint32_t rapid_window_ms) {
  if ((now_ms - last_overflow_ms) < rapid_window_ms) {
    return (current_count == 0xFF) ? 0xFF : static_cast<uint8_t>(current_count + 1);
  }
  return 1;
}

inline bool should_reinit_after_overflow_count(uint8_t overflow_count,
                                               uint8_t threshold) {
  return overflow_count >= threshold;
}

}  // namespace overflow_logic
}  // namespace elero
}  // namespace esphome
