#pragma once
// Elero packet validation predicates — extracted as pure functions for testability.

#include <cstdint>

namespace esphome {
namespace elero {
namespace packet_validation {

static constexpr uint8_t MIN_PACKET_SIZE = 17;
static constexpr uint8_t MAX_PACKET_SIZE = 57;
static constexpr uint8_t FIFO_LENGTH = 64;
static constexpr uint8_t HEADER_OVERHEAD = 27;  // bytes before payload in a command packet

inline bool is_valid_packet_length(uint8_t length) {
  return length >= MIN_PACKET_SIZE && length <= MAX_PACKET_SIZE;
}

inline uint8_t max_safe_dests() { return (MAX_PACKET_SIZE - HEADER_OVERHEAD) / 3; }

inline bool is_valid_dest_count(uint8_t num_dests) {
  return num_dests >= 1 && num_dests <= max_safe_dests();
}

inline uint8_t calculate_dests_length(uint8_t typ, uint8_t num_dests) {
  return (typ > 0x60) ? static_cast<uint8_t>(num_dests * 3) : num_dests;
}

inline bool is_valid_packet_bounds(uint8_t length, uint8_t dests_len) {
  // Minimum decode safety: payload[0..1] + encrypted payload bytes must exist.
  // interpret_msg() reads payload bytes through index (19 + dests_len + 9),
  // so length must be at least (28 + dests_len).
  uint16_t offset = 26u + dests_len;
  return (offset + 2u) <= length && offset < FIFO_LENGTH;
}

inline bool is_rssi_in_bounds(uint8_t length) { return (uint16_t)(length + 2) < FIFO_LENGTH; }

inline uint8_t extract_crc(uint8_t status_byte) { return status_byte >> 7; }

inline uint8_t extract_lqi(uint8_t status_byte) { return status_byte & 0x7f; }

}  // namespace packet_validation
}  // namespace elero
}  // namespace esphome
