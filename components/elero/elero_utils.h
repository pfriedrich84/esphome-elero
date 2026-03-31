#pragma once
// Elero utility functions — extracted as pure functions for testability.

#include <cstdint>

namespace esphome {
namespace elero {
namespace utils {

static constexpr uint8_t RSSI_SIGN_BIT = 127;
static constexpr int8_t RSSI_OFFSET = -74;
static constexpr int RSSI_DIVISOR = 2;

inline float calculate_rssi(uint8_t rssi_raw) {
  if (rssi_raw > RSSI_SIGN_BIT) {
    return static_cast<float>(static_cast<int8_t>(rssi_raw)) / RSSI_DIVISOR + RSSI_OFFSET;
  }
  return static_cast<float>(rssi_raw) / RSSI_DIVISOR + RSSI_OFFSET;
}

inline float registers_to_mhz(uint8_t freq2, uint8_t freq1, uint8_t freq0) {
  return (26.0f / 65536.0f) * ((static_cast<uint32_t>(freq2) << 16) |
                                (static_cast<uint32_t>(freq1) << 8) |
                                 static_cast<uint32_t>(freq0));
}

}  // namespace utils
}  // namespace elero
}  // namespace esphome
