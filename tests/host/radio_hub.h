#pragma once
#include "framework.h"
#include "radio_types.h"
#include "elero/cc1101.h"
#include "elero/elero_rx_fifo.h"
#include "elero/elero_radio_timing.h"
#include "elero/elero_status_read.h"
#include <atomic>
#include <cstring>
#include <deque>
#include <map>

#define RADIOLIB_ERR_NONE 0
#define pdTRUE 1
#define pdMS_TO_TICKS(x) (x)
namespace esphome {
inline void delay_microseconds_safe(uint32_t) {}
namespace elero {
inline int xQueueSend(std::vector<TxCompletion> *q, const TxCompletion *c, uint32_t) {
  q->push_back(*c); return pdTRUE;
}
struct TestPin { bool value{false}; bool digital_read() const { return value; } };
class Elero;
struct TestRadio {
  Elero *hub;
  int16_t standby();
};

// SPI and queues are fakes; process_rx, send_command_internal_, advance_tx,
// abort/flush, read_status/read_buf and publish_tx_completion_ are verbatim
// production bodies. The chip model asserts/records strobes, not RF success.
class Elero {
 public:
  Elero() { radio_storage.hub = this; }
  struct RxFifoIO;
  bool process_rx(bool keep_idle = false);
  void advance_tx();
  bool send_command_internal_(t_elero_command *, uint32_t = 0);
  void tx_abort_();
  void flush_rx();
  void flush_and_rx();
  bool enter_idle_();
  uint8_t read_status_once_(uint8_t);
  uint8_t read_status(uint8_t);
  bool read_status_stable(uint8_t, uint8_t &);
  void read_buf(uint8_t, uint8_t *, uint8_t);
  void publish_tx_completion_(uint32_t, bool);
  void increment_parser_drop_count(const char *reason) { drops.emplace_back(reason); }
  void capture_raw_packet_(uint8_t) {}
  void interpret_msg() {
    received.emplace_back(msg_rx_, msg_rx_ + msg_rx_[0] + 3);
    metadata.push_back(current_rx_meta_);
  }
  void observe_tx_queue_latency_(uint32_t) {}
  void reset() { rx_fifo_.reset(); fifo.clear(); tx_fifo.clear(); marc = CC1101_MARCSTATE_IDLE; }
  bool init() { write_cmd(CC1101_SRX); return true; }
  void enable() { header_pending = true; }
  void disable() {}
  uint8_t transfer_byte(uint8_t byte) {
    if (header_pending) { address = byte & 0x3f; header_pending = false; return 0; }
    if (!script[address].empty()) {
      auto v = script[address].front(); script[address].pop_front(); return v;
    }
    switch (address) {
      case CC1101_MARCSTATE: return marc;
      case CC1101_RXBYTES: return fifo.size() | (rx_overflow ? 0x80 : 0);
      case CC1101_TXBYTES: return tx_fifo.size() | (tx_underflow ? 0x80 : 0);
      case CC1101_PKTSTATUS: return channel_clear ? 0x10 : 0;
      case CC1101_RXFIFO: {
        if (marc != CC1101_MARCSTATE_IDLE || fifo.empty()) { illegal_fifo_read = true; return 0; }
        auto v = fifo.front(); fifo.pop_front(); return v;
      }
      default: return 0;
    }
  }
  void write_cmd(uint8_t cmd) {
    strobes.push_back(cmd);
    switch (cmd) {
      case CC1101_SIDLE: marc = CC1101_MARCSTATE_IDLE; pin.value = false; break;
      case CC1101_SRX: marc = CC1101_MARCSTATE_RX; break;
      case CC1101_SFRX:
        illegal_flush |= marc != CC1101_MARCSTATE_IDLE;
        fifo.clear(); rx_overflow = false; break;
      case CC1101_SFTX:
        illegal_flush |= marc != CC1101_MARCSTATE_IDLE && marc != CC1101_MARCSTATE_TXFIFO_UFLOW;
        tx_fifo.clear(); tx_underflow = false; break;
      case CC1101_STX:
        stx_from_rx &= marc == CC1101_MARCSTATE_RX;
        if (channel_clear && !reject_stx) { marc = CC1101_MARCSTATE_TX; pin.value = true; }
        break;
    }
  }
  void write_burst(uint8_t, const uint8_t *bytes, uint8_t count) { tx_fifo.assign(bytes, bytes + count); }
  void finish_tx() { tx_fifo.clear(); marc = CC1101_MARCSTATE_RX; pin.value = false; tx_done_ = true; }

  TestPin pin;
  TestPin *gdo0_pin_{&pin};
  TestRadio radio_storage{};
  TestRadio *radio_{&radio_storage};
  uint8_t msg_rx_[64]{}, msg_tx_[64]{};
  RxTimeline rx_timeline_;
  RxMetadata current_rx_meta_{};
  RxFifoReader rx_fifo_;
  CcaBackoff cca_backoff_;
  bool tx_started_seen_{false};
  std::atomic<bool> spi_failed_{false}, radio_fatal_error_{false}, rx_ready_{false}, tx_done_{false};
  std::atomic<bool> packet_dump_mode_{false};
  bool packet_dump_pending_update_{false};
  std::atomic<uint8_t> radio_mode_{static_cast<uint8_t>(RadioMode::RX)};
  std::atomic<TxState> tx_state_{TxState::IDLE};
  std::atomic<uint32_t> tx_count_{0};
  uint32_t tx_state_entered_ms_{0}, last_tx_complete_ms_{0}, active_tx_transaction_id_{0};
  uint32_t rx_overflow_count_{0}, last_rx_overflow_ms_{0};
  std::vector<TxCompletion> completions;
  std::vector<TxCompletion> *tx_completion_queue_{&completions};
  std::vector<std::vector<uint8_t>> received;
  std::vector<RxMetadata> metadata;
  std::vector<std::string> drops;
  std::vector<uint8_t> strobes, tx_fifo;
  std::deque<uint8_t> fifo;
  std::map<uint8_t, std::deque<uint8_t>> script;
  uint8_t marc{CC1101_MARCSTATE_RX}, address{0};
  bool channel_clear{true}, reject_stx{false}, header_pending{false};
  bool rx_overflow{false}, tx_underflow{false};
  bool illegal_fifo_read{false}, illegal_flush{false}, stx_from_rx{true};
};
inline int16_t TestRadio::standby() { hub->write_cmd(CC1101_SIDLE); return RADIOLIB_ERR_NONE; }
}}
