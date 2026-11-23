#pragma once
#include "framework.h"
#include "radio_types.h"
#include "elero/cc1101.h"
#include "elero/elero_rx_fifo.h"
#include "elero/elero_radio_timing.h"
#include "elero/elero_status_read.h"
#include "elero/elero_reinit_request.h"
#include <array>
#include <functional>
#include <atomic>
#include <cstring>
#include <deque>
#include <map>

#define RADIOLIB_ERR_NONE 0
#define pdTRUE 1
#define pdMS_TO_TICKS(x) (x)
// Only the RadioLib boundary is fake; the production estimator supplies these
// explicit settings rather than reading the library's stale bootstrap cache.
enum ModemType_t { RADIOLIB_MODEM_FSK };
struct DataRate_t { struct { uint32_t bitRate, freqDev; } fsk; };
struct PacketConfig_t { struct { uint16_t preambleLength; uint8_t syncWordLength, crcLength; } fsk; };
namespace esphome {
inline uint32_t test_sub_us = 0;
inline uint32_t micros() { return test_now * 1000 + test_sub_us; }
inline void delay_microseconds_safe(uint32_t us) { test_sub_us += us; }
inline void delay(uint32_t ms) { test_now += ms; }
inline unsigned test_task = 1;
inline unsigned xTaskGetCurrentTaskHandle() { return test_task; }
namespace elero {
inline int xQueueSend(std::vector<TxCompletion> *q, const TxCompletion *c, uint32_t) {
  q->push_back(*c); return pdTRUE;
}
struct TestPin { bool value{false}; bool digital_read() const { return value; } };
class Elero;
struct TestControlQueue {
  Elero *hub{nullptr};
  bool full{false}, immediate{true};
  std::deque<RadioMessage> messages;
};
inline int xQueueSend(TestControlQueue *, const RadioMessage *, uint32_t);
struct TestRadio {
  Elero *hub;
  int16_t enableRxAfterTx(bool enable);
  uint32_t calculateTimeOnAir(ModemType_t, DataRate_t rate, PacketConfig_t packet, size_t len) {
    last_rate = rate; last_packet = packet; last_length = len;
    return ((packet.fsk.preambleLength + packet.fsk.syncWordLength + packet.fsk.crcLength * 8 + len * 8) * 1000000u) /
           rate.fsk.bitRate;
  }
  DataRate_t last_rate{};
  PacketConfig_t last_packet{};
  size_t last_length{0};
  std::function<void()> during_enable_rx;
};

// SPI and queues are fakes; process_rx, send_command_internal_, advance_tx,
// abort/flush, read_status/read_buf and publish_tx_completion_ are verbatim
// production bodies. The chip model asserts/records strobes, not RF success.
class Elero {
 public:
  Elero() { radio_storage.hub = this; control_queue.hub = this; }
  struct RxFifoIO;
  struct RadioProfileIO;
  bool process_rx(bool keep_idle = false);
  void advance_tx();
  bool send_command_internal_(t_elero_command *, uint32_t = 0);
  void tx_abort_();
  void flush_rx();
  void flush_and_rx();
  bool enter_idle_();
  bool init();
  bool init_at_frequency_(uint32_t);
  bool reinit_frequency(uint8_t, uint8_t, uint8_t);
  uint32_t get_frequency_registers() const { return frequency_registers_.load(); }
  uint32_t estimate_airtime_us_(uint8_t);
  bool wait_rx();
  bool apply_frequency_(uint8_t, uint8_t, uint8_t);
  void handle_frequency_reinit_(uint32_t, uint8_t, uint8_t, uint8_t);
  void check_radio_state_();
  bool is_tx_idle() const { return tx_state_ == TxState::IDLE; }
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
  void reset() {
    ++resets;
    radio_profile_valid_ = false;
    rx_ready_ = false; tx_done_ = false;
    rx_fifo_.reset(); fifo.clear(); tx_fifo.clear(); registers.fill(0); marc = CC1101_MARCSTATE_IDLE;
  }
  void write_reg(uint8_t addr, uint8_t value) {
    illegal_config_write |= marc != CC1101_MARCSTATE_IDLE;
    if (addr != dropped_write) registers[addr] = value;
  }
  uint8_t read_reg(uint8_t addr) { return registers[addr]; }
  void enable() { header_pending = true; }
  void disable() {}
  uint8_t transfer_byte(uint8_t byte) {
    if (header_pending) { address = byte & 0x3f; patable_index = 0; header_pending = false; return 0; }
    if (!script[address].empty()) {
      auto v = script[address].front(); script[address].pop_front(); return v;
    }
    switch (address) {
      case CC1101_MARCSTATE: return marc;
      case CC1101_RXBYTES: return fifo.size() | (rx_overflow ? 0x80 : 0);
      case CC1101_TXBYTES: return tx_fifo.size() | (tx_underflow ? 0x80 : 0);
      case CC1101_PKTSTATUS: return channel_clear ? 0x10 : 0;
      case CC1101_PATABLE: return patable[patable_index++ % patable.size()];
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
      case CC1101_SIDLE:
        if (!ignore_idle) { marc = CC1101_MARCSTATE_IDLE; pin.value = false; }
        break;
      case CC1101_SRX:
        if (!reject_rx) marc = CC1101_MARCSTATE_RX;
        if (calibrate_on_rx) registers[CC1101_FSCAL1] ^= 0x01;
        break;
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
  void write_burst(uint8_t addr, const uint8_t *bytes, uint8_t count) {
    if (addr == CC1101_PATABLE) {
      std::copy(bytes, bytes + count, patable.begin());
      if (corrupt_patable) patable.back() ^= 0x01;
    } else { tx_fifo.assign(bytes, bytes + count); }
  }
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
  std::atomic<bool> radio_profile_valid_{true};  // Existing delivery tests start after successful setup.
  FrequencyReinitRequest reinit_request_;
  TestControlQueue control_queue;
  TestControlQueue *tx_queue_{&control_queue};
  unsigned radio_task_handle_{2};
  std::atomic<uint32_t> frequency_registers_{0x21717a};
  std::atomic<bool> packet_dump_mode_{false};
  bool packet_dump_pending_update_{false};
  std::atomic<uint8_t> radio_mode_{static_cast<uint8_t>(RadioMode::RX)};
  std::atomic<TxState> tx_state_{TxState::IDLE};
  std::atomic<uint32_t> tx_count_{0};
  uint32_t tx_state_entered_ms_{0}, last_tx_complete_ms_{0}, active_tx_transaction_id_{0};
  uint32_t rx_overflow_count_{0}, last_rx_overflow_ms_{0};
  uint32_t last_radio_check_ms_{0}, watchdog_window_start_ms_{0};
  uint8_t watchdog_flush_count_{0}, watchdog_reset_count_{0};
  std::atomic<uint32_t> watchdog_recovery_count_{0};
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
  bool illegal_fifo_read{false}, illegal_flush{false}, illegal_config_write{false}, stx_from_rx{true};
  bool ignore_idle{false}, reject_rx{false}, corrupt_patable{false}, calibrate_on_rx{false};
  bool enable_rx_error{false}, enable_rx_noop{false};
  uint8_t dropped_write{0xff};
  unsigned resets{0}, enable_rx_calls{0};
  std::array<uint8_t, 64> registers{};
  std::array<uint8_t, 8> patable{};
  size_t patable_index{0};
};
inline int xQueueSend(TestControlQueue *q, const RadioMessage *msg, uint32_t) {
  if (q->full) return 0;
  if (q->immediate) {
    const auto before = test_task; test_task = q->hub->radio_task_handle_;
    q->hub->handle_frequency_reinit_(msg->request_id, msg->freq.freq2, msg->freq.freq1, msg->freq.freq0);
    test_task = before;
  } else { q->messages.push_back(*msg); }
  return pdTRUE;
}
inline int16_t TestRadio::enableRxAfterTx(bool enable) {
  ++hub->enable_rx_calls;
  if (during_enable_rx) during_enable_rx();
  if (hub->enable_rx_error) return -16;
  if (!hub->enable_rx_noop)
    hub->write_reg(CC1101_MCSM1, (hub->read_reg(CC1101_MCSM1) & ~0x03) | (enable ? 0x03 : 0x00));
  return RADIOLIB_ERR_NONE;
}
}}
