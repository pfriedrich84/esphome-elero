#pragma once
// Elero TX helpers — pure sanitization logic for testability.

#include <cstdint>

#define ELERO_HAS_TX_ADDR_HELPER 1

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

inline uint8_t count_nonzero_dest_addrs(const uint32_t *dest_addrs, uint8_t max_dests) {
  uint8_t count = 0;
  while (count < max_dests && dest_addrs[count] != 0) {
    count++;
  }
  return count;
}

inline uint16_t calculate_msg_len(uint8_t num_dests) {
  // header(16) + num_dests*3 + payload(10)
  return static_cast<uint16_t>(16u + static_cast<uint16_t>(num_dests) * 3u + 10u);
}

inline bool is_msg_len_valid(uint16_t msg_len, uint16_t max_packet_size) {
  return msg_len <= max_packet_size;
}

}  // namespace tx_logic
}  // namespace elero
}  // namespace esphome
