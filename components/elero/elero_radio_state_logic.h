#pragma once
// CC1101 radio state decisions — pure logic for RX/TX orchestration.

#include "cc1101.h"
#include <cstdint>

#define ELERO_HAS_TX_TIMEOUT_HELPER 1

namespace esphome {
namespace elero {
namespace radio_state_logic {

inline bool is_tx_progress_state(uint8_t marc) {
  return marc == CC1101_MARCSTATE_TX ||
         marc == CC1101_MARCSTATE_STARTCAL ||
         marc == CC1101_MARCSTATE_BWBOOST ||
         marc == CC1101_MARCSTATE_FS_LOCK ||
         marc == CC1101_MARCSTATE_IFADCON ||
         marc == CC1101_MARCSTATE_ENDCAL ||
         marc == CC1101_MARCSTATE_FSTXON ||
         marc == CC1101_MARCSTATE_RXTX_SWITCH;
}

inline bool is_watchdog_healthy_rx(uint8_t marc) {
  return marc == CC1101_MARCSTATE_RX;
}

inline bool is_watchdog_transient_state(uint8_t marc) {
  if (marc >= CC1101_MARCSTATE_VCOON_MC && marc <= CC1101_MARCSTATE_ENDCAL)
    return true;
  return marc == CC1101_MARCSTATE_RX_END || marc == CC1101_MARCSTATE_RX_RST ||
         marc == CC1101_MARCSTATE_TXRX_SWITCH || marc == CC1101_MARCSTATE_RXTX_SWITCH;
}

inline bool should_restart_rx_from_idle(uint8_t marc) {
  return marc == CC1101_MARCSTATE_IDLE;
}

inline bool has_tx_timed_out(uint32_t elapsed_ms, uint32_t timeout_ms) {
  return elapsed_ms >= timeout_ms;
}

inline uint8_t rx_drain_limit(bool priority_tx_pending, bool normal_tx_pending, uint8_t max_when_idle) {
  return (priority_tx_pending || normal_tx_pending) ? 1 : max_when_idle;
}

inline uint8_t bounded_fifo_count(uint8_t avail, uint8_t fifo_length) {
  return avail > fifo_length ? fifo_length : avail;
}

inline bool has_complete_fifo_packet(uint8_t declared_length, uint8_t fifo_count) {
  return static_cast<uint16_t>(declared_length) + 3u <= fifo_count;
}

}  // namespace radio_state_logic
}  // namespace elero
}  // namespace esphome
