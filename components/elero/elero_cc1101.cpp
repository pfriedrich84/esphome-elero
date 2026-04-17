#include "elero.h"
#include "elero_crypto.h"
#include "elero_utils.h"
#include "elero_watchdog_logic.h"
#include "elero_recovery_logic.h"
#include "elero_overflow_logic.h"
#include "elero_tx_logic.h"
#include "elero_dedup_logic.h"
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
void Elero::process_rx() {
  // Guard: only process RX when radio is actually in RX mode
  if (this->radio_mode_.load(std::memory_order_relaxed) != static_cast<uint8_t>(RadioMode::RX))
    return;
  if (!this->rx_ready_.load(std::memory_order_acquire))
    return;
  this->rx_ready_.store(false, std::memory_order_release);

  // When TX commands are pending, limit RX drain to 1 packet per call so
  // the next radio_task_loop_() iteration can dequeue TX first (TX priority).
  // When TX idle, drain up to ELERO_MAX_RX_PER_LOOP to clear the FIFO fast.
  uint8_t max_drain = ELERO_MAX_RX_PER_LOOP;
  if ((this->tx_priority_queue_ && uxQueueMessagesWaiting(this->tx_priority_queue_) > 0) ||
      (this->tx_queue_ && uxQueueMessagesWaiting(this->tx_queue_) > 0)) {
    max_drain = 1;
  }
  for (uint8_t iter = 0; iter < max_drain; iter++) {
    uint8_t len = this->read_status(CC1101_RXBYTES);

    if (len & 0x80) {  // overflow bit set — FIFO data unreliable
      uint32_t now = millis();
      this->rx_overflow_count_ = overflow_logic::next_overflow_count(
          now, this->last_rx_overflow_ms_, this->rx_overflow_count_, 1000);

      // After rapid repeated overflows, escalate to full radio reinit.
      if (overflow_logic::should_reinit_after_overflow_count(this->rx_overflow_count_, 5)) {
        ESP_LOGW(TAG, "RX FIFO overflow persists after %d flushes, full radio reinit",
                 this->rx_overflow_count_);
        this->rx_overflow_count_ = 0;
        this->last_rx_overflow_ms_ = now;
        this->reset();
        this->init();
        return;
      }

      // For rapid repeated overflows, suppress repetitive flushes and let the
      // counter decide when to escalate.
      if (this->rx_overflow_count_ > 1) {
        return;
      }

      this->last_rx_overflow_ms_ = now;
      ESP_LOGW(TAG, "RX FIFO overflow in process_rx, flushing");
      this->flush_rx();
      return;
    }

    uint8_t avail = len & 0x7F;
    if (avail == 0)
      return;  // FIFO empty — done

    uint8_t fifo_count;
    if (avail > CC1101_FIFO_LENGTH) {
      ESP_LOGV(TAG, "Received more bytes than FIFO length");
      this->read_buf(CC1101_RXFIFO, this->msg_rx_, CC1101_FIFO_LENGTH);
      fifo_count = CC1101_FIFO_LENGTH;
    } else {
      fifo_count = avail;
      this->read_buf(CC1101_RXFIFO, this->msg_rx_, fifo_count);
    }

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    ESP_LOGV(TAG, "RAW RX %d bytes: %s", fifo_count,
             format_hex_pretty(this->msg_rx_, fifo_count).c_str());
#endif

    // Capture to ring buffer if dump mode is active
    this->packet_dump_pending_update_ = false;
    if (this->packet_dump_mode_.load(std::memory_order_acquire)) {
      this->capture_raw_packet_(fifo_count);
      this->packet_dump_pending_update_ = true;
    }

    // Sanity check: first byte is the payload length; we need at least
    // payload + length byte + 2 appended status bytes (RSSI + LQI).
    if (this->msg_rx_[0] + 3 <= fifo_count) {
      this->interpret_msg();
    } else if (this->packet_dump_pending_update_) {
      this->mark_last_raw_packet_(false, "short_read");
      this->packet_dump_pending_update_ = false;
    }
  }
}

// ---------------------------------------------------------------------------
// advance_tx — non-blocking TX state machine (one step per call)
// ---------------------------------------------------------------------------
void Elero::advance_tx() {
  uint32_t now = millis();
  uint32_t elapsed = now - this->tx_state_entered_ms_;

  switch (this->tx_state_.load(std::memory_order_acquire)) {

    case TxState::TRANSMITTING: {
      // Fast path: ISR signalled TX completion via tx_done_ flag
      bool isr_done = this->tx_done_.load(std::memory_order_acquire);
      uint8_t marc = this->read_status(CC1101_MARCSTATE) & 0x1F;

      if (isr_done) {
        this->tx_done_.store(false, std::memory_order_release);
        // Verify TX FIFO is drained
        uint8_t bytes = this->read_status(CC1101_TXBYTES) & 0x7F;
        if (bytes == 0) {
          this->tx_count_.fetch_add(1, std::memory_order_relaxed);
          ESP_LOGV(TAG, "TX complete via ISR (marc=%s, %lums)",
                   marcstate_to_string(marc), (unsigned long) elapsed);
          this->tx_state_.store(TxState::COOLDOWN, std::memory_order_release);
          this->tx_state_entered_ms_ = now;
          this->last_tx_complete_ms_ = now;
        } else {
          ESP_LOGW(TAG, "TX ISR fired but %d bytes still in FIFO (marc=%s), aborting",
                   bytes, marcstate_to_string(marc));
          this->tx_abort_();
        }
        break;
      }

      // Fallback: poll MARCSTATE for TX completion
      bool tx_in_progress = (marc == CC1101_MARCSTATE_TX) ||
                            (marc == CC1101_MARCSTATE_STARTCAL) ||
                            (marc == CC1101_MARCSTATE_BWBOOST) ||
                            (marc == CC1101_MARCSTATE_FS_LOCK) ||
                            (marc == CC1101_MARCSTATE_IFADCON) ||
                            (marc == CC1101_MARCSTATE_ENDCAL) ||
                            (marc == CC1101_MARCSTATE_FSTXON) ||
                            (marc == CC1101_MARCSTATE_RXTX_SWITCH);
      if (tx_in_progress) {
        if (elapsed > TX_STATE_TIMEOUT_MS) {
          ESP_LOGW(TAG, "TX timeout in TRANSMITTING (marc=%s, %lums), aborting",
                   marcstate_to_string(marc), (unsigned long) elapsed);
          this->tx_abort_();
        }
      } else if (marc == CC1101_MARCSTATE_TXFIFO_UFLOW) {
        ESP_LOGE(TAG, "TX FIFO underflow");
        this->tx_abort_();
      } else {
        uint8_t bytes = this->read_status(CC1101_TXBYTES) & 0x7F;
        if (bytes == 0) {
          this->tx_count_.fetch_add(1, std::memory_order_relaxed);
          ESP_LOGV(TAG, "TX complete (marc=%s, %lums)",
                   marcstate_to_string(marc), (unsigned long) elapsed);
          this->tx_state_.store(TxState::COOLDOWN, std::memory_order_release);
          this->tx_state_entered_ms_ = now;
          this->last_tx_complete_ms_ = now;
        } else {
          ESP_LOGW(TAG, "TX failed: %d bytes still in FIFO (marc=%s) — likely CCA rejection, will retry",
                   bytes, marcstate_to_string(marc));
          this->tx_abort_();
        }
      }
      break;
    }

    case TxState::COOLDOWN: {
      if (elapsed >= TX_COOLDOWN_MS) {
        this->radio_mode_.store(static_cast<uint8_t>(RadioMode::RX), std::memory_order_relaxed);
        this->tx_state_.store(TxState::IDLE, std::memory_order_release);
        uint8_t rxbytes = this->read_status(CC1101_RXBYTES);
        if (rxbytes & 0x80) {
          ESP_LOGW(TAG, "RX FIFO overflow detected after TX, flushing");
          this->flush_rx();
        } else if ((rxbytes & 0x7F) > 0) {
          this->rx_ready_.store(true, std::memory_order_release);
        }
        // Don't call process_rx() here — let the next radio_task_loop_()
        // iteration check the TX queue first (step 1) before processing
        // RX (step 2).  This ensures TX always has priority after cooldown.
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
  this->flush_and_rx();
  this->radio_mode_.store(static_cast<uint8_t>(RadioMode::RX), std::memory_order_relaxed);
  this->tx_state_.store(TxState::IDLE, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// is_duplicate_packet_ — suppress relay-hop duplicates of the same packet
// ---------------------------------------------------------------------------
bool Elero::is_duplicate_packet_(uint32_t src, uint8_t cnt) {
  uint32_t now = millis();
  uint64_t key = (static_cast<uint64_t>(src) << 8) | cnt;
  auto it = this->dedup_map_.find(key);
  if (it != this->dedup_map_.end() &&
      dedup_logic::is_duplicate_within_window(now, it->second, ELERO_DEDUP_WINDOW_MS)) {
    return true;  // already seen within window
  }
  this->dedup_map_[key] = now;
  return false;
}

void Elero::prune_dedup_map_() {
  uint32_t now = millis();
  if (now - this->last_dedup_prune_ms_ < RADIO_WATCHDOG_MS)
    return;
  this->last_dedup_prune_ms_ = now;
  for (auto it = this->dedup_map_.begin(); it != this->dedup_map_.end();) {
    if (dedup_logic::should_prune_entry(now, it->second, ELERO_DEDUP_WINDOW_MS))
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
  if (marc == CC1101_MARCSTATE_RX) {
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
  if (marc >= CC1101_MARCSTATE_VCOON_MC && marc <= CC1101_MARCSTATE_ENDCAL)
    return;
  if (marc == CC1101_MARCSTATE_RX_END || marc == CC1101_MARCSTATE_RX_RST)
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

  // Stuck IDLE — simple SRX restart (Level 0, doesn't count toward escalation)
  if (marc == CC1101_MARCSTATE_IDLE) {
    ESP_LOGW(TAG, "Radio watchdog: stuck in IDLE, restarting RX");
    this->write_cmd(CC1101_SRX);
    return;
  }

  // Level 1: flush FIFO
  if (this->watchdog_flush_count_ < WATCHDOG_MAX_FLUSHES_PER_WINDOW) {
    this->watchdog_flush_count_++;
    if (marc == CC1101_MARCSTATE_RXFIFO_OFLOW) {
      ESP_LOGW(TAG, "Radio watchdog L1: RX FIFO overflow, flushing (%d/%d in window)",
               this->watchdog_flush_count_, WATCHDOG_MAX_FLUSHES_PER_WINDOW);
      this->flush_rx();
    } else {
      ESP_LOGW(TAG, "Radio watchdog L1: unexpected state %s (0x%02x), flushing (%d/%d in window)",
               marcstate_to_string(marc), marc,
               this->watchdog_flush_count_, WATCHDOG_MAX_FLUSHES_PER_WINDOW);
      this->flush_and_rx();
    }
    return;
  }

  // Level 2: full chip reset
  if (this->watchdog_reset_count_ < WATCHDOG_MAX_RESETS_PER_WINDOW) {
    this->watchdog_reset_count_++;
    ESP_LOGW(TAG, "Radio watchdog L2: %d flushes exhausted, full reset (%d/%d in window)",
             WATCHDOG_MAX_FLUSHES_PER_WINDOW,
             this->watchdog_reset_count_, WATCHDOG_MAX_RESETS_PER_WINDOW);
    this->reset();
    if (!this->init()) {
      ESP_LOGW(TAG, "Radio watchdog L2: reinit failed after reset");
    }
    return;
  }

  // Level 3: mark permanently failed
  ESP_LOGE(TAG, "Radio watchdog L3: %d flushes + %d resets exhausted in 60s window — marking failed",
           WATCHDOG_MAX_FLUSHES_PER_WINDOW, WATCHDOG_MAX_RESETS_PER_WINDOW);
  ESP_LOGE(TAG, "  If GPIO12 is used for SPI MISO, it may be pulling VDD_SDIO to 1.8V at boot.");
  ESP_LOGE(TAG, "  Use non-strapping pins for SPI (e.g. CLK=18, MISO=19, MOSI=23).");
  this->spi_failed_.store(true, std::memory_order_release);
  this->radio_fatal_error_.store(true, std::memory_order_release);
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

void Elero::flush_and_rx() {
  ESP_LOGVV(TAG, "flush_and_rx");
  this->radio_->standby();
  uint8_t marc = this->read_status(CC1101_MARCSTATE) & 0x1F;
  if (marc != CC1101_MARCSTATE_IDLE) {
    this->write_cmd(CC1101_SIDLE);
    delay_microseconds_safe(500);
  }
  this->write_cmd(CC1101_SFRX);
  this->write_cmd(CC1101_SFTX);
  this->write_cmd(CC1101_SRX);
  this->radio_mode_.store(static_cast<uint8_t>(RadioMode::RX), std::memory_order_relaxed);
  this->rx_ready_.store(false, std::memory_order_release);
}

void Elero::flush_rx() {
  ESP_LOGVV(TAG, "flush_rx");
  this->radio_->standby();
  uint8_t marc = this->read_status(CC1101_MARCSTATE) & 0x1F;
  if (marc != CC1101_MARCSTATE_IDLE) {
    this->write_cmd(CC1101_SIDLE);
    delay_microseconds_safe(500);
  }
  this->write_cmd(CC1101_SFRX);
  this->write_cmd(CC1101_SRX);
  this->radio_mode_.store(static_cast<uint8_t>(RadioMode::RX), std::memory_order_relaxed);
  this->rx_ready_.store(false, std::memory_order_release);
}

void Elero::reset() {
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
      ESP_LOGW(TAG, "init: SPI health check attempt %d/%d failed (wrote 0x08, read 0x%02x), retrying in %u us",
               attempt, max_spi_retries, check, delay_us);
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
bool Elero::send_command_internal_(t_elero_command *cmd) {
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
    ESP_LOGD(TAG, "send to 0x%06x: cmd=0x%02x ch=%02d cnt=%02d",
             cmd->dest_addrs[0], cmd->payload[4], cmd->channel, cmd->counter);
  } else {
    ESP_LOGD(TAG, "send group (%d dests): cmd=0x%02x ch=%02d cnt=%02d",
             num_dests, cmd->payload[4], cmd->channel, cmd->counter);
  }
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  ESP_LOGV(TAG, "  TX raw [%d bytes]: %s", static_cast<int>(msg_len + 1),
           format_hex_pretty(this->msg_tx_, static_cast<uint8_t>(msg_len + 1)).c_str());
#endif

  this->radio_->standby();

  uint8_t marc = this->read_status(CC1101_MARCSTATE) & 0x1F;
  if (marc != CC1101_MARCSTATE_IDLE) {
    this->send_cmd_reinit_failures_ = recovery_logic::next_reinit_failure_count(
        this->send_cmd_reinit_failures_, false);
    if (this->send_cmd_reinit_failures_ >= 3) {
      if (!this->spi_failed_.load(std::memory_order_acquire)) {
        ESP_LOGE(TAG, "send_command: %d consecutive reinit failures — SPI appears permanently broken.",
                 this->send_cmd_reinit_failures_);
        ESP_LOGE(TAG, "  If GPIO12 is used for SPI MISO, it may be pulling VDD_SDIO to 1.8V at boot.");
        ESP_LOGE(TAG, "  Use non-strapping pins for SPI (e.g. CLK=18, MISO=19, MOSI=23).");
        this->spi_failed_.store(true, std::memory_order_release);
        this->radio_fatal_error_.store(true, std::memory_order_release);
      }
      return false;
    }
    ESP_LOGW(TAG, "send_command: radio not in IDLE (marc=0x%02x), reinitializing (%d/%d)",
             marc, this->send_cmd_reinit_failures_, 3);
    this->reset();
    bool init_ok = this->init();
    this->send_cmd_reinit_failures_ =
        recovery_logic::apply_reinit_outcome(this->send_cmd_reinit_failures_, init_ok);
    return false;
  }
  this->send_cmd_reinit_failures_ = 0;

  this->enable();
  this->transfer_byte(CC1101_SFTX);
  this->disable();
  this->enable();
  this->transfer_byte(CC1101_SFRX);
  this->disable();
  delay_microseconds_safe(SPI_SETTLE_US);

  // Switch to TX mode BEFORE loading FIFO and strobing STX.
  // The ISR will route GDO0 signals to tx_done_ while in TX mode,
  // preserving any rx_ready_ flag that was set before this point.
  this->radio_mode_.store(static_cast<uint8_t>(RadioMode::TX), std::memory_order_relaxed);
  this->tx_done_.store(false, std::memory_order_release);

  delay_microseconds_safe(20);
  this->write_burst(CC1101_TXFIFO, this->msg_tx_, this->msg_tx_[0] + 1);
  this->write_cmd(CC1101_STX);

  this->tx_state_.store(TxState::TRANSMITTING, std::memory_order_release);
  this->tx_state_entered_ms_ = millis();
  return true;
}

}  // namespace elero
}  // namespace esphome
