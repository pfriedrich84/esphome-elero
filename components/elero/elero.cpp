#include "elero.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif
#include <cstring>
#include <algorithm>

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome {
namespace elero {

#ifdef USE_LOGGER
// Static callback forwarding ESPHome log messages into the Elero ring buffer
// for the web UI.  Uses the add_log_callback() API (ESPHome 2025.x+).
static void elero_log_callback(void *instance, uint8_t level, const char *tag,
                                const char *message, size_t message_len) {
  auto *hub = static_cast<Elero *>(instance);
  // Map ESPHome levels (1-7) to the 5-level scheme used by the web UI:
  //   ESPHome 1 ERROR         → 1 error
  //   ESPHome 2 WARN          → 2 warn
  //   ESPHome 3 INFO          → 3 info
  //   ESPHome 4 CONFIG        → 3 info
  //   ESPHome 5 DEBUG         → 4 debug
  //   ESPHome 6/7 VERBOSE+    → 5 verbose
  uint8_t mapped;
  if (level <= 1)
    mapped = 1;
  else if (level == 2)
    mapped = 2;
  else if (level <= 4)
    mapped = 3;
  else if (level == 5)
    mapped = 4;
  else
    mapped = 5;
  hub->append_log(mapped, tag, "%s", message);
}
#endif

static const char *TAG = "elero";
static const uint8_t  SPI_SETTLE_US = 5;  // inter-transaction settling (CC1101 needs ~1-2µs)
static const uint8_t flash_table_encode[] = {0x08, 0x02, 0x0d, 0x01, 0x0f, 0x0e, 0x07, 0x05, 0x09, 0x0c, 0x00, 0x0a, 0x03, 0x04, 0x0b, 0x06};
static const uint8_t flash_table_decode[] = {0x0a, 0x03, 0x01, 0x0c, 0x0d, 0x07, 0x0f, 0x06, 0x00, 0x08, 0x0b, 0x0e, 0x09, 0x02, 0x05, 0x04};

// ---------------------------------------------------------------------------
// EspHomeRadioLibHal — bridges ESPHome's SPIDevice to RadioLib's HAL
// ---------------------------------------------------------------------------
EspHomeRadioLibHal::EspHomeRadioLibHal()
    : RadioLibHal(0x01 /*INPUT*/, 0x02 /*OUTPUT*/, 0 /*LOW*/, 1 /*HIGH*/, 1 /*RISING*/, 2 /*FALLING*/) {}

void EspHomeRadioLibHal::pinMode(uint32_t pin, uint32_t mode) {
  // GPIO is managed by ESPHome — no-op
}
void EspHomeRadioLibHal::digitalWrite(uint32_t pin, uint32_t value) {
  // CS is managed by ESPHome's SPIDevice enable/disable — no-op
}
uint32_t EspHomeRadioLibHal::digitalRead(uint32_t pin) {
  return 0;  // Not used in register-only mode
}
void EspHomeRadioLibHal::attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) {
  // Interrupts are managed by Elero::setup() directly — no-op
}
void EspHomeRadioLibHal::detachInterrupt(uint32_t interruptNum) {
  // No-op
}
void EspHomeRadioLibHal::delay(RadioLibTime_t ms) {
  esphome::delay(ms);
}
void EspHomeRadioLibHal::delayMicroseconds(RadioLibTime_t us) {
  delay_microseconds_safe(us);
}
RadioLibTime_t EspHomeRadioLibHal::millis() {
  return esphome::millis();
}
RadioLibTime_t EspHomeRadioLibHal::micros() {
  return esphome::micros();
}
long EspHomeRadioLibHal::pulseIn(uint32_t pin, uint32_t state, RadioLibTime_t timeout) {
  return 0;  // Not used
}
void EspHomeRadioLibHal::spiBegin() {
  // SPI bus is initialized by ESPHome's SPI component — no-op
}
void EspHomeRadioLibHal::spiBeginTransaction() {
  if (this->spi_parent_ != nullptr) {
    static_cast<Elero *>(this->spi_parent_)->enable();
  }
}
void EspHomeRadioLibHal::spiTransfer(uint8_t *out, size_t len, uint8_t *in) {
  if (this->spi_parent_ == nullptr)
    return;
  auto *parent = static_cast<Elero *>(this->spi_parent_);
  for (size_t i = 0; i < len; i++) {
    uint8_t tx = (out != nullptr) ? out[i] : 0x00;
    uint8_t rx = parent->transfer_byte(tx);
    if (in != nullptr)
      in[i] = rx;
  }
}
void EspHomeRadioLibHal::spiEndTransaction() {
  if (this->spi_parent_ != nullptr) {
    static_cast<Elero *>(this->spi_parent_)->disable();
    delay_microseconds_safe(SPI_SETTLE_US);  // Match main's inter-transaction settling time for CC1101
  }
}
void EspHomeRadioLibHal::spiEnd() {
  // SPI bus lifecycle is managed by ESPHome — no-op
}

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

const char *elero_state_to_string(uint8_t state) {
  switch (state) {
    case ELERO_STATE_TOP: return "top";
    case ELERO_STATE_BOTTOM: return "bottom";
    case ELERO_STATE_INTERMEDIATE: return "intermediate";
    case ELERO_STATE_TILT: return "tilt";
    case ELERO_STATE_BLOCKING: return "blocking";
    case ELERO_STATE_OVERHEATED: return "overheated";
    case ELERO_STATE_TIMEOUT: return "timeout";
    case ELERO_STATE_START_MOVING_UP: return "start_moving_up";
    case ELERO_STATE_START_MOVING_DOWN: return "start_moving_down";
    case ELERO_STATE_MOVING_UP: return "moving_up";
    case ELERO_STATE_MOVING_DOWN: return "moving_down";
    case ELERO_STATE_STOPPED: return "stopped";
    case ELERO_STATE_TOP_TILT: return "top_tilt";
    case ELERO_STATE_BOTTOM_TILT: return "bottom_tilt"; // also ELERO_STATE_OFF (0x0f)
    case ELERO_STATE_ON: return "on";
    default: return "unknown";
  }
}

void dispatch_commands(Elero *parent, std::queue<uint8_t> &queue,
                       t_elero_command &cmd, uint8_t &send_packets,
                       uint8_t &send_retries, uint32_t &last_command,
                       bool &queue_full_published, uint32_t now,
                       const char *tag, uint32_t blind_addr,
                       void (*increase_counter_fn)(void *ctx), void *ctx) {
  // Skip immediately if hub SPI is permanently broken — no point retrying.
  if (parent->is_failed()) return;

  // When stop_urgent is active, defer non-stop commands from other covers
  // so the stopping cover's packets transmit without queue contention.
  if (parent->is_stop_urgent() && !queue.empty()) {
    uint8_t front_cmd = queue.front();
    if (front_cmd != ELERO_COMMAND_COVER_STOP) {
      return;  // defer until stop_urgent clears
    }
  }

  // Exponential backoff on retries: normal delay is send_delay (1ms),
  // but after failures we wait 10/20/40ms to break the thundering herd
  // when multiple covers retry simultaneously.
  uint32_t delay = parent->get_send_delay();
  if (send_retries > 0) {
    uint8_t shift = (send_retries > 3) ? 3 : send_retries;
    delay += (10u << shift);  // +20ms, +40ms, +80ms
  }

  // Stop commands bypass backoff entirely — they are time-critical
  bool is_stop = (!queue.empty() && queue.front() == ELERO_COMMAND_COVER_STOP);
  if (is_stop || (now - last_command) > delay) {
    if (!queue.empty()) {
      cmd.payload[4] = queue.front();
      if (parent->send_command(&cmd)) {
        send_packets++;
        send_retries = 0;
        if (send_packets >= parent->get_send_repeats()) {
          queue.pop();
          send_packets = 0;
          increase_counter_fn(ctx);
#ifdef USE_TEXT_SENSOR
          if (queue_full_published && queue.empty()) {
            queue_full_published = false;
          }
#endif
        }
        last_command = now;
      } else {
        send_retries++;
        ESP_LOGD(tag, "Retry #%d for 0x%06x (backoff %lums)",
                 send_retries, blind_addr, (unsigned long)delay);
        if (send_retries > ELERO_SEND_RETRIES) {
          ESP_LOGE(tag, "Hit maximum retries for 0x%06x, giving up.", blind_addr);
          parent->increment_tx_drop_count();
          send_retries = 0;
          queue.pop();
        }
        last_command = now;
      }
    }
  }
}

Elero::~Elero() {
  // 1. Signal radio task to stop and wait for it to exit
  if (this->radio_task_handle_ != nullptr) {
    this->task_shutdown_.store(true, std::memory_order_release);
    // Send SHUTDOWN message to unblock task if waiting on queue
    RadioMessage shutdown_msg{};
    shutdown_msg.type = RadioControlType::SHUTDOWN;
    if (this->tx_queue_) {
      xQueueSend(this->tx_queue_, &shutdown_msg, pdMS_TO_TICKS(100));
    }
    // Wait for task to exit (up to 1s)
    for (int i = 0; i < 100 && this->radio_task_handle_ != nullptr; i++) {
      delay(10);
    }
    if (this->radio_task_handle_ != nullptr) {
      vTaskDelete(this->radio_task_handle_);  // force-kill as last resort
      this->radio_task_handle_ = nullptr;
    }
  }
  // 2. Now safe to detach interrupt and delete RadioLib
  if (this->gdo0_pin_ != nullptr) {
    this->gdo0_pin_->detach_interrupt();
  }
  delete this->radio_;
  this->radio_ = nullptr;
  delete this->radio_module_;
  this->radio_module_ = nullptr;
  // 3. Clean up FreeRTOS resources
  if (this->tx_queue_) { vQueueDelete(this->tx_queue_); this->tx_queue_ = nullptr; }
  if (this->tx_priority_queue_) { vQueueDelete(this->tx_priority_queue_); this->tx_priority_queue_ = nullptr; }
  if (this->rx_queue_) { vQueueDelete(this->rx_queue_); this->rx_queue_ = nullptr; }
#ifdef USE_LOGGER
  // Callback-based log listener — no heap allocation to clean up.
  this->log_listener_ = nullptr;
#endif
}

void Elero::loop() {
  // Check if the radio task detected a fatal SPI error
  if (this->radio_fatal_error_.load(std::memory_order_acquire)) {
    if (!this->is_failed()) {
      this->mark_failed(LOG_STR("Radio task: SPI permanently broken — check pin assignments"));
    }
    return;
  }
  // Skip all processing if SPI is permanently broken (e.g. strapping pin issue).
  if (this->spi_failed_.load(std::memory_order_acquire))
    return;

  // 1. Drain RX results from radio task (Core 0 → Core 1 via queue).
  if (this->rx_queue_) {
    RxResult rx;
    uint8_t rx_drain_count = 0;
    while (rx_drain_count < ELERO_MAX_RX_PER_LOOP &&
           xQueueReceive(this->rx_queue_, &rx, 0) == pdTRUE) {
      this->dispatch_rx_result_(rx);
      rx_drain_count++;
    }
  }

  // 2. Drain runtime blind command queues (enqueues to TX queue).
  if (this->is_tx_idle()) {
    this->drain_runtime_queues();
    this->poll_runtime_blinds_();
  }

  // 3. Recompute dead-reckoning positions for runtime-adopted blinds.
  this->recompute_runtime_positions_();

  // 4. Publish hub-level diagnostic sensors periodically (every 30 s).
#ifdef USE_SENSOR
  {
    uint32_t now = millis();
    if (now - this->last_hub_sensor_update_ms_ >= 30000) {
      this->last_hub_sensor_update_ms_ = now;
      if (this->frequency_sensor_ != nullptr)
        this->frequency_sensor_->publish_state(
            registers_to_mhz(this->freq2_, this->freq1_, this->freq0_));
      if (this->rx_count_sensor_ != nullptr)
        this->rx_count_sensor_->publish_state((float) this->get_rx_count());
      if (this->tx_count_sensor_ != nullptr)
        this->tx_count_sensor_->publish_state((float) this->get_tx_count());
      if (this->watchdog_recovery_sensor_ != nullptr)
        this->watchdog_recovery_sensor_->publish_state((float) this->get_watchdog_recovery_count());
    }
  }
#endif
}

// ---------------------------------------------------------------------------
// process_rx — drain all available packets from the CC1101 RX FIFO
// ---------------------------------------------------------------------------
void Elero::process_rx() {
  if (!this->rx_ready_.load(std::memory_order_acquire))
    return;
  this->rx_ready_.store(false, std::memory_order_release);

  // Drain up to ELERO_MAX_RX_PER_LOOP packets per call to prevent infinite
  // loops if noise keeps triggering the interrupt.
  for (uint8_t iter = 0; iter < ELERO_MAX_RX_PER_LOOP; iter++) {
    uint8_t len = this->read_status(CC1101_RXBYTES);

    if (len & 0x80) {  // overflow bit set — FIFO data unreliable
      uint32_t now = millis();
      // Rate-limit: if we already flushed recently, suppress log and skip
      if (now - this->last_rx_overflow_ms_ < 1000) {
        this->rx_overflow_count_++;
        // After 5 rapid overflows, escalate to full radio reinit
        if (this->rx_overflow_count_ >= 5) {
          ESP_LOGW(TAG, "RX FIFO overflow persists after %d flushes, full radio reinit",
                   this->rx_overflow_count_);
          this->rx_overflow_count_ = 0;
          this->last_rx_overflow_ms_ = now;
          this->reset();
          this->init();
        }
        return;
      }
      this->rx_overflow_count_ = 1;
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
static const uint32_t TX_STATE_TIMEOUT_MS = 50;      // per-state watchdog
static const uint32_t TX_COOLDOWN_MS = 3;             // settle time after TX before next TX (PLL lock ~75µs)
static const uint32_t RADIO_WATCHDOG_MS = 5000;       // radio health check interval

void Elero::advance_tx() {
  uint32_t now = millis();
  uint32_t elapsed = now - this->tx_state_entered_ms_;

  switch (this->tx_state_.load(std::memory_order_acquire)) {

    case TxState::TRANSMITTING: {
      // TX completion: CC1101 auto-transitions to RX (MCSM1 TXOFF_MODE=0x3).
      // Detect by polling MARCSTATE — when it leaves TX the packet is sent.
      // After STX strobe the CC1101 goes through calibration states before TX:
      //   STARTCAL → BWBOOST → FS_LOCK → ENDCAL → FSTXON → TX
      // We must wait through these intermediate states, not abort.
      uint8_t marc = this->read_status(CC1101_MARCSTATE) & 0x1F;
      bool tx_in_progress = (marc == CC1101_MARCSTATE_TX) ||
                            (marc == CC1101_MARCSTATE_STARTCAL) ||
                            (marc == CC1101_MARCSTATE_BWBOOST) ||
                            (marc == CC1101_MARCSTATE_FS_LOCK) ||
                            (marc == CC1101_MARCSTATE_IFADCON) ||
                            (marc == CC1101_MARCSTATE_ENDCAL) ||
                            (marc == CC1101_MARCSTATE_FSTXON) ||
                            (marc == CC1101_MARCSTATE_RXTX_SWITCH);
      if (tx_in_progress) {
        // Still calibrating or transmitting — check for timeout
        if (elapsed > TX_STATE_TIMEOUT_MS) {
          ESP_LOGW(TAG, "TX timeout in TRANSMITTING (marc=%s, %lums), aborting",
                   marcstate_to_string(marc), (unsigned long) elapsed);
          this->tx_abort_();
        }
      } else if (marc == CC1101_MARCSTATE_TXFIFO_UFLOW) {
        // TX FIFO underflow — data was partially sent, abort and retry
        ESP_LOGE(TAG, "TX FIFO underflow");
        this->tx_abort_();
      } else {
        // MARCSTATE left TX — verify FIFO drained (packet actually sent)
        uint8_t bytes = this->read_status(CC1101_TXBYTES) & 0x7F;
        if (bytes == 0) {
          this->tx_count_.fetch_add(1, std::memory_order_relaxed);
          ESP_LOGV(TAG, "TX complete (marc=%s, %lums)",
                   marcstate_to_string(marc), (unsigned long) elapsed);
          this->tx_state_.store(TxState::COOLDOWN, std::memory_order_release);
          this->tx_state_entered_ms_ = now;
          this->last_tx_complete_ms_ = now;
        } else {
          // FIFO not empty — packet was never sent.  Most likely cause is CCA
          // (Clear Channel Assessment) rejection: the channel was busy so the
          // CC1101 returned to RX without transmitting.
          ESP_LOGW(TAG, "TX failed: %d bytes still in FIFO (marc=%s) — likely CCA rejection, will retry",
                   bytes, marcstate_to_string(marc));
          this->tx_abort_();
        }
      }
      break;
    }

    case TxState::COOLDOWN: {
      // Allow the CC1101 time to settle into RX before accepting the next TX.
      if (elapsed >= TX_COOLDOWN_MS) {
        this->tx_state_.store(TxState::IDLE, std::memory_order_release);
        // Check for RX FIFO issues that may have occurred during the TX cycle.
        // The ISR still sets rx_ready_ during TX, but process_rx() only runs
        // when IDLE, so overflow or pending data could go unnoticed without
        // this explicit check.
        uint8_t rxbytes = this->read_status(CC1101_RXBYTES);
        if (rxbytes & 0x80) {
          ESP_LOGW(TAG, "RX FIFO overflow detected after TX, flushing");
          this->flush_rx();
        } else if ((rxbytes & 0x7F) > 0) {
          // Data arrived during TX — make sure process_rx() sees it
          this->rx_ready_.store(true, std::memory_order_release);
        }
        this->process_rx();
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
  this->tx_state_.store(TxState::IDLE, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// is_duplicate_packet_ — suppress relay-hop duplicates of the same packet
// ---------------------------------------------------------------------------
bool Elero::is_duplicate_packet_(uint32_t src, uint8_t cnt) {
  uint32_t now = millis();
  uint64_t key = (static_cast<uint64_t>(src) << 8) | cnt;
  auto it = this->dedup_map_.find(key);
  if (it != this->dedup_map_.end() && (now - it->second) < ELERO_DEDUP_WINDOW_MS) {
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
    if ((now - it->second) >= ELERO_DEDUP_WINDOW_MS)
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

  // RX is the expected state when tx_state_ is IDLE
  if (marc == CC1101_MARCSTATE_RX) {
    this->consecutive_watchdog_failures_ = 0;
    return;
  }

  // Transient calibration / synthesizer states — let them complete
  if (marc >= CC1101_MARCSTATE_VCOON_MC && marc <= CC1101_MARCSTATE_ENDCAL)
    return;
  // RX wind-down states are also transient
  if (marc == CC1101_MARCSTATE_RX_END || marc == CC1101_MARCSTATE_RX_RST)
    return;

  // RXFIFO_OVERFLOW — flush and restart RX (escalate to full reinit if repeated)
  if (marc == CC1101_MARCSTATE_RXFIFO_OFLOW) {
    ESP_LOGW(TAG, "Radio watchdog: RX FIFO overflow, flushing");
    this->watchdog_recovery_count_.fetch_add(1, std::memory_order_relaxed);
    this->flush_rx();
    // Verify recovery worked
    uint8_t marc_after = this->read_status(CC1101_MARCSTATE) & 0x1F;
    if (marc_after != CC1101_MARCSTATE_RX) {
      ESP_LOGW(TAG, "Radio watchdog: flush_rx failed (marc=0x%02x), full reinit", marc_after);
      this->reset();
      if (!this->init()) {
        this->consecutive_watchdog_failures_++;
      }
    }
    return;
  }

  // IDLE — radio stopped listening, restart RX
  if (marc == CC1101_MARCSTATE_IDLE) {
    ESP_LOGW(TAG, "Radio watchdog: stuck in IDLE, restarting RX");
    this->watchdog_recovery_count_.fetch_add(1, std::memory_order_relaxed);
    this->consecutive_watchdog_failures_ = 0;  // IDLE is a valid CC1101 state, not SPI failure
    this->write_cmd(CC1101_SRX);
    return;
  }

  // Any other unexpected state — full reset + reinitialize
  // flush_and_rx() is insufficient when the CC1101 is unresponsive (e.g.
  // MARCSTATE reads 0x1f = SPI returning 0xFF).  A hardware reset via
  // SRES followed by full register configuration is needed.
  ESP_LOGW(TAG, "Radio watchdog: unexpected state 0x%02x, full reinit", marc);
  this->watchdog_recovery_count_.fetch_add(1, std::memory_order_relaxed);
  this->consecutive_watchdog_failures_++;

  // If the watchdog has failed 2+ times in a row without recovery, SPI is
  // likely permanently broken (e.g. GPIO12 strapping pin issue).  Stop
  // retrying to avoid flooding the log.
  if (this->consecutive_watchdog_failures_ >= 2) {
    ESP_LOGE(TAG, "Radio watchdog: %d consecutive failures — SPI appears permanently broken.",
             this->consecutive_watchdog_failures_);
    ESP_LOGE(TAG, "  If GPIO12 is used for SPI MISO, it may be pulling VDD_SDIO to 1.8V at boot.");
    ESP_LOGE(TAG, "  Use non-strapping pins for SPI (e.g. CLK=18, MISO=19, MOSI=23).");
    this->spi_failed_.store(true, std::memory_order_release);
    this->radio_fatal_error_.store(true, std::memory_order_release);
    return;
  }

  this->reset();
  if (!this->init()) {
    // init() detected SPI failure — escalate immediately on next watchdog
    this->consecutive_watchdog_failures_++;
  }
}

// ---------------------------------------------------------------------------
// drain_runtime_queues — send one pending command from runtime-adopted blinds
// ---------------------------------------------------------------------------
void Elero::drain_runtime_queues() {
  std::lock_guard<std::mutex> lock(state_mutex_);
  for (auto &entry : this->runtime_blinds_) {
    auto &rb = entry.second;
    if (!rb.command_queue.empty()) {
      uint8_t cmd_byte = rb.command_queue.front();
      t_elero_command cmd{};
      cmd.counter = rb.cmd_counter;
      cmd.blind_addr = rb.blind_address;
      cmd.remote_addr = rb.remote_address;
      cmd.channel = rb.channel;
      cmd.pck_inf[0] = rb.pck_inf[0];
      cmd.pck_inf[1] = rb.pck_inf[1];
      cmd.hop = rb.hop;
      cmd.payload[0] = rb.payload_1;
      cmd.payload[1] = rb.payload_2;
      cmd.payload[4] = cmd_byte;
      if (this->send_command(&cmd)) {
        rb.send_packets_count++;
        if (rb.send_packets_count >= this->send_repeats_) {
          rb.command_queue.pop();
          rb.send_packets_count = 0;
          if (rb.cmd_counter == 0xFF)
            rb.cmd_counter = 1;
          else
            rb.cmd_counter++;
        }
      }
      break;  // Only one TX per loop iteration
    }
  }
}

// ---------------------------------------------------------------------------
// poll_runtime_blinds_ — enqueue periodic status checks for runtime blinds
// ---------------------------------------------------------------------------
void Elero::poll_runtime_blinds_() {
  std::lock_guard<std::mutex> lock(state_mutex_);
  uint32_t now = millis();
  for (auto &entry : this->runtime_blinds_) {
    auto &rb = entry.second;
    // Skip if poll interval is effectively disabled (uint32_t max)
    if (rb.poll_intvl_ms == 0 || rb.poll_intvl_ms == UINT32_MAX)
      continue;
    // Skip if command queue already has pending commands
    if (!rb.command_queue.empty())
      continue;
    if ((now - rb.last_poll_ms) >= rb.poll_intvl_ms) {
      rb.last_poll_ms = now;
      if (rb.command_queue.size() < ELERO_MAX_COMMAND_QUEUE) {
        rb.command_queue.push(ELERO_COMMAND_COVER_CHECK);
        ESP_LOGD(TAG, "Periodic poll for runtime blind 0x%06x", rb.blind_address);
      }
    }
  }
}

// ---------------------------------------------------------------------------
// update_runtime_blind_direction_ — set moving direction from RF state byte
// ---------------------------------------------------------------------------
void Elero::update_runtime_blind_direction_(RuntimeBlind &rb, uint8_t state) {
  int8_t old_dir = rb.moving_direction;
  switch (state) {
    case ELERO_STATE_START_MOVING_UP:
    case ELERO_STATE_MOVING_UP:
      rb.moving_direction = 1;  // opening
      break;
    case ELERO_STATE_START_MOVING_DOWN:
    case ELERO_STATE_MOVING_DOWN:
      rb.moving_direction = -1;  // closing
      break;
    case ELERO_STATE_TOP:
      rb.moving_direction = 0;
      rb.position = 1.0f;
      break;
    case ELERO_STATE_BOTTOM:
      rb.moving_direction = 0;
      rb.position = 0.0f;
      break;
    case ELERO_STATE_STOPPED:
    case ELERO_STATE_INTERMEDIATE:
    case ELERO_STATE_TILT:
    case ELERO_STATE_BLOCKING:
    case ELERO_STATE_OVERHEATED:
    case ELERO_STATE_TIMEOUT:
      rb.moving_direction = 0;
      break;
    default:
      break;
  }
  // Reset recompute timestamp when direction changes
  if (old_dir != rb.moving_direction) {
    rb.last_recompute_ms = millis();
  }
}

// ---------------------------------------------------------------------------
// recompute_runtime_positions_ — dead-reckoning position update for moving
// runtime blinds (called from loop())
// ---------------------------------------------------------------------------
void Elero::recompute_runtime_positions_() {
  std::lock_guard<std::mutex> lock(state_mutex_);
  uint32_t now = millis();
  for (auto &entry : this->runtime_blinds_) {
    auto &rb = entry.second;
    if (rb.moving_direction == 0)
      continue;
    // Only track position when both durations are configured
    if (rb.open_duration_ms == 0 || rb.close_duration_ms == 0)
      continue;
    // If position is unknown, we can't dead-reckon
    if (rb.position < 0.0f)
      continue;

    uint32_t elapsed = now - rb.last_recompute_ms;
    if (elapsed == 0)
      continue;

    // Sanity check: skip recompute if elapsed time is implausibly large
    // (e.g., millis() wraparound glitch or stale last_recompute_ms)
    if (elapsed > ELERO_TIMEOUT_MOVEMENT) {
      rb.last_recompute_ms = now;
      continue;
    }

    float delta;
    if (rb.moving_direction > 0) {
      // Opening: position increases
      delta = static_cast<float>(elapsed) / static_cast<float>(rb.open_duration_ms);
      rb.position += delta;
    } else {
      // Closing: position decreases
      delta = static_cast<float>(elapsed) / static_cast<float>(rb.close_duration_ms);
      rb.position -= delta;
    }
    // Clamp to [0.0, 1.0]
    if (rb.position > 1.0f) rb.position = 1.0f;
    if (rb.position < 0.0f) rb.position = 0.0f;
    rb.last_recompute_ms = now;
  }
}

void IRAM_ATTR Elero::interrupt(Elero *arg) {
  // GDO0 (IOCFG0=0x06) fires on sync-word/end-of-packet.  We always set
  // rx_ready_ — stale flags during TX are harmlessly ignored because
  // process_rx() only runs when tx_state_ == IDLE.
  arg->rx_ready_.store(true, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// Radio task — runs on Core 0, owns ALL SPI access after setup()
// ---------------------------------------------------------------------------
void Elero::radio_task_func_(void *param) {
  auto *hub = static_cast<Elero *>(param);
  ESP_LOGI(TAG, "Radio task started on Core %d", xPortGetCoreID());
  while (!hub->task_shutdown_.load(std::memory_order_acquire)) {
    hub->radio_task_loop_();
    // Adaptive delay: yield immediately when TX active or queues have pending work
    // to minimize stop command latency; sleep 1ms when idle to save CPU.
    bool has_work = (hub->tx_state_.load(std::memory_order_acquire) != TxState::IDLE) ||
                    (hub->tx_priority_queue_ && uxQueueMessagesWaiting(hub->tx_priority_queue_) > 0) ||
                    (hub->tx_queue_ && uxQueueMessagesWaiting(hub->tx_queue_) > 0);
    if (has_work) {
      taskYIELD();
    } else {
      vTaskDelay(1);  // yield to WiFi/system tasks (~1ms)
    }
  }
  ESP_LOGI(TAG, "Radio task shutting down");
  hub->radio_task_handle_ = nullptr;  // signal destructor we're done
  vTaskDelete(nullptr);  // delete self
}

void Elero::radio_task_loop_() {
  // 1. Process TX commands from Core 1 — priority queue first for time-critical stop commands
  RadioMessage msg{};
  bool got_msg = false;
  if (this->tx_priority_queue_ && xQueueReceive(this->tx_priority_queue_, &msg, 0) == pdTRUE) {
    got_msg = true;
  } else if (this->tx_queue_ && xQueueReceive(this->tx_queue_, &msg, 0) == pdTRUE) {
    got_msg = true;
  }
  if (got_msg) {
    switch (msg.type) {
      case RadioControlType::TX_COMMAND:
        this->send_command_internal_(&msg.tx.cmd);
        break;
      case RadioControlType::REINIT_FREQ: {
        bool ok = false;
        if (this->is_tx_idle()) {
          this->rx_ready_.store(false, std::memory_order_release);
          this->tx_state_.store(TxState::IDLE, std::memory_order_release);
          this->freq2_ = msg.freq.freq2;
          this->freq1_ = msg.freq.freq1;
          this->freq0_ = msg.freq.freq0;
          this->reset();
          this->init();
          float mhz = registers_to_mhz(msg.freq.freq2, msg.freq.freq1, msg.freq.freq0);
          ESP_LOGI(TAG, "CC1101 re-initialised: %.2f MHz (0x%02x 0x%02x 0x%02x)",
                   mhz, msg.freq.freq2, msg.freq.freq1, msg.freq.freq0);
          ok = true;
        }
        if (msg.result_ptr) *msg.result_ptr = ok;
        if (msg.completion_sem) xSemaphoreGive(msg.completion_sem);
        break;
      }
      case RadioControlType::START_SCAN:
        this->scan_mode_.store(true, std::memory_order_release);
        break;
      case RadioControlType::STOP_SCAN:
        this->scan_mode_.store(false, std::memory_order_release);
        break;
      case RadioControlType::START_DUMP:
        this->packet_dump_mode_.store(true, std::memory_order_release);
        ESP_LOGI(TAG, "Packet dump mode started");
        break;
      case RadioControlType::STOP_DUMP:
        this->packet_dump_mode_.store(false, std::memory_order_release);
        ESP_LOGI(TAG, "Packet dump mode stopped");
        break;
      case RadioControlType::SHUTDOWN:
        return;  // will be caught by shutdown check in radio_task_func_
    }
  }

  // 2. Process RX if ISR flag set and TX idle/cooldown
  TxState cur_tx = this->tx_state_.load(std::memory_order_acquire);
  if (cur_tx == TxState::IDLE || cur_tx == TxState::COOLDOWN) {
    this->process_rx();
  }

  // 3. Advance TX state machine
  if (cur_tx != TxState::IDLE) {
    this->advance_tx();
  }

  // 4. Periodic radio health check (only when TX idle)
  if (this->tx_state_.load(std::memory_order_acquire) == TxState::IDLE) {
    this->check_radio_state_();
    this->prune_dedup_map_();
  }
}

void Elero::dump_config() {
  ESP_LOGCONFIG(TAG, "Elero CC1101:");
  LOG_PIN("  GDO0 Pin: ", this->gdo0_pin_);
  ESP_LOGCONFIG(TAG, "  freq2: 0x%02x, freq1: 0x%02x, freq0: 0x%02x", this->freq2_, this->freq1_, this->freq0_);
  ESP_LOGCONFIG(TAG, "  Send repeats: %d, send delay: %d ms", this->send_repeats_, this->send_delay_);
  ESP_LOGCONFIG(TAG, "  RadioLib: begin() + standby() + setFrequency(); direct SPI for register access");
  if (this->spi_failed_.load(std::memory_order_acquire)) {
    ESP_LOGCONFIG(TAG, "  SPI Status: FAILED — CC1101 communication broken");
    ESP_LOGCONFIG(TAG, "  Check SPI pin assignments — avoid ESP32 strapping pins (GPIO0/2/5/12/15)");
  }
  ESP_LOGCONFIG(TAG, "  Registered covers: %d", this->address_to_cover_mapping_.size());
}

void Elero::setup() {
  ESP_LOGI(TAG, "Setting up Elero Component...");

  // Allow the CC1101 to stabilize after power-on.  On boards like the LilyGo
  // T-Embed CC1101 the radio sits behind a GPIO-controlled power rail that may
  // have been enabled only moments before setup() runs (ESPHome on_boot delays
  // are non-blocking coroutines).  The CC1101 datasheet requires a minimum of
  // ~40 µs after power-on, but real-world modules with slow voltage regulators
  // need significantly more.  150 ms covers typical RC rise-times.
  delay(150);

  this->spi_setup();

  // Initialize RadioLib HAL adapter — bridge ESPHome SPI to RadioLib Module.
  // We use RADIOLIB_NC for all pins because GPIO/interrupt management stays
  // with ESPHome.  RadioLib is only used for SPI register access with
  // built-in verify-readback error handling.
  this->radio_hal_.set_spi_parent(this);
  this->radio_module_ = new Module(&this->radio_hal_, RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC);
  this->radio_ = new CC1101(this->radio_module_);

  // begin() initializes spiConfig, Module::init(), resets CC1101, and verifies
  // the chip version (10 retries).  Parameters don't matter — our init()
  // overwrites all registers with Elero-specific values afterwards.
  int16_t rc = this->radio_->begin(868.35, 47.607, 38.383, 101.5625, 10, 32);
  if (rc == RADIOLIB_ERR_NONE) {
    ESP_LOGI(TAG, "RadioLib CC1101 initialized (chip version verified)");
  } else if (rc == RADIOLIB_ERR_CHIP_NOT_FOUND) {
    ESP_LOGE(TAG, "RadioLib: CC1101 chip not found! Check SPI wiring. rc=%d", rc);
    this->mark_failed(LOG_STR("CC1101 chip not found — check SPI wiring"));
    return;
  } else {
    ESP_LOGW(TAG, "RadioLib begin() returned rc=%d, continuing with custom init", rc);
  }

  this->gdo0_pin_->setup();
  this->gdo0_pin_->attach_interrupt(Elero::interrupt, this, gpio::INTERRUPT_FALLING_EDGE);
  this->reset();
  if (!this->init()) {
    // First reset+init failed — try once more with a longer post-reset delay.
    // Some CC1101 modules or ESP32 boot sequences need additional settling time.
    ESP_LOGW(TAG, "First init failed, retrying reset+init with extended delay...");
    delay(10);  // 10ms extra settling time
    this->reset();
    if (!this->init()) {
      // SPI diagnostic dump — read chip ID registers and test multiple register
      // writes to help the user identify wiring or pin assignment issues.
      uint8_t partnum = this->read_status(CC1101_PARTNUM);  // Expected: 0x00 for CC1101
      uint8_t version = this->read_status(CC1101_VERSION);  // Expected: 0x14 for CC1101
      ESP_LOGE(TAG, "CC1101 SPI diagnostic: PARTNUM=0x%02x (expect 0x00), VERSION=0x%02x (expect 0x14)", partnum, version);

      // Test raw SPI: write different values to a writable register and read back
      const uint8_t test_vals[] = {0xAA, 0x55, 0x0F, 0x00};
      for (uint8_t tv : test_vals) {
        this->write_reg(CC1101_FSCTRL1, tv);
        uint8_t rb = this->read_reg(CC1101_FSCTRL1);
        ESP_LOGE(TAG, "  SPI write/read test: wrote 0x%02x, read 0x%02x %s", tv, rb, (tv == rb) ? "OK" : "MISMATCH");
      }

      ESP_LOGE(TAG, "CC1101 SPI communication is broken — the radio is non-functional.");
      if (partnum == 0x00 && version == 0x14) {
        ESP_LOGE(TAG, "  CC1101 chip IS detected but register writes fail.");
        ESP_LOGE(TAG, "  This may indicate MOSI is not connected or the SPI bus has a conflict.");
      } else if (partnum == 0x00 && version == 0x00) {
        ESP_LOGE(TAG, "  SPI returns all zeros — MISO may be stuck LOW or CS not reaching the CC1101.");
      } else if (partnum == 0xFF && version == 0xFF) {
        ESP_LOGE(TAG, "  SPI returns all ones — MISO may be stuck HIGH or the CC1101 is not powered.");
      } else {
        ESP_LOGE(TAG, "  Unexpected chip ID — verify the radio module is a CC1101 on this SPI bus.");
      }
      ESP_LOGE(TAG, "  Configured SPI: CS=pin_cc1101_cs, GDO0=pin_cc1101_gdo0");
      ESP_LOGE(TAG, "  Verify SPI wiring: CLK, MOSI, MISO, CS must match the board schematic.");
      this->spi_failed_.store(true, std::memory_order_release);
      this->mark_failed(LOG_STR("CC1101 SPI communication broken — check pin assignments"));
      return;
    }
    ESP_LOGI(TAG, "CC1101 init succeeded on second attempt after extended reset delay");
  }

#ifdef USE_LOGGER
  // Forward all ESP_LOG messages into the ring buffer so the web UI Log tab
  // can display them when capture is enabled.
  if (logger::global_logger != nullptr) {
    logger::global_logger->add_log_callback(this, elero_log_callback);
    this->log_listener_ = this;  // non-null sentinel so destructor knows it was registered
  }
#endif

  // Create FreeRTOS queues for dual-core communication
  this->tx_queue_ = xQueueCreate(ELERO_TX_QUEUE_DEPTH, sizeof(RadioMessage));
  this->tx_priority_queue_ = xQueueCreate(ELERO_TX_PRIORITY_QUEUE_DEPTH, sizeof(RadioMessage));
  this->rx_queue_ = xQueueCreate(32, sizeof(RxResult));  // 32 deep for 5+ simultaneous blind responses
  if (!this->tx_queue_ || !this->tx_priority_queue_ || !this->rx_queue_) {
    ESP_LOGE(TAG, "Failed to create FreeRTOS queues for radio task");
    this->mark_failed(LOG_STR("Failed to allocate radio task queues"));
    return;
  }

  // Spawn radio task on Core 0 — ALL SPI access moves to this task.
  // Priority 19: below WiFi (23) to prevent WiFi starvation, above most
  // ESP-IDF system tasks.
  BaseType_t rc_task = xTaskCreatePinnedToCore(
    Elero::radio_task_func_,     // function
    "elero_radio",               // name (max 16 chars)
    8192,                        // stack size (bytes)
    this,                        // parameter
    19,                          // priority
    &this->radio_task_handle_,   // handle
    0                            // Core 0
  );
  if (rc_task != pdPASS) {
    ESP_LOGE(TAG, "Failed to create radio task on Core 0");
    this->mark_failed(LOG_STR("Failed to create radio task"));
    return;
  }
  ESP_LOGI(TAG, "Radio task spawned on Core 0 (priority 19, stack 8192)");

  // Request the main loop to skip its ~16ms sleep so loop() runs every pass.
  // With dual-core, this speeds up RX queue draining on Core 1 and improves
  // position tracking responsiveness for auto-stop.
  this->high_freq_.start();

  // Publish initial values for hub-level diagnostic sensors.
#ifdef USE_SENSOR
  if (this->frequency_sensor_ != nullptr)
    this->frequency_sensor_->publish_state(
        registers_to_mhz(this->freq2_, this->freq1_, this->freq0_));
  if (this->rx_count_sensor_ != nullptr)
    this->rx_count_sensor_->publish_state(0.0f);
  if (this->tx_count_sensor_ != nullptr)
    this->tx_count_sensor_->publish_state(0.0f);
  if (this->watchdog_recovery_sensor_ != nullptr)
    this->watchdog_recovery_sensor_->publish_state(0.0f);
  this->last_hub_sensor_update_ms_ = millis();
#endif
}

float Elero::registers_to_mhz(uint8_t freq2, uint8_t freq1, uint8_t freq0) {
  return (26.0f / 65536.0f) * ((static_cast<uint32_t>(freq2) << 16) |
                                (static_cast<uint32_t>(freq1) << 8) |
                                 static_cast<uint32_t>(freq0));
}

bool Elero::reinit_frequency(uint8_t freq2, uint8_t freq1, uint8_t freq0) {
  if (!this->tx_queue_) return false;
  // Send frequency change request to radio task with synchronous response
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
  // Wait up to 5 seconds for radio task to process
  xSemaphoreTake(sem, pdMS_TO_TICKS(5000));
  vSemaphoreDelete(sem);
  return result;
}

bool Elero::reinit_frequency_mhz(float mhz) {
  // Convert MHz to CC1101 FREQ register values:
  // FREQ = mhz * 65536 / 26.0
  uint32_t freq_word = static_cast<uint32_t>(mhz * 65536.0f / 26.0f + 0.5f);
  uint8_t f2 = (freq_word >> 16) & 0xFF;
  uint8_t f1 = (freq_word >> 8) & 0xFF;
  uint8_t f0 = freq_word & 0xFF;
  return this->reinit_frequency(f2, f1, f0);
}

void Elero::flush_and_rx() {
  ESP_LOGVV(TAG, "flush_and_rx");
  this->radio_->standby();  // blocks until IDLE (~1ms)
  // Verify we actually reached IDLE — standby() may fail silently if the
  // CC1101 is unresponsive (SPI returns 0xFF).
  uint8_t marc = this->read_status(CC1101_MARCSTATE) & 0x1F;
  if (marc != CC1101_MARCSTATE_IDLE) {
    // Force IDLE via direct strobe as fallback
    this->write_cmd(CC1101_SIDLE);
    delay_microseconds_safe(500);
  }
  this->write_cmd(CC1101_SFRX);
  this->write_cmd(CC1101_SFTX);
  this->write_cmd(CC1101_SRX);
  this->rx_ready_.store(false, std::memory_order_release);
}

void Elero::flush_rx() {
  ESP_LOGVV(TAG, "flush_rx");
  this->radio_->standby();  // blocks until IDLE (~1ms)
  // Verify we actually reached IDLE — standby() may fail silently if the
  // CC1101 is unresponsive (SPI returns 0xFF).
  uint8_t marc = this->read_status(CC1101_MARCSTATE) & 0x1F;
  if (marc != CC1101_MARCSTATE_IDLE) {
    // Force IDLE via direct strobe as fallback
    this->write_cmd(CC1101_SIDLE);
    delay_microseconds_safe(500);
  }
  this->write_cmd(CC1101_SFRX);
  this->write_cmd(CC1101_SRX);
  this->rx_ready_.store(false, std::memory_order_release);
}

void Elero::reset() {
  // Software reset via command strobes — direct SPI for timing control.
  // The CC1101 datasheet specifies that after SRES, the chip needs up to
  // ~1ms to complete its internal reset sequence before it can accept
  // further commands.
  this->enable();
  this->transfer_byte(CC1101_SRES);
  delay_microseconds_safe(5000);  // 5ms — give CC1101 crystal oscillator time to stabilize after SRES
  this->transfer_byte(CC1101_SIDLE);
  delay_microseconds_safe(100);
  this->disable();
}

bool Elero::init() {
  // Early SPI health check: write one register and verify readback.
  // Retry up to 5 times with exponential backoff — after reset(), some CC1101
  // modules need additional stabilization time before SPI responds correctly.
  // RadioLib's SPIsetRegValue does read-before-write + verify, so each attempt
  // involves 3 SPI transactions.
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
      uint32_t delay_us = 2000u * (1u << (attempt - 1));  // 2ms, 4ms, 8ms, 16ms
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
  // Command strobes are single-byte SPI transactions — use direct SPI
  // since RadioLib's Module class has no dedicated command-strobe method.
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

// wait_tx() and wait_tx_done() have been replaced by the non-blocking
// advance_tx() state machine.  transmit() is no longer needed — callers
// use send_command() which kicks off the state machine instead.

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
  // CC1101 status registers require the READ_BURST flag (0xC0) to distinguish
  // them from command strobes at the same addresses (0x30-0x3D).
  // Use direct SPI for exact control in the TX state machine critical path.
  this->enable();
  this->transfer_byte(addr | CC1101_READ_BURST);
  uint8_t data = this->transfer_byte(0x00);
  this->disable();
  delay_microseconds_safe(SPI_SETTLE_US);
  return data;
}

void Elero::read_buf(uint8_t addr, uint8_t *buf, uint8_t len) {
  // FIFO reads (0x3F) use burst mode for multi-byte access.
  // Direct SPI for exact control in the RX processing critical path.
  this->enable();
  this->transfer_byte(addr | CC1101_READ_BURST);
  for (uint8_t i = 0; i < len; i++)
    buf[i] = this->transfer_byte(0x00);
  this->disable();
  delay_microseconds_safe(SPI_SETTLE_US);
}

uint8_t Elero::count_bits(uint8_t byte)
{
  uint8_t i;
  uint8_t ones = 0;
  uint8_t mask = 1;

  for( i = 0; i < 8; i++ )
  {
    if( mask & byte )
    {
      ones += 1;
    }

    mask <<= 1;
  }

  return ones & 0x01;
}


void Elero::calc_parity(uint8_t* msg)
{
  uint8_t i;
  uint8_t p = 0;

  for( i = 0; i < 4; i++ )
  {
    uint8_t a = count_bits( msg[0 + i*2] );
    uint8_t b = count_bits( msg[1 + i*2] );

    p |= a ^ b;
    p <<= 1;
  }

  msg[7] = (p << 3);
}

void Elero::add_r20_to_nibbles(uint8_t* msg, uint8_t r20, uint8_t start, uint8_t length)
{
  uint8_t i;

  for( i = start; i < length; i++ )
  {
    uint8_t d = msg[i];

    uint8_t ln = (d + r20) & 0x0F;
    uint8_t hn = ((d & 0xF0) + (r20 & 0xF0)) & 0xFF;

    msg[i] = hn | ln;

    r20 = (r20 - 0x22) & 0xFF;
  }
}

void Elero::sub_r20_from_nibbles(uint8_t* msg, uint8_t r20, uint8_t start, uint8_t length)
{
  uint8_t i;

  for(i = start; i < length; i++)
  {
    uint8_t d = msg[i];

    uint8_t ln = (d - r20) & 0x0F;
    uint8_t hn = ((d & 0xF0) - (r20 & 0xF0)) & 0xFF;

    msg[i] = hn | ln;

    r20 = (r20 - 0x22) & 0xFF;
  }
}

void Elero::xor_2byte_in_array_encode(uint8_t* msg, uint8_t xor0, uint8_t xor1)
{
  uint8_t i;

  for( i = 1; i < 4; i++ )
  {
    msg[i*2 + 0] = msg[i*2 + 0] ^ xor0;
    msg[i*2 + 1] = msg[i*2 + 1] ^ xor1;
  }
}

void Elero::xor_2byte_in_array_decode(uint8_t* msg, uint8_t xor0, uint8_t xor1)
{
  uint8_t i;

  for( i = 0; i < 4; i++ )
  {
    msg[i*2 + 0] = msg[i*2 + 0] ^ xor0;
    msg[i*2 + 1] = msg[i*2 + 1] ^ xor1;
  }
}

void Elero::encode_nibbles(uint8_t* msg)
{
  uint8_t i;

  for( i = 0; i < 8; i++ )
  {
    uint8_t nh = (msg[i] >> 4) & 0x0F;
    uint8_t nl = msg[i] & 0x0F;

    uint8_t dh = flash_table_encode[nh];
    uint8_t dl = flash_table_encode[nl];

    msg[i] = ((dh << 4) & 0xFF) | ((dl) & 0xFF);
  }
}

void Elero::decode_nibbles(uint8_t* msg, uint8_t len)
{
  uint8_t i;

  for( i = 0; i < len; i++ )
  {
    uint8_t nh = (msg[i] >> 4) & 0x0F;
    uint8_t nl = msg[i] & 0x0F;

    uint8_t dh = flash_table_decode[nh];
    uint8_t dl = flash_table_decode[nl];

    msg[i] = ((dh << 4) & 0xFF) | ((dl) & 0xFF);
  }
}

void Elero::msg_decode(uint8_t *msg) {
  decode_nibbles(msg, 8);
  sub_r20_from_nibbles(msg, 0xFE, 0, 2);
  xor_2byte_in_array_decode(msg, msg[0], msg[1]);
  sub_r20_from_nibbles(msg, 0xBA, 2, 8);
}

void Elero::msg_encode(uint8_t* msg) {
  uint8_t xor0 = msg[0];
  uint8_t xor1 = msg[1];
  calc_parity(msg);
  add_r20_to_nibbles(msg, 0xFE, 0, 8);
  xor_2byte_in_array_encode(msg, xor0, xor1);
  encode_nibbles(msg);
}

void Elero::interpret_msg() {
  uint8_t length = this->msg_rx_[0];
  // Sanity check
  if(length > ELERO_MAX_PACKET_SIZE) {
    uint8_t dump_len = (length <= (uint8_t)(CC1101_FIFO_LENGTH - 3)) ? (length + 3) : CC1101_FIFO_LENGTH;
    ESP_LOGE(TAG, "Received invalid packet: too long (%d)", length);
    ESP_LOGD(TAG, "  Raw [%d bytes]: %s", dump_len,
             format_hex_pretty(this->msg_rx_, dump_len).c_str());
    if (this->packet_dump_pending_update_) {
      this->mark_last_raw_packet_(false, "too_long");
      this->packet_dump_pending_update_ = false;
    }
    return;
  }

  // Minimum length: header fields extend to msg_rx_[16] (num_dests), so the
  // payload portion (length) must be at least 17 bytes.  Shorter packets are
  // non-Elero RF noise on the shared 868 MHz band — expected and harmless.
  static const uint8_t ELERO_MIN_PACKET_SIZE = 17;
  if (length < ELERO_MIN_PACKET_SIZE) {
    ESP_LOGD(TAG, "Received non-Elero packet: too short (%d bytes)", length);
    if (this->packet_dump_pending_update_) {
      this->mark_last_raw_packet_(false, "too_short");
      this->packet_dump_pending_update_ = false;
    }
    return;
  }

  uint8_t cnt = this->msg_rx_[1];
  uint8_t typ = this->msg_rx_[2];
  uint8_t typ2 = this->msg_rx_[3];
  uint8_t hop = this->msg_rx_[4];
  uint8_t syst = this->msg_rx_[5];
  uint8_t chl = this->msg_rx_[6];
  uint32_t src = ((uint32_t)this->msg_rx_[7] << 16) | ((uint32_t)this->msg_rx_[8] << 8) | (this->msg_rx_[9]);
  uint32_t bwd = ((uint32_t)this->msg_rx_[10] << 16) | ((uint32_t)this->msg_rx_[11] << 8) | (this->msg_rx_[12]);
  uint32_t fwd = ((uint32_t)this->msg_rx_[13] << 16) | ((uint32_t)this->msg_rx_[14] << 8) | (this->msg_rx_[15]);
  uint8_t num_dests = this->msg_rx_[16];
  uint32_t dst;
  uint8_t dests_len;

  // Validate destination count before multiplication to prevent overflow.
  // Max safe value: destinations start at byte 17, followed by payload accessed
  // up to byte 26 + dests_len.  For 3-byte dests: max = (ELERO_MAX_PACKET_SIZE - 27) / 3 = 10.
  static const uint8_t MAX_SAFE_DESTS = (ELERO_MAX_PACKET_SIZE - 27) / 3;
  if (num_dests > MAX_SAFE_DESTS) {
    ESP_LOGW(TAG, "Received invalid packet: too many destinations (%d)", num_dests);
    ESP_LOGW(TAG, "  Raw [%d bytes]: %s", length + 3,
             format_hex_pretty(this->msg_rx_, length + 3).c_str());
    if (this->packet_dump_pending_update_) {
      this->mark_last_raw_packet_(false, "too_many_dests");
      this->packet_dump_pending_update_ = false;
    }
    return;
  }

  if(typ > 0x60) {
    dests_len = num_dests * 3;
    dst = ((uint32_t)this->msg_rx_[17] << 16) | ((uint32_t)this->msg_rx_[18] << 8) | (this->msg_rx_[19]);
  } else {
    dests_len = num_dests;
    dst = this->msg_rx_[17];
  }

  // Sanity check: msg_decode accesses 8 bytes at msg_rx_[19 + dests_len],
  // so the highest index touched is 26 + dests_len. This must be within both
  // the packet (length) and the FIFO buffer.
  if((uint16_t)(26 + dests_len) > length || (uint16_t)(26 + dests_len) >= CC1101_FIFO_LENGTH) {
    ESP_LOGW(TAG, "Received invalid packet: dests_len too long (%d) for length %d", dests_len, length);
    ESP_LOGW(TAG, "  Raw [%d bytes]: %s", length + 3,
             format_hex_pretty(this->msg_rx_, length + 3).c_str());
    if (this->packet_dump_pending_update_) {
      this->mark_last_raw_packet_(false, "dests_len_too_long");
      this->packet_dump_pending_update_ = false;
    }
    return;
  }

  // RSSI and LQI are appended by CC1101 after packet data at indices length+1 and length+2
  if ((uint16_t)(length + 2) >= CC1101_FIFO_LENGTH) {
    ESP_LOGW(TAG, "Received invalid packet: RSSI/LQI out of buffer bounds (length=%d)", length);
    ESP_LOGW(TAG, "  Raw [%d bytes]: %s", CC1101_FIFO_LENGTH,
             format_hex_pretty(this->msg_rx_, CC1101_FIFO_LENGTH).c_str());
    if (this->packet_dump_pending_update_) {
      this->mark_last_raw_packet_(false, "rssi_oob");
      this->packet_dump_pending_update_ = false;
    }
    return;
  }

  uint8_t payload1 = this->msg_rx_[17 + dests_len];
  uint8_t payload2 = this->msg_rx_[18 + dests_len];
  uint8_t crc = this->msg_rx_[length + 2] >> 7;
  uint8_t lqi = this->msg_rx_[length + 2] & 0x7f;

  // Calculate RSSI in dBm (CC1101 transmits as two's complement encoded value)
  float rssi;
  uint8_t rssi_raw = this->msg_rx_[length + 1];
  if (rssi_raw > ELERO_RSSI_SIGN_BIT) {
    // Negative value (two's complement): convert from two's complement
    rssi = static_cast<float>(static_cast<int8_t>(rssi_raw)) / ELERO_RSSI_DIVISOR + ELERO_RSSI_OFFSET;
  } else {
    // Positive value
    rssi = static_cast<float>(rssi_raw) / ELERO_RSSI_DIVISOR + ELERO_RSSI_OFFSET;
  }
  uint8_t *payload = &this->msg_rx_[19 + dests_len];
  msg_decode(payload);
  if (this->packet_dump_pending_update_) {
    this->mark_last_raw_packet_(true, nullptr);
    this->packet_dump_pending_update_ = false;
  }
  this->rx_count_.fetch_add(1, std::memory_order_relaxed);
  ESP_LOGD(TAG, "rcv'd from 0x%06x: state=0x%02x rssi=%.1f", src, payload[6], rssi);
  ESP_LOGV(TAG, "rcv'd: len=%02d, cnt=%02d, typ=0x%02x, typ2=0x%02x, hop=0x%02x, syst=0x%02x, chl=%02d, src=0x%06x, bwd=0x%06x, fwd=0x%06x, #dst=%02d, dst=0x%06x, rssi=%2.1f, lqi=%2d, crc=%2d, payload=[0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x]", length, cnt, typ, typ2, hop, syst, chl, src, bwd, fwd, num_dests, dst, rssi, lqi, crc, payload1, payload2, payload[0], payload[1], payload[2], payload[3], payload[4], payload[5], payload[6], payload[7]);

  // Build RxResult and push to queue for Core 1 dispatch.
  // Deduplication of status packets stays here (Core 0) to avoid queue spam.
  bool is_status = (typ == 0xca) || (typ == 0xc9);
  bool is_command = (typ == 0x6a) || (typ == 0x69);

  if (is_status && this->is_duplicate_packet_(src, cnt)) {
    ESP_LOGV(TAG, "Duplicate status from 0x%06x cnt=%d (relay hop), skipping", src, cnt);
    return;
  }

  RxResult rx{};
  rx.blind_address = src;
  rx.remote_address = is_status ? fwd : src;  // status: fwd=remote; command: src=remote
  rx.channel = chl;
  rx.pck_inf[0] = typ;
  rx.pck_inf[1] = typ2;
  rx.hop = hop;
  rx.state = payload[6];
  rx.rssi = rssi;
  rx.timestamp_ms = millis();
  memcpy(rx.payload, payload, 10);
  rx.cnt = cnt;
  rx.is_status = is_status;
  rx.is_command = is_command;
  rx.payload_1 = payload1;
  rx.payload_2 = payload2;

  // Populate discovery data
  rx.scan_hit = this->scan_mode_.load(std::memory_order_acquire) && (is_status || is_command);
  rx.params_from_command = is_command;

  // For command packets, extract destination addresses
  rx.num_dests = 0;
  rx.is_own_echo = false;
  if (is_command) {
    rx.is_own_echo = (this->own_remote_addresses_.count(src) > 0);
    uint8_t safe_num = (num_dests > 10) ? 10 : num_dests;
    rx.num_dests = safe_num;
    for (uint8_t i = 0; i < safe_num; i++) {
      if (typ > 0x60) {  // 3-byte addressing
        rx.dest_addrs[i] = ((uint32_t)this->msg_rx_[17 + i * 3] << 16) |
                            ((uint32_t)this->msg_rx_[18 + i * 3] << 8) |
                            this->msg_rx_[19 + i * 3];
      } else {            // 1-byte addressing
        rx.dest_addrs[i] = this->msg_rx_[17 + i];
      }
    }
  }

  // Push to RX queue — drop if full (stale data is acceptable)
  if (this->rx_queue_) {
    if (xQueueSend(this->rx_queue_, &rx, 0) != pdTRUE) {
      ESP_LOGW(TAG, "RX queue full, dropping packet from 0x%06x", src);
    }
  }
}

void Elero::register_cover(EleroBlindBase *cover) {
  uint32_t address = cover->get_blind_address();
  if(this->address_to_cover_mapping_.find(address) != this->address_to_cover_mapping_.end()) {
    ESP_LOGE(TAG, "A blind with this address is already registered - this is currently not supported");
    return;
  }
  this->address_to_cover_mapping_.insert({address, cover});
  this->own_remote_addresses_.insert(cover->get_remote_address());
  cover->set_poll_offset((this->address_to_cover_mapping_.size() - 1) * ELERO_POLL_STAGGER_MS);
}

void Elero::register_light(EleroLightBase *light) {
  uint32_t address = light->get_blind_address();
  if(this->address_to_light_mapping_.find(address) != this->address_to_light_mapping_.end()) {
    ESP_LOGE(TAG, "A light with this address is already registered - this is currently not supported");
    return;
  }
  this->address_to_light_mapping_.insert({address, light});
  this->own_remote_addresses_.insert(light->get_remote_address());
}

#ifdef USE_SENSOR
void Elero::register_rssi_sensor(uint32_t address, sensor::Sensor *sensor) {
  this->address_to_rssi_sensor_[address] = sensor;
}
#endif

#ifdef USE_TEXT_SENSOR
void Elero::register_text_sensor(uint32_t address, text_sensor::TextSensor *sensor) {
  this->address_to_text_sensor_[address] = sensor;
}

void Elero::publish_text_sensor_state(uint32_t address, const std::string &state) {
  auto it = this->address_to_text_sensor_.find(address);
  if (it != this->address_to_text_sensor_.end()) {
    it->second->publish_state(state);
  }
}
#endif

void Elero::start_packet_dump() {
  packet_dump_mode_.store(true, std::memory_order_release);
  ESP_LOGI(TAG, "Packet dump mode started");
}

void Elero::stop_packet_dump() {
  packet_dump_mode_.store(false, std::memory_order_release);
  ESP_LOGI(TAG, "Packet dump mode stopped");
}

void Elero::clear_raw_packets() {
  std::lock_guard<std::mutex> lock(packet_dump_mutex_);
  raw_packets_.clear();
  raw_packet_write_idx_ = 0;
}

void Elero::capture_raw_packet_(uint8_t fifo_len) {
  uint8_t actual_len = (fifo_len > CC1101_FIFO_LENGTH) ? CC1101_FIFO_LENGTH : fifo_len;
  RawPacket pkt{};
  pkt.timestamp_ms = millis();
  pkt.fifo_len = actual_len;
  memcpy(pkt.data, this->msg_rx_, actual_len);
  pkt.valid = false;
  pkt.reject_reason[0] = '\0';

  std::lock_guard<std::mutex> lock(packet_dump_mutex_);
  if (raw_packets_.size() < ELERO_MAX_RAW_PACKETS) {
    raw_packets_.push_back(pkt);
    raw_packet_write_idx_ = (uint16_t)(raw_packets_.size() - 1);
  } else {
    raw_packet_write_idx_ = (raw_packet_write_idx_ + 1) % ELERO_MAX_RAW_PACKETS;
    raw_packets_[raw_packet_write_idx_] = pkt;
  }
}

void Elero::mark_last_raw_packet_(bool valid, const char *reason) {
  std::lock_guard<std::mutex> lock(packet_dump_mutex_);
  if (raw_packets_.empty()) return;
  auto &pkt = raw_packets_[raw_packet_write_idx_];
  pkt.valid = valid;
  if (!valid && reason != nullptr) {
    strncpy(pkt.reject_reason, reason, sizeof(pkt.reject_reason) - 1);
    pkt.reject_reason[sizeof(pkt.reject_reason) - 1] = '\0';
  }
}

void Elero::track_discovered_blind(uint32_t src, uint32_t remote, uint8_t channel,
                                    uint8_t pck_inf0, uint8_t pck_inf1, uint8_t hop,
                                    uint8_t payload_1, uint8_t payload_2,
                                    float rssi, uint8_t state, bool from_command) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  // Check if already tracked
  for (auto &blind : this->discovered_blinds_) {
    if (blind.blind_address == src) {
      blind.rssi = rssi;
      blind.last_seen = millis();
      if (state != 0) blind.last_state = state;
      blind.times_seen++;
      // Upgrade CA-derived params with command-packet params (higher quality):
      // a 6a/69 command packet tells us the exact format the remote uses, so
      // those values must be preferred over what the blind's own CA responses
      // carry (CA channel/hop/pck_inf describe the response format, not the
      // command format).
      if (from_command && !blind.params_from_command) {
        blind.remote_address = remote;
        blind.channel = channel;
        blind.pck_inf[0] = pck_inf0;
        blind.pck_inf[1] = pck_inf1;
        blind.hop = hop;
        blind.payload_1 = payload_1;
        blind.payload_2 = payload_2;
        blind.params_from_command = true;
        ESP_LOGI(TAG, "Upgraded blind 0x%06x params from command packet: ch=%d, pck_inf=0x%02x/0x%02x, hop=0x%02x, payload=0x%02x/0x%02x",
                 src, channel, pck_inf0, pck_inf1, hop, payload_1, payload_2);
      }
      return;
    }
  }
  // Add new entry
  if (this->discovered_blinds_.size() < ELERO_MAX_DISCOVERED) {
    DiscoveredBlind blind{};
    blind.blind_address = src;
    blind.remote_address = remote;
    blind.channel = channel;
    blind.pck_inf[0] = pck_inf0;
    blind.pck_inf[1] = pck_inf1;
    blind.hop = hop;
    blind.payload_1 = payload_1;
    blind.payload_2 = payload_2;
    blind.rssi = rssi;
    blind.last_seen = millis();
    blind.last_state = state;
    blind.times_seen = 1;
    blind.params_from_command = from_command;
    this->discovered_blinds_.push_back(blind);
    ESP_LOGI(TAG, "Discovered new device: addr=0x%06x, remote=0x%06x, ch=%d, rssi=%.1f, src=%s",
             src, remote, channel, rssi, from_command ? "cmd_pkt" : "status_pkt");
  }
}

// ---------------------------------------------------------------------------
// send_command — public API (Core 1): enqueue a TX command to the radio task
// ---------------------------------------------------------------------------
bool Elero::send_command(t_elero_command *cmd) {
  if (this->spi_failed_.load(std::memory_order_acquire))
    return false;
  if (!this->tx_queue_)
    return false;

  RadioMessage msg{};
  msg.type = RadioControlType::TX_COMMAND;
  msg.tx.cmd = *cmd;
  if (xQueueSend(this->tx_queue_, &msg, pdMS_TO_TICKS(10)) == pdTRUE) {
    return true;
  }
  ESP_LOGW(TAG, "TX queue full, command to 0x%06x dropped", cmd->blind_addr);
  this->tx_drop_count_.fetch_add(1, std::memory_order_relaxed);
  return false;
}

// ---------------------------------------------------------------------------
// send_command_priority — public API (Core 1): enqueue a high-priority TX
// command (e.g. stop) that bypasses the normal queue.
// ---------------------------------------------------------------------------
bool Elero::send_command_priority(t_elero_command *cmd) {
  if (this->spi_failed_.load(std::memory_order_acquire))
    return false;
  if (!this->tx_priority_queue_)
    return false;

  RadioMessage msg{};
  msg.type = RadioControlType::TX_COMMAND;
  msg.tx.cmd = *cmd;
  if (xQueueSend(this->tx_priority_queue_, &msg, pdMS_TO_TICKS(10)) == pdTRUE) {
    return true;
  }
  ESP_LOGW(TAG, "Priority TX queue full, command to 0x%06x dropped", cmd->blind_addr);
  this->tx_drop_count_.fetch_add(1, std::memory_order_relaxed);
  return false;
}

// ---------------------------------------------------------------------------
// send_command_internal_ — Core 0 only: execute TX via SPI
// ---------------------------------------------------------------------------
bool Elero::send_command_internal_(t_elero_command *cmd) {
  // Reject if SPI is permanently broken or TX is already in progress.
  if (this->spi_failed_.load(std::memory_order_acquire))
    return false;
  if (this->tx_state_.load(std::memory_order_acquire) != TxState::IDLE)
    return false;

  ESP_LOGVV(TAG, "send_command called");
  uint16_t code = (0x00 - (cmd->counter * ELERO_CRYPTO_MULT)) & ELERO_CRYPTO_MASK;
  this->msg_tx_[0] = ELERO_MSG_LENGTH;
  this->msg_tx_[1] = cmd->counter; // message counter
  this->msg_tx_[2] = cmd->pck_inf[0];
  this->msg_tx_[3] = cmd->pck_inf[1];
  this->msg_tx_[4] = cmd->hop; // hop info
  this->msg_tx_[5] = ELERO_SYS_ADDR;
  this->msg_tx_[6] = cmd->channel; // channel
  this->msg_tx_[7] = ((cmd->remote_addr >> 16) & 0xff); // source address
  this->msg_tx_[8] = ((cmd->remote_addr >> 8) & 0xff);
  this->msg_tx_[9] =((cmd->remote_addr) & 0xff);
  this->msg_tx_[10] = ((cmd->remote_addr >> 16) & 0xff); // backward address
  this->msg_tx_[11] = ((cmd->remote_addr >> 8) & 0xff);
  this->msg_tx_[12] =((cmd->remote_addr) & 0xff);
  this->msg_tx_[13] = ((cmd->remote_addr >> 16) & 0xff); // forward address
  this->msg_tx_[14] = ((cmd->remote_addr >> 8) & 0xff);
  this->msg_tx_[15] =((cmd->remote_addr) & 0xff);
  this->msg_tx_[16] = ELERO_DEST_COUNT;
  this->msg_tx_[17] = ((cmd->blind_addr >> 16) & 0xff); // blind address
  this->msg_tx_[18] = ((cmd->blind_addr >> 8) & 0xff);
  this->msg_tx_[19] = ((cmd->blind_addr) & 0xff);
  for(int i=0; i<10; i++)
    this->msg_tx_[20 + i] = cmd->payload[i];
  this->msg_tx_[22] = ((code >> 8) & 0xff);
  this->msg_tx_[23] = (code & 0xff);

  uint8_t *payload = &this->msg_tx_[22];
  msg_encode(payload);

  ESP_LOGD(TAG, "send to 0x%06x: cmd=0x%02x ch=%02d cnt=%02d",
           cmd->blind_addr, cmd->payload[4], cmd->channel, cmd->counter);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  ESP_LOGV(TAG, "  TX raw [%d bytes]: %s", ELERO_MSG_LENGTH + 1,
           format_hex_pretty(this->msg_tx_, ELERO_MSG_LENGTH + 1).c_str());
#endif

  // Synchronous TX initiation: RadioLib's standby() blocks until IDLE (~1ms),
  // then we flush, load FIFO, and send STX.  Going to IDLE first so STX is
  // not subject to CCA (Elero motors actively transmit, causing CCA failures).
  this->radio_->standby();

  // Verify we actually reached IDLE — if not, radio is unresponsive.
  // Track consecutive reinit failures to detect permanent SPI breakage early
  // (e.g. GPIO12 strapping pin) without waiting for the 5-second watchdog.
  uint8_t marc = this->read_status(CC1101_MARCSTATE) & 0x1F;
  if (marc != CC1101_MARCSTATE_IDLE) {
    this->send_cmd_reinit_failures_++;
    if (this->send_cmd_reinit_failures_ >= 3) {
      // SPI is permanently broken — stop retrying to avoid log spam.
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
    if (!this->init()) {
      // init() SPI health check failed — escalate failure count so we bail
      // faster on the next call instead of spamming 25+ register-write warnings.
      this->send_cmd_reinit_failures_++;
    }
    return false;
  }
  this->send_cmd_reinit_failures_ = 0;

  // Flush both FIFOs (valid in IDLE state per CC1101 spec): TX for a clean
  // slate, RX to discard any partial packet data from the reception that
  // SIDLE interrupted.  Batched: two strobes with a single trailing delay.
  this->enable();
  this->transfer_byte(CC1101_SFTX);
  this->disable();
  this->enable();
  this->transfer_byte(CC1101_SFRX);
  this->disable();
  delay_microseconds_safe(SPI_SETTLE_US);
  this->rx_ready_.store(false, std::memory_order_release);

  // Load TX FIFO and start transmission
  delay_microseconds_safe(20);  // FIFO clear settles in <1 SPI clock; 20µs is ample margin
  this->write_burst(CC1101_TXFIFO, this->msg_tx_, this->msg_tx_[0] + 1);
  this->write_cmd(CC1101_STX);

  this->tx_state_.store(TxState::TRANSMITTING, std::memory_order_release);
  this->tx_state_entered_ms_ = millis();
  return true;
}

// ---------------------------------------------------------------------------
// dispatch_rx_result_ — route a decoded RX result to entities, sensors,
// discovery, and runtime blinds.  Runs on Core 1 (main loop).
// Prepared for dual-core: in the future, interpret_msg() will populate an
// RxResult on Core 0 and push it to the RX queue; this method will drain
// the queue on Core 1.  For now it is unused (interpret_msg() dispatches
// directly), but included to verify the struct and logic compile cleanly.
// ---------------------------------------------------------------------------
void Elero::dispatch_rx_result_(const RxResult &rx) {
  // 1. RSSI sensor update
#ifdef USE_SENSOR
  {
    auto rssi_it = this->address_to_rssi_sensor_.find(rx.blind_address);
    if (rssi_it != this->address_to_rssi_sensor_.end()) {
      rssi_it->second->publish_state(rx.rssi);
    }
  }
#endif

  // 2. Discovery tracking (scan mode)
  if (rx.scan_hit) {
    this->track_discovered_blind(rx.blind_address, rx.remote_address, rx.channel,
                                  rx.pck_inf[0], rx.pck_inf[1], rx.hop,
                                  rx.payload_1, rx.payload_2,
                                  rx.rssi, rx.state, rx.params_from_command);
    // For command packets, also track each destination
    if (rx.is_command) {
      for (uint8_t i = 0; i < rx.num_dests; i++) {
        this->track_discovered_blind(rx.dest_addrs[i], rx.blind_address, rx.channel,
                                      rx.pck_inf[0], rx.pck_inf[1], rx.hop,
                                      rx.payload_1, rx.payload_2,
                                      rx.rssi, 0, true);
      }
    }
  }

  // 3. Status packets (0xca/0xc9): dispatch state to entities
  //    Deduplication already handled on Core 0 in interpret_msg().
  if (rx.is_status) {

#ifdef USE_TEXT_SENSOR
    {
      auto text_it = this->address_to_text_sensor_.find(rx.blind_address);
      if (text_it != this->address_to_text_sensor_.end()) {
        text_it->second->publish_state(elero_state_to_string(rx.state));
      }
    }
#endif

    auto search = this->address_to_cover_mapping_.find(rx.blind_address);
    if (search != this->address_to_cover_mapping_.end()) {
      search->second->notify_rx_meta(rx.timestamp_ms, rx.rssi);
      search->second->set_rx_state(rx.state);
    }

    auto light_search = this->address_to_light_mapping_.find(rx.blind_address);
    if (light_search != this->address_to_light_mapping_.end()) {
      light_search->second->notify_rx_meta(rx.timestamp_ms, rx.rssi);
      light_search->second->set_rx_state(rx.state);
    }

    // Update runtime adopted blinds
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      auto it = this->runtime_blinds_.find(rx.blind_address);
      if (it != this->runtime_blinds_.end()) {
        it->second.last_seen_ms = rx.timestamp_ms;
        it->second.last_rssi = rx.rssi;
        it->second.last_state = rx.state;
        this->update_runtime_blind_direction_(it->second, rx.state);
      }
    }
  } else {
    // Non-status packets: still update RSSI/last_seen for known blinds
    auto search = this->address_to_cover_mapping_.find(rx.blind_address);
    if (search != this->address_to_cover_mapping_.end()) {
      search->second->notify_rx_meta(rx.timestamp_ms, rx.rssi);
    }
    auto light_search = this->address_to_light_mapping_.find(rx.blind_address);
    if (light_search != this->address_to_light_mapping_.end()) {
      light_search->second->notify_rx_meta(rx.timestamp_ms, rx.rssi);
    }
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      auto rb_it = this->runtime_blinds_.find(rx.blind_address);
      if (rb_it != this->runtime_blinds_.end()) {
        rb_it->second.last_seen_ms = rx.timestamp_ms;
        rb_it->second.last_rssi = rx.rssi;
      }
    }

    // Remote command packets: trigger immediate polls
    if (rx.is_command && !rx.is_own_echo) {
      for (uint8_t i = 0; i < rx.num_dests; i++) {
        auto c_it = this->address_to_cover_mapping_.find(rx.dest_addrs[i]);
        if (c_it != this->address_to_cover_mapping_.end()) {
          c_it->second->schedule_immediate_poll();
        }
        auto l_it = this->address_to_light_mapping_.find(rx.dest_addrs[i]);
        if (l_it != this->address_to_light_mapping_.end()) {
          l_it->second->schedule_immediate_poll();
        }
      }
    }
  }
}

// ─── Runtime blind adoption ───────────────────────────────────────────────

bool Elero::adopt_blind(const DiscoveredBlind &discovered, const std::string &name,
                        DeviceType type) {
  if (this->is_cover_configured(discovered.blind_address))
    return false;
  if (this->is_light_configured(discovered.blind_address))
    return false;
  std::lock_guard<std::mutex> lock(state_mutex_);
  if (this->runtime_blinds_.find(discovered.blind_address) != this->runtime_blinds_.end())
    return false;
  RuntimeBlind rb{};
  rb.blind_address = discovered.blind_address;
  rb.remote_address = discovered.remote_address;
  rb.channel = discovered.channel;
  rb.pck_inf[0] = discovered.pck_inf[0];
  rb.pck_inf[1] = discovered.pck_inf[1];
  rb.hop = discovered.hop;
  rb.payload_1 = discovered.payload_1;
  rb.payload_2 = discovered.payload_2;
  rb.name = name.empty() ? "Adopted" : name;
  rb.device_type = type;
  rb.last_seen_ms = discovered.last_seen;
  rb.last_rssi = discovered.rssi;
  rb.last_state = discovered.last_state;
  this->runtime_blinds_.insert({discovered.blind_address, std::move(rb)});
  this->own_remote_addresses_.insert(discovered.remote_address);
  ESP_LOGI(TAG, "Adopted runtime %s 0x%06x as \"%s\"",
           type == DeviceType::LIGHT ? "light" : "blind",
           discovered.blind_address, rb.name.c_str());
  return true;
}

bool Elero::remove_runtime_blind(uint32_t addr) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  auto it = this->runtime_blinds_.find(addr);
  if (it != this->runtime_blinds_.end()) {
    ESP_LOGI(TAG, "Removed runtime blind 0x%06x", addr);
    this->runtime_blinds_.erase(it);
    return true;
  }
  return false;
}

bool Elero::send_runtime_command(uint32_t addr, uint8_t cmd_byte) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  auto it = this->runtime_blinds_.find(addr);
  if (it != this->runtime_blinds_.end()) {
    if (it->second.command_queue.size() < ELERO_MAX_COMMAND_QUEUE) {
      it->second.command_queue.push(cmd_byte);
      return true;
    }
    return false;  // Queue full
  }
  return false;
}

bool Elero::update_runtime_blind_settings(uint32_t addr, uint32_t open_dur_ms,
                                          uint32_t close_dur_ms, uint32_t poll_intvl_ms) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  auto it = this->runtime_blinds_.find(addr);
  if (it != this->runtime_blinds_.end()) {
    it->second.open_duration_ms = open_dur_ms;
    it->second.close_duration_ms = close_dur_ms;
    it->second.poll_intvl_ms = poll_intvl_ms;
    return true;
  }
  return false;
}

bool Elero::is_blind_adopted(uint32_t addr) const {
  std::lock_guard<std::mutex> lock(state_mutex_);
  return this->runtime_blinds_.find(addr) != this->runtime_blinds_.end();
}

// ─── Log buffer ───────────────────────────────────────────────────────────

void Elero::append_log(uint8_t level, const char *tag, const char *fmt, ...) {
  if (!this->log_capture_) return;
  LogEntry entry{};
  entry.timestamp_ms = millis();
  entry.level = level;
  strncpy(entry.tag, tag, sizeof(entry.tag) - 1);
  va_list args;
  va_start(args, fmt);
  vsnprintf(entry.message, sizeof(entry.message), fmt, args);
  va_end(args);
  std::lock_guard<std::mutex> lock(this->log_mutex_);
  if (!this->log_buffer_full_ && this->log_entries_.size() < ELERO_LOG_BUFFER_SIZE) {
    this->log_entries_.push_back(entry);
  } else {
    this->log_buffer_full_ = true;
    this->log_entries_[this->log_write_idx_] = entry;
    this->log_write_idx_ = (this->log_write_idx_ + 1) % ELERO_LOG_BUFFER_SIZE;
  }
}

}  // namespace elero
}  // namespace esphome
