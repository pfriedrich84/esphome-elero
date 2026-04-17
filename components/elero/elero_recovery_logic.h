#pragma once
// Elero recovery helpers — pure predicates for reinit failure accounting.

#include <cstdint>

namespace esphome {
namespace elero {
namespace recovery_logic {

inline uint8_t next_reinit_failure_count(uint8_t current,
                                         bool already_incremented_this_cycle) {
  if (already_incremented_this_cycle)
    return current;
  return (current == 0xFF) ? 0xFF : static_cast<uint8_t>(current + 1);
}

}  // namespace recovery_logic
}  // namespace elero
}  // namespace esphome
