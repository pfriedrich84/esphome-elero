#pragma once

#include <cstdint>

namespace esphome {
namespace elero {
namespace counter_logic {

inline bool is_stale_counter(uint8_t last_seen, uint8_t current) {
  uint8_t forward_distance = static_cast<uint8_t>(current - last_seen);
  return forward_distance == 0 || forward_distance > 127;
}

}  // namespace counter_logic
}  // namespace elero
}  // namespace esphome
