#include "elero.h"
#include "elero_crypto.h"
#include "elero_utils.h"
#include "elero_watchdog_logic.h"
#include "elero_recovery_logic.h"
#include "elero_overflow_logic.h"
#include "elero_tx_logic.h"
#include "elero_dedup_logic.h"
#include "elero_radio_state_logic.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cstring>

namespace esphome {
namespace elero {

static const char *TAG = "elero";
static const uint8_t SPI_SETTLE_US = 5;

static const uint32_t TX_STATE_TIMEOUT_MS = 50;
static const uint32_t TX_COOLDOWN_MS = 1;  // CC1101 PLL settles in ~75µs; 1ms is ample margin
static const uint32_t RADIO_WATCHDOG_MS = 5000;
static const uint32_t WATCHDOG_ESCALATION_WINDOW_MS = 60000;  // 60s window for escalating recovery
static const uint8_t WATCHDOG_MAX_FLUSHES_PER_WINDOW = 3;     // L1: flush threshold before escalating to reset
static const uint8_t WATCHDOG_MAX_RESETS_PER_WINDOW = 3;      // L2: reset threshold before marking failed

static const char *marcstate_to_string(uint8_t marc) {
  switch (marc) {
    case CC1101_MARCSTATE_SLEEP: return "SLEEP";
    case CC1101_MARCSTATE_IDLE: return "IDLE";
    case CC1101_MARCSTATE_XOFF: return "XOFF";
    case CC1101_MARCSTATE_VCOON_MC: return "VCOON_MC";
    case CC1101_MARCSTATE_REGON_MC: return "REGON_MC";
    case CC1101_MARCSTATE_MANCAL: return "MANCAL";
    case CC1101_MARCSTATE_VCOON: return "VCOON";
    case CC1101_MARCSTATE_REGON: return "REGON";
    case CC1101_MARCSTATE_STARTCAL: return "STARTCAL";
    case CC1101_MARCSTATE_BWBOOST: return "BWBOOST";
    case CC1101_MARCSTATE_FS_LOCK: return "FS_LOCK";
    case CC1101_MARCSTATE_IFADCON: return "IFADCON";
    case CC1101_MARCSTATE_ENDCAL: return "ENDCAL";
    case CC1101_MARCSTATE_RX: return "RX";
    case CC1101_MARCSTATE_RX_END: return "RX_END";
    case CC1101_MARCSTATE_RX_RST: return "RX_RST";
    case CC1101_MARCSTATE_TXRX_SWITCH: return "TXRX_SWITCH";
    case CC1101_MARCSTATE_RXFIFO_OFLOW: return "RXFIFO_OFLOW";
    case CC1101_MARCSTATE_FSTXON: return "FSTXON";
    case CC1101_MARCSTATE_TX: return "TX";
    case CC1101_MARCSTATE_TX_END: return "TX_END";
    case CC1101_MARCSTATE_RXTX_SWITCH: return "RXTX_SWITCH";
    case CC1101_MARCSTATE_TXFIFO_UFLOW: return "TXFIFO_UFLOW";
    default: return "UNKNOWN";
  }
}

// ---------------------------------------------------------------------------
// process_rx — drain all available packets from the CC1101 RX FIFO
// ---------------------------------------------------------------------------
struct Elero::RxFifoIO {
  Elero &hub;
  uint32_t now() const { return millis(); }
  bool packet_active() const { return hub.gdo0_pin_->digital_read(); }
  bool enter_idle() {
    return hub.enter_idle_();
  }
  bool rx_bytes(uint8_t &value) { return hub.read_status_stable(CC1101_RXBYTES, value); }
  void read_fifo(uint8_t *bytes, uint8_t count) { hub.read_buf(CC1101_RXFIFO, bytes, count); }
  void resume_rx() { hub.write_cmd(CC1101_SRX); }
  void discard(const char *reason) {
    ESP_LOGW(TAG, "RX FIFO recovery: %s", reason);
    hub.increment_parser_drop_count(reason);
    if (strcmp(reason, "fifo_overflow") == 0) {
      const auto time = millis();
      hub.rx_overflow_count_ = overflow_logic::next_overflow_count(
          time, hub.last_rx_overflow_ms_, hub.rx_overflow_count_, 1000);
      hub.last_rx_overflow_ms_ = time;
    }
    hub.write_cmd(CC1101_SFRX);  // caller verified IDLE, never flush in RX
  }
  void packet(const uint8_t *bytes, uint8_t count, const RxMetadata &meta) {
    memcpy(hub.msg_rx_, bytes, count);
    hub.current_rx_meta_ = meta;
    hub.packet_dump_pending_update_ = false;
    if (hub.packet_dump_mode_.load(std::memory_order_acquire)) {
      hub.capture_raw_packet_(count);
      hub.packet_dump_pending_update_ = true;
    }
    hub.interpret_msg();
  }
};

bool Elero::process_rx(bool keep_idle) {
  if (this->radio_mode_.load(std::memory_order_relaxed) != static_cast<uint8_t>(RadioMode::RX))
    return false;
  // Poll the FIFO as well as the edge flag: several packet ends can coalesce
  // into one interrupt, including an RX end misrouted during TX completion.
  if (!keep_idle && !this->rx_ready_.load(std::memory_order_acquire) &&
      !this->gdo0_pin_->digital_read() && this->read_status(CC1101_RXBYTES) == 0)
    return true;
  this->rx_ready_.store(false, std::memory_order_release);
  RxFifoIO io{*this};
  const auto result = this->rx_fifo_.drain(io, this->rx_timeline_, keep_idle);
  if (overflow_logic::should_reinit_after_overflow_count(this->rx_overflow_count_, 5)) {
    this->rx_overflow_count_ = 0;
    this->reset();
    this->init();
    return false;
  }
  if (result == RxFifoReader::Result::RECEIVING || result == RxFifoReader::Result::IO_ERROR) {
    this->rx_ready_.store(true, std::memory_order_release);
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// advance_tx — non-blocking TX state machine (one step per call)
// ---------------------------------------------------------------------------
void Elero::advance_tx() {
  uint32_t now = millis();
  uint32_t elapsed = now - this->tx_state_entered_ms_;

  switch (this->tx_state_.load(std::memory_order_acquire)) {

    case TxState::CCA: {
      uint8_t marc = 0;
      const bool stable = this->read_status_stable(CC1101_MARCSTATE, marc);
      if (marc == CC1101_MARCSTATE_TXFIFO_UFLOW) {
        this->tx_abort_();
        break;
      }
      if (!stable) {
        if (this->cca_backoff_.expired(now)) this->tx_abort_();
        break;
      }
      marc &= 0x1f;
      // A transition just after the previous STX sample is not a CCA rejection.
      if (marc == CC1101_MARCSTATE_TX || marc == CC1101_MARCSTATE_RXTX_SWITCH ||
          marc == CC1101_MARCSTATE_TX_END) {
        this->tx_started_seen_ = marc != CC1101_MARCSTATE_RXTX_SWITCH;
        this->radio_mode_.store(static_cast<uint8_t>(RadioMode::TX), std::memory_order_relaxed);
        this->tx_state_.store(TxState::TRANSMITTING, std::memory_order_release);
        this->tx_state_entered_ms_ = now;
        break;
      }
      if (this->cca_backoff_.expired(now) || marc == CC1101_MARCSTATE_TXFIFO_UFLOW) {
        ESP_LOGW(TAG, "CCA budget exhausted or TX FIFO fault; delivery remains unconfirmed");
        this->tx_abort_();
        break;
      }
      if (!this->cca_backoff_.due(now)) break;
      if (this->gdo0_pin_->digital_read() || this->rx_ready_.load(std::memory_order_acquire) ||
          this->read_status(CC1101_RXBYTES) != 0) {
        if (this->process_rx()) this->cca_backoff_.listen(now);
        break;
      }
      if (marc != CC1101_MARCSTATE_RX) break;  // settling/calibration, bounded by attempt budget
      uint8_t packet_status = 0;
      if (!this->read_status_stable(CC1101_PKTSTATUS, packet_status)) break;
      if ((packet_status & 0x10) == 0) {
        this->cca_backoff_.busy(now);
        break;  // busy channel: remain RX, no FIFO flush and no blind SIDLE→STX
      }
      this->tx_done_.store(false, std::memory_order_release);
      this->radio_mode_.store(static_cast<uint8_t>(RadioMode::TX), std::memory_order_relaxed);
      this->write_cmd(CC1101_STX);  // MCSM1 CCA_MODE is effective only from RX
      marc = this->read_status(CC1101_MARCSTATE) & 0x1f;
      if (marc == CC1101_MARCSTATE_RX) {
        this->radio_mode_.store(static_cast<uint8_t>(RadioMode::RX), std::memory_order_relaxed);
        this->cca_backoff_.busy(now);  // hardware rechecked CCA; retain loaded TX bytes
      } else {
        this->tx_started_seen_ = marc == CC1101_MARCSTATE_TX || marc == CC1101_MARCSTATE_TX_END;
        this->tx_state_.store(TxState::TRANSMITTING, std::memory_order_release);
        this->tx_state_entered_ms_ = now;
      }
      break;
    }

    case TxState::TRANSMITTING: {
      uint8_t marc = 0, bytes = 0;
      const bool marc_stable = this->read_status_stable(CC1101_MARCSTATE, marc);
      const bool bytes_stable = this->read_status_stable(CC1101_TXBYTES, bytes);
      // Faults win over the ISR fast path and over apparently empty low bits.
      if (marc == CC1101_MARCSTATE_TXFIFO_UFLOW || (bytes & 0x80) != 0) {
        ESP_LOGE(TAG, "TX underflow: marc=0x%02x TXBYTES=0x%02x", marc, bytes);
        this->tx_abort_();
        break;
      }
      if (!marc_stable || !bytes_stable) {
        if (elapsed >= TX_STATE_TIMEOUT_MS) this->tx_abort_();
        break;
      }
      marc &= 0x1f;
      if (marc == CC1101_MARCSTATE_TX || marc == CC1101_MARCSTATE_TX_END)
        this->tx_started_seen_ = true;
      this->tx_done_.store(false, std::memory_order_release);  // hint, never motor ACK
      const auto observation = radio_state_logic::observe_tx(
          marc, bytes, this->tx_started_seen_, elapsed, TX_STATE_TIMEOUT_MS);
      if (observation == radio_state_logic::TxObservation::SUCCESS) {
        this->tx_count_.fetch_add(1, std::memory_order_relaxed);
        ESP_LOGD(TAG, "Local TX complete; motor delivery unconfirmed (%lums)", (unsigned long) elapsed);
        this->publish_tx_completion_(this->active_tx_transaction_id_, true);
        this->active_tx_transaction_id_ = 0;
        this->tx_state_.store(TxState::COOLDOWN, std::memory_order_release);
        this->tx_state_entered_ms_ = now;
        this->last_tx_complete_ms_ = now;
      } else if (observation == radio_state_logic::TxObservation::FAILURE) {
        ESP_LOGW(TAG, "Unproven/failed TX: marc=%s TXBYTES=0x%02x", marcstate_to_string(marc), bytes);
        this->tx_abort_();
      }
      break;
    }

    case TxState::COOLDOWN: {
      if (elapsed >= TX_COOLDOWN_MS) {
        this->radio_mode_.store(static_cast<uint8_t>(RadioMode::RX), std::memory_order_relaxed);
        this->tx_state_.store(TxState::IDLE, std::memory_order_release);
        uint8_t rxbytes = 0;
        const bool stable = this->read_status_stable(CC1101_RXBYTES, rxbytes);
        if (rxbytes & 0x80) {
          ESP_LOGW(TAG, "RX FIFO overflow detected after TX, flushing");
          this->flush_rx();
        } else if (!stable || (rxbytes & 0x7F) > 0) {
          this->rx_ready_.store(true, std::memory_order_release);
        }
        this->process_rx();  // rescue feedback before any next TX, including STOP
      }
      break;
    }

    case TxState::IDLE:
    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// tx_abort_ — force-reset radio to clean RX state after TX failure
// ---------------------------------------------------------------------------
void Elero::tx_abort_() {
  if (this->radio_mode_.load(std::memory_order_relaxed) == static_cast<uint8_t>(RadioMode::RX) &&
      (this->read_status(CC1101_MARCSTATE) & 0x1f) == CC1101_MARCSTATE_RX) {
    // A CCA refusal is not an RX fault. Leave any active reception alone; the
    // next preparation clears the unused TX bytes after safely draining RX.
    this->process_rx();
  } else {
    this->flush_and_rx();
  }
  this->publish_tx_completion_(this->active_tx_transaction_id_, false);
  this->active_tx_transaction_id_ = 0;
  this->radio_mode_.store(static_cast<uint8_t>(RadioMode::RX), std::memory_order_relaxed);
  this->tx_state_.store(TxState::IDLE, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// is_duplicate_packet_ — suppress relay-hop duplicates of the same packet
// ---------------------------------------------------------------------------
bool Elero::is_duplicate_packet_(uint32_t src, uint8_t cnt) {
  if (this->dedup_window_ms_ == 0)
    return false;
  uint32_t now = millis();
  uint64_t key = (static_cast<uint64_t>(src) << 8) | cnt;
  auto it = this->dedup_map_.find(key);
  if (it != this->dedup_map_.end() &&
      dedup_logic::is_duplicate_within_window(now, it->second, this->dedup_window_ms_)) {
    return true;  // already seen within window
  }
  this->dedup_map_[key] = now;
  return false;
}

void Elero::prune_dedup_map_() {
  if (this->dedup_window_ms_ == 0) {
    this->dedup_map_.clear();
    return;
  }
  uint32_t now = millis();
  if (now - this->last_dedup_prune_ms_ < RADIO_WATCHDOG_MS)
    return;
  this->last_dedup_prune_ms_ = now;
  for (auto it = this->dedup_map_.begin(); it != this->dedup_map_.end();) {
    if (dedup_logic::should_prune_entry(now, it->second, this->dedup_window_ms_))
      it = this->dedup_map_.erase(it);
    else
      ++it;
  }
}

// ---------------------------------------------------------------------------
// check_radio_state_ — periodic watchdog to detect and recover stuck CC1101
// ---------------------------------------------------------------------------
void Elero::check_radio_state_() {
  uint32_t now = millis();
  if (now - this->last_radio_check_ms_ < RADIO_WATCHDOG_MS)
    return;
  this->last_radio_check_ms_ = now;

  uint8_t marc = this->read_status(CC1101_MARCSTATE) & 0x1F;

  // Healthy RX state — reset escalation counters/window.
  if (radio_state_logic::is_watchdog_healthy_rx(marc)) {
    watchdog_logic::EscalationState state{
      this->watchdog_flush_count_,
      this->watchdog_reset_count_,
      this->watchdog_window_start_ms_
    };
    watchdog_logic::reset_on_healthy(state, now);
    this->watchdog_flush_count_ = state.flush_count;
    this->watchdog_reset_count_ = state.reset_count;
    this->watchdog_window_start_ms_ = state.window_start_ms;
    return;
  }
  // Transient calibration states — let them settle
  if (radio_state_logic::is_watchdog_transient_state(marc))
    return;

  // --- Something is wrong: escalating recovery ---
  this->watchdog_recovery_count_.fetch_add(1, std::memory_order_relaxed);

  // Reset escalation window if expired.
  {
    watchdog_logic::EscalationState state{
      this->watchdog_flush_count_,
      this->watchdog_reset_count_,
      this->watchdog_window_start_ms_
    };
    watchdog_logic::reset_if_window_expired(state, now, WATCHDOG_ESCALATION_WINDOW_MS);
    this->watchdog_flush_count_ = state.flush_count;
    this->watchdog_reset_count_ = state.reset_count;
    this->watchdog_window_start_ms_ = state.window_start_ms;
  }

  switch (watchdog_logic::choose_recovery_action(marc, this->watchdog_flush_count_,
                                                 this->watchdog_reset_count_,
                                                 WATCHDOG_MAX_FLUSHES_PER_WINDOW,
                                                 WATCHDOG_MAX_RESETS_PER_WINDOW)) {
    case watchdog_logic::RecoveryAction::RESTART_RX:
      ESP_LOGW(TAG, "Radio watchdog: stuck in IDLE, restarting RX");
      this->write_cmd(CC1101_SRX);
      return;

    case watchdog_logic::RecoveryAction::FLUSH_RX:
      this->watchdog_flush_count_++;
      ESP_LOGW(TAG, "Radio watchdog L1: RX FIFO overflow, flushing (%d/%d in window)",
               this->watchdog_flush_count_, WATCHDOG_MAX_FLUSHES_PER_WINDOW);
      this->flush_rx();
      return;

    case watchdog_logic::RecoveryAction::FLUSH_AND_RX:
      this->watchdog_flush_count_++;
      ESP_LOGW(TAG, "Radio watchdog L1: unexpected state %s (0x%02x), flushing (%d/%d in window)",
               marcstate_to_string(marc), marc,
               this->watchdog_flush_count_, WATCHDOG_MAX_FLUSHES_PER_WINDOW);
      this->flush_and_rx();
      return;

    case watchdog_logic::RecoveryAction::RESET:
      this->watchdog_reset_count_++;
      ESP_LOGW(TAG, "Radio watchdog L2: %d flushes exhausted, full reset (%d/%d in window)",
               WATCHDOG_MAX_FLUSHES_PER_WINDOW,
               this->watchdog_reset_count_, WATCHDOG_MAX_RESETS_PER_WINDOW);
      this->reset();
      if (!this->init()) {
        ESP_LOGW(TAG, "Radio watchdog L2: reinit failed after reset");
      }
      return;

    case watchdog_logic::RecoveryAction::FAIL:
      ESP_LOGE(TAG, "Radio watchdog L3: %d flushes + %d resets exhausted in 60s window — marking failed",
               WATCHDOG_MAX_FLUSHES_PER_WINDOW, WATCHDOG_MAX_RESETS_PER_WINDOW);
      ESP_LOGE(TAG, "  If GPIO12 is used for SPI MISO, it may be pulling VDD_SDIO to 1.8V at boot.");
      ESP_LOGE(TAG, "  Use non-strapping pins for SPI (e.g. CLK=18, MISO=19, MOSI=23).");
      this->spi_failed_.store(true, std::memory_order_release);
      this->radio_fatal_error_.store(true, std::memory_order_release);
      return;
  }
}

// ---------------------------------------------------------------------------
// CC1101 register access and initialization
// ---------------------------------------------------------------------------

float Elero::registers_to_mhz(uint8_t freq2, uint8_t freq1, uint8_t freq0) {
  return utils::registers_to_mhz(freq2, freq1, freq0);
}

bool Elero::reinit_frequency(uint8_t freq2, uint8_t freq1, uint8_t freq0) {
  if (!this->tx_queue_) return false;
  SemaphoreHandle_t sem = xSemaphoreCreateBinary();
  if (!sem) return false;
  bool result = false;
  RadioMessage msg{};
  msg.type = RadioControlType::REINIT_FREQ;
  msg.freq.freq2 = freq2;
  msg.freq.freq1 = freq1;
  msg.freq.freq0 = freq0;
  msg.completion_sem = sem;
  msg.result_ptr = &result;
  if (xQueueSend(this->tx_queue_, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
    vSemaphoreDelete(sem);
    return false;
  }
  xSemaphoreTake(sem, pdMS_TO_TICKS(5000));
  vSemaphoreDelete(sem);
  return result;
}

bool Elero::reinit_frequency_mhz(float mhz) {
  uint32_t freq_word = static_cast<uint32_t>(mhz * 65536.0f / 26.0f + 0.5f);
  uint8_t f2 = (freq_word >> 16) & 0xFF;
  uint8_t f1 = (freq_word >> 8) & 0xFF;
  uint8_t f0 = freq_word & 0xFF;
  return this->reinit_frequency(f2, f1, f0);
}

bool Elero::enter_idle_() {
  this->radio_->standby();
  uint8_t marc = 0;
  if (this->read_status_stable(CC1101_MARCSTATE, marc) && (marc & 0x1f) == CC1101_MARCSTATE_IDLE)
    return true;
  this->write_cmd(CC1101_SIDLE);
  delay_microseconds_safe(500);
  return this->read_status_stable(CC1101_MARCSTATE, marc) && (marc & 0x1f) == CC1101_MARCSTATE_IDLE;
}

void Elero::flush_and_rx() {
  ESP_LOGVV(TAG, "flush_and_rx");
  if (!this->enter_idle_()) {
    this->radio_fatal_error_.store(true, std::memory_order_release);
    return;  // SFRX/SFTX are illegal in an unverified live state
  }
  this->rx_fifo_.reset();
  this->write_cmd(CC1101_SFRX);
  this->write_cmd(CC1101_SFTX);
  this->write_cmd(CC1101_SRX);
  this->radio_mode_.store(static_cast<uint8_t>(RadioMode::RX), std::memory_order_relaxed);
  this->rx_ready_.store(false, std::memory_order_release);
}

void Elero::flush_rx() {
  ESP_LOGVV(TAG, "flush_rx");
  if (!this->enter_idle_()) {
    this->radio_fatal_error_.store(true, std::memory_order_release);
    return;
  }
  this->rx_fifo_.reset();
  this->write_cmd(CC1101_SFRX);
  this->write_cmd(CC1101_SRX);
  this->radio_mode_.store(static_cast<uint8_t>(RadioMode::RX), std::memory_order_relaxed);
  this->rx_ready_.store(false, std::memory_order_release);
}

void Elero::reset() {
  this->rx_fifo_.reset();
  this->enable();
  this->transfer_byte(CC1101_SRES);
  delay_microseconds_safe(5000);
  this->transfer_byte(CC1101_SIDLE);
  delay_microseconds_safe(100);
  this->disable();
}

// ---------------------------------------------------------------------------
// verify_spi_write_ — complementary pattern write-readback test (0xAA/0x55)
// Detects stuck MOSI bits before init() writes all CC1101 registers.
// ---------------------------------------------------------------------------
bool Elero::verify_spi_write_() {
  bool ok = true;
  const uint8_t patterns[] = {0xAA, 0x55};  // complementary: every bit toggles

  for (uint8_t pattern : patterns) {
    this->write_reg(CC1101_FSCTRL1, pattern);
    uint8_t readback = this->read_reg(CC1101_FSCTRL1);
    if (readback != pattern) {
      ESP_LOGD(TAG, "SPI verify: wrote 0x%02x to FSCTRL1, read back 0x%02x — MISMATCH (stuck bits: 0x%02x)",
               pattern, readback, static_cast<uint8_t>(pattern ^ readback));
      ok = false;
    } else {
      ESP_LOGD(TAG, "SPI verify: wrote 0x%02x to FSCTRL1, read back 0x%02x — OK", pattern, readback);
    }
  }

  // Restore register to clean state for subsequent init
  this->write_reg(CC1101_FSCTRL1, 0x00);
  return ok;
}

// ---------------------------------------------------------------------------
// diagnose_spi_failure_ — actionable error messages for common SPI wiring
// problems. Called when init() has failed after all retries.
// ---------------------------------------------------------------------------
void Elero::diagnose_spi_failure_() {
  uint8_t partnum = this->read_status(CC1101_PARTNUM);
  uint8_t version = this->read_status(CC1101_VERSION);
  ESP_LOGE(TAG, "CC1101 SPI diagnostic: PARTNUM=0x%02x (expect 0x00), VERSION=0x%02x (expect 0x14)", partnum, version);

  // Write-readback tests on a writable register
  uint8_t all_readback_or = 0x00;
  uint8_t all_readback_and = 0xFF;
  const uint8_t test_vals[] = {0xAA, 0x55, 0x0F, 0x00};
  for (uint8_t tv : test_vals) {
    this->write_reg(CC1101_FSCTRL1, tv);
    uint8_t rb = this->read_reg(CC1101_FSCTRL1);
    ESP_LOGE(TAG, "  SPI write/read test: wrote 0x%02x, read 0x%02x %s", tv, rb, (tv == rb) ? "OK" : "MISMATCH");
    all_readback_or |= rb;
    all_readback_and &= rb;
  }

  ESP_LOGE(TAG, "CC1101 SPI communication is broken — the radio is non-functional.");

  if (partnum == 0x00 && version == 0x00 && all_readback_or == 0x00) {
    // Every read returns 0x00 — MISO line is stuck low
    ESP_LOGE(TAG, "  SPI returns all zeros — MISO is stuck LOW.");
    ESP_LOGE(TAG, "  Check: MISO wiring, CS not reaching the CC1101, or chip held in reset.");
  } else if (partnum == 0xFF && version == 0xFF && all_readback_and == 0xFF) {
    // Every read returns 0xFF — MISO line is stuck high (chip not powered)
    ESP_LOGE(TAG, "  SPI returns all ones — MISO is stuck HIGH.");
    ESP_LOGE(TAG, "  Check: CC1101 VCC/GND power connections and that the module is seated properly.");
  } else if (partnum == 0x00 && version == 0x14) {
    // Chip ID is correct but register writes fail — MOSI issue
    ESP_LOGE(TAG, "  CC1101 chip IS detected (PARTNUM/VERSION correct) but register writes fail.");
    ESP_LOGE(TAG, "  Check: MOSI wiring or SPI bus conflict with another device on the same bus.");
  } else {
    ESP_LOGE(TAG, "  Unexpected chip ID — verify the radio module is a CC1101 on this SPI bus.");
  }

  ESP_LOGE(TAG, "  Verify SPI wiring: CLK, MOSI, MISO, CS must match your board schematic.");
  ESP_LOGE(TAG, "  Avoid ESP32 strapping pins (GPIO0, GPIO2, GPIO5, GPIO12, GPIO15) for SPI signals.");

  this->spi_failed_.store(true, std::memory_order_release);
  this->mark_failed(LOG_STR("CC1101 SPI communication broken — check pin assignments"));
}

bool Elero::init() {
  const uint8_t max_spi_retries = 5;
  bool spi_ok = false;
  for (uint8_t attempt = 1; attempt <= max_spi_retries; attempt++) {
    this->write_reg(CC1101_FSCTRL1, 0x08);
    uint8_t check = this->read_reg(CC1101_FSCTRL1);
    if (check == 0x08) {
      spi_ok = true;
      if (attempt > 1) {
        ESP_LOGI(TAG, "init: SPI health check passed on attempt %d/%d", attempt, max_spi_retries);
      }
      break;
    }
    if (attempt < max_spi_retries) {
      uint32_t delay_us = 2000u * (1u << (attempt - 1));
      ESP_LOGW(TAG, "init: SPI health check attempt %d/%d failed (wrote 0x08, read 0x%02x), retrying in %lu us",
               attempt, max_spi_retries, check, static_cast<unsigned long>(delay_us));
      delay_microseconds_safe(delay_us);
    } else {
      ESP_LOGE(TAG, "init: SPI health check failed after %d attempts (wrote 0x08, read 0x%02x) — aborting init",
               max_spi_retries, check);
    }
  }
  if (!spi_ok) {
    return false;
  }

  uint8_t patable_data[] = {0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0};

  this->write_reg(CC1101_FSCTRL0, 0x00);
  this->write_reg(CC1101_FREQ2, this->freq2_);
  this->write_reg(CC1101_FREQ1, this->freq1_);
  this->write_reg(CC1101_FREQ0, this->freq0_);
  this->write_reg(CC1101_MDMCFG4, 0x7B);
  this->write_reg(CC1101_MDMCFG3, 0x83);
  this->write_reg(CC1101_MDMCFG2, 0x13);
  this->write_reg(CC1101_MDMCFG1, 0x52);
  this->write_reg(CC1101_MDMCFG0, 0xF8);
  this->write_reg(CC1101_CHANNR, 0x00);
  this->write_reg(CC1101_DEVIATN, 0x43);
  this->write_reg(CC1101_FREND1, 0xB6);
  this->write_reg(CC1101_FREND0, 0x10);
  this->write_reg(CC1101_MCSM0, 0x18);
  this->write_reg(CC1101_MCSM1, 0x3F);
  this->write_reg(CC1101_FOCCFG, 0x1D);
  this->write_reg(CC1101_BSCFG, 0x1F);
  this->write_reg(CC1101_AGCCTRL2, 0xC7);
  this->write_reg(CC1101_AGCCTRL1, 0x00);
  this->write_reg(CC1101_AGCCTRL0, 0xB2);
  this->write_reg(CC1101_FSCAL3, 0xEA);
  this->write_reg(CC1101_FSCAL2, 0x2A);
  this->write_reg(CC1101_FSCAL1, 0x00);
  this->write_reg(CC1101_FSCAL0, 0x1F);
  this->write_reg(CC1101_FSTEST, 0x59);
  this->write_reg(CC1101_TEST2, 0x81);
  this->write_reg(CC1101_TEST1, 0x35);
  this->write_reg(CC1101_TEST0, 0x09);
  this->write_reg(CC1101_IOCFG0, 0x06);
  this->write_reg(CC1101_PKTCTRL1, 0x8C);
  this->write_reg(CC1101_PKTCTRL0, 0x45);
  this->write_reg(CC1101_ADDR, 0x00);
  this->write_reg(CC1101_PKTLEN, 0x3C);
  this->write_reg(CC1101_SYNC1, 0xD3);
  this->write_reg(CC1101_SYNC0, 0x91);
  this->write_burst(CC1101_PATABLE, patable_data, 8);

  this->write_cmd(CC1101_SRX);
  if (!this->wait_rx()) {
    ESP_LOGW(TAG, "init: CC1101 failed to enter RX after configuration");
    return false;
  }
  return true;
}

void Elero::write_reg(uint8_t addr, uint8_t data) {
  this->enable();
  this->write_byte(addr);
  this->write_byte(data);
  this->disable();
  delay_microseconds_safe(SPI_SETTLE_US);
}

void Elero::write_burst(uint8_t addr, uint8_t *data, uint8_t len) {
  this->enable();
  this->write_byte(addr | CC1101_WRITE_BURST);
  for (uint8_t i = 0; i < len; i++)
    this->write_byte(data[i]);
  this->disable();
  delay_microseconds_safe(SPI_SETTLE_US);
}

void Elero::write_cmd(uint8_t cmd) {
  this->enable();
  this->transfer_byte(cmd);
  this->disable();
  delay_microseconds_safe(SPI_SETTLE_US);
}

bool Elero::wait_rx() {
  ESP_LOGVV(TAG, "wait_rx");
  uint8_t timeout = 200;
  while ((this->read_status(CC1101_MARCSTATE) != CC1101_MARCSTATE_RX) && (--timeout != 0)) {
    delay_microseconds_safe(200);
  }

  if(timeout > 0)
    return true;
  ESP_LOGE(TAG, "Timed out waiting for RX: 0x%02x", this->read_status(CC1101_MARCSTATE));
  return false;
}

uint8_t Elero::read_reg(uint8_t addr) {
  uint8_t data;
  this->enable();
  this->write_byte(addr | CC1101_READ_SINGLE);
  data = this->read_byte();
  this->disable();
  delay_microseconds_safe(SPI_SETTLE_US);
  return data;
}

uint8_t Elero::read_status(uint8_t addr) {
  if (addr != CC1101_MARCSTATE && addr != CC1101_RXBYTES && addr != CC1101_TXBYTES &&
      addr != CC1101_PKTSTATUS)
    return this->read_status_once_(addr);
  uint8_t value = 0;
  return this->read_status_stable(addr, value) ? value : 0xff;  // fail closed
}

bool Elero::read_status_stable(uint8_t addr, uint8_t &value) {
  return read_stable_status([this, addr]() { return this->read_status_once_(addr); }, value,
      addr == CC1101_RXBYTES || addr == CC1101_TXBYTES ? 0x80 : 0,
      addr == CC1101_MARCSTATE ? CC1101_MARCSTATE_TXFIFO_UFLOW : 0xff);
}

uint8_t Elero::read_status_once_(uint8_t addr) {
  this->enable();
  this->transfer_byte(addr | CC1101_READ_BURST);
  uint8_t data = this->transfer_byte(0x00);
  this->disable();
  delay_microseconds_safe(SPI_SETTLE_US);
  return data;
}

void Elero::read_buf(uint8_t addr, uint8_t *buf, uint8_t len) {
  this->enable();
  this->transfer_byte(addr | CC1101_READ_BURST);
  for (uint8_t i = 0; i < len; i++)
    buf[i] = this->transfer_byte(0x00);
  this->disable();
  delay_microseconds_safe(SPI_SETTLE_US);
}

// ---------------------------------------------------------------------------
// send_command_internal_ — Core 0 only: execute TX via SPI
// ---------------------------------------------------------------------------
bool Elero::send_command_internal_(t_elero_command *cmd, uint32_t enqueued_at_ms) {
  if (this->spi_failed_.load(std::memory_order_acquire))
    return false;
  // Note: caller (radio_task_loop_) guarantees tx_state_ == IDLE before
  // dequeuing a TX_COMMAND, so no idle check needed here.

  ESP_LOGVV(TAG, "send_command called");
  uint8_t requested_num_dests = cmd->num_dests;
  uint8_t num_dests = tx_logic::sanitize_num_dests(requested_num_dests, ELERO_MAX_DESTS);
  if (num_dests != requested_num_dests) {
    ESP_LOGW(TAG, "Invalid num_dests=%d, sanitized to %d", requested_num_dests, num_dests);
  }

  uint8_t available_dests = tx_logic::count_nonzero_dest_addrs(cmd->dest_addrs, ELERO_MAX_DESTS);
  if (available_dests == 0) {
    ESP_LOGE(TAG, "No valid destination address available for TX command");
    return false;
  }
  uint8_t effective_dests = tx_logic::effective_num_dests(num_dests, available_dests);
  if (effective_dests != num_dests) {
    ESP_LOGW(TAG, "Requested %u destinations but only %u populated, clamping",
             num_dests, available_dests);
  }
  num_dests = effective_dests;
  uint16_t msg_len = tx_logic::calculate_msg_len(num_dests);
  if (!tx_logic::is_msg_len_valid(msg_len, ELERO_MAX_PACKET_SIZE)) {
    ESP_LOGE(TAG, "Invalid TX packet length %u for num_dests=%u", static_cast<unsigned>(msg_len), num_dests);
    return false;
  }

  uint16_t code = (0x00 - (cmd->counter * ELERO_CRYPTO_MULT)) & ELERO_CRYPTO_MASK;
  this->msg_tx_[0] = static_cast<uint8_t>(msg_len);
  this->msg_tx_[1] = cmd->counter;
  this->msg_tx_[2] = cmd->pck_inf[0];
  this->msg_tx_[3] = cmd->pck_inf[1];
  this->msg_tx_[4] = cmd->hop;
  this->msg_tx_[5] = ELERO_SYS_ADDR;
  this->msg_tx_[6] = cmd->channel;
  this->msg_tx_[7] = ((cmd->remote_addr >> 16) & 0xff);
  this->msg_tx_[8] = ((cmd->remote_addr >> 8) & 0xff);
  this->msg_tx_[9] = ((cmd->remote_addr) & 0xff);
  this->msg_tx_[10] = ((cmd->remote_addr >> 16) & 0xff);
  this->msg_tx_[11] = ((cmd->remote_addr >> 8) & 0xff);
  this->msg_tx_[12] = ((cmd->remote_addr) & 0xff);
  this->msg_tx_[13] = ((cmd->remote_addr >> 16) & 0xff);
  this->msg_tx_[14] = ((cmd->remote_addr >> 8) & 0xff);
  this->msg_tx_[15] = ((cmd->remote_addr) & 0xff);
  this->msg_tx_[16] = num_dests;
  // Write destination addresses (3 bytes each, big-endian)
  for (uint8_t i = 0; i < num_dests; i++) {
    uint32_t dest = cmd->dest_addrs[i];
    this->msg_tx_[17 + i * 3] = (dest >> 16) & 0xff;
    this->msg_tx_[18 + i * 3] = (dest >> 8) & 0xff;
    this->msg_tx_[19 + i * 3] = dest & 0xff;
  }
  // Payload starts after destination addresses
  uint8_t pld_off = 17 + num_dests * 3;
  for (int i = 0; i < 10; i++)
    this->msg_tx_[pld_off + i] = cmd->payload[i];
  // Crypto code overwrites payload[2..3]
  this->msg_tx_[pld_off + 2] = ((code >> 8) & 0xff);
  this->msg_tx_[pld_off + 3] = (code & 0xff);

  uint8_t *payload = &this->msg_tx_[pld_off + 2];
  crypto::msg_encode(payload);

  if (num_dests == 1) {
    ESP_LOGD(TAG, "send to 0x%06lx: cmd=0x%02x ch=%02d cnt=%02d",
             static_cast<unsigned long>(cmd->dest_addrs[0]), cmd->payload[4], cmd->channel, cmd->counter);
  } else {
    ESP_LOGD(TAG, "send group (%d dests): cmd=0x%02x ch=%02d cnt=%02d",
             num_dests, cmd->payload[4], cmd->channel, cmd->counter);
  }
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  ESP_LOGV(TAG, "  TX raw [%d bytes]: %s", static_cast<int>(msg_len + 1),
           format_hex_pretty(this->msg_tx_, static_cast<uint8_t>(msg_len + 1)).c_str());
#endif

  // Drain complete feedback first and acquire verified IDLE without consuming
  // an in-flight CRC-autoflush prefix. Normal preparation never flushes RX.
  if (!this->process_rx(true)) return false;
  this->write_cmd(CC1101_SFTX);
  this->write_burst(CC1101_TXFIFO, this->msg_tx_, this->msg_tx_[0] + 1);
  this->write_cmd(CC1101_SRX);
  this->tx_started_seen_ = false;
  this->tx_done_.store(false, std::memory_order_release);
  this->radio_mode_.store(static_cast<uint8_t>(RadioMode::RX), std::memory_order_relaxed);
  this->cca_backoff_.start(millis(), cmd->remote_addr ^ cmd->counter ^ millis());
  this->observe_tx_queue_latency_(enqueued_at_ms);
  this->tx_state_.store(TxState::CCA, std::memory_order_release);
  this->tx_state_entered_ms_ = millis();
  return true;
}

}  // namespace elero
}  // namespace esphome
