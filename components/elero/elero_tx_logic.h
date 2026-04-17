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

inline uint8_t calculate_msg_len(uint8_t num_dests) {
  // header(16) + num_dests*3 + payload(10)
  return static_cast<uint8_t>(16 + num_dests * 3 + 10);
}

inline bool is_msg_len_valid(uint8_t msg_len, uint8_t max_packet_size) {
  return msg_len <= max_packet_size;
}

}  // namespace tx_logic
}  // namespace elero
}  // namespace esphome
