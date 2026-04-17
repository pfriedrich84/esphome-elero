#pragma once
// Elero TX helpers — pure sanitization logic for testability.

#include <cstdint>

namespace esphome {
namespace elero {
namespace tx_logic {

inline uint8_t sanitize_num_dests(uint8_t requested, uint8_t max_dests) {
  if (requested == 0)
    return 1;
  if (requested > max_dests)
    return max_dests;
  return requested;
}

}  // namespace tx_logic
}  // namespace elero
}  // namespace esphome
