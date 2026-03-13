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
// Small adapter that implements the LogListener interface so the Elero hub
// can forward ESPHome log messages into its ring buffer for the web UI.
class EleroLogListener : public logger::LogListener {
 public:
  explicit EleroLogListener(Elero *parent) : parent_(parent) {}
  void on_log(uint8_t level, const char *tag, const char *message, size_t message_len) override {
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
    this->parent_->append_log(mapped, tag, "%s", message);
  }

 protected:
  Elero *parent_;
};
#endif

static const char *TAG = "elero";
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
  if (!parent->is_tx_idle()) return;

  if ((now - last_command) > parent->get_send_delay()) {
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
        ESP_LOGD(tag, "Retry #%d for 0x%06x", send_retries, blind_addr);
        send_retries++;
        if (send_retries > ELERO_SEND_RETRIES) {
          ESP_LOGE(tag, "Hit maximum retries for 0x%06x, giving up.", blind_addr);
          send_retries = 0;
          queue.pop();
        }
        last_command = now;
      }
    }
  }
}

Elero::~Elero() {
  delete this->radio_;
  this->radio_ = nullptr;
  delete this->radio_module_;
  this->radio_module_ = nullptr;
#ifdef USE_LOGGER
  delete static_cast<EleroLogListener *>(this->log_listener_);
  this->log_listener_ = nullptr;
#endif
}

void Elero::loop() {
  // Skip all processing if SPI is permanently broken (e.g. strapping pin issue).
  if (this->spi_failed_)
    return;

  // 1. ALWAYS process pending RX packets first (highest priority).
  //    Only safe when not mid-TX — process_rx() ignores stale rx_ready_
  //    flags by only running when TX is idle or in cooldown.
  TxState cur_tx = this->tx_state_.load(std::memory_order_acquire);
  if (cur_tx == TxState::IDLE || cur_tx == TxState::COOLDOWN) {
    this->process_rx();
  }

  // 2. Advance the non-blocking TX state machine (one step per loop).
  if (cur_tx != TxState::IDLE) {
    this->advance_tx();
  }

  // 3. Drain runtime blind command queues (only when TX is idle).
  if (this->tx_state_.load(std::memory_order_acquire) == TxState::IDLE) {
    this->drain_runtime_queues();
    // 3b. Enqueue periodic status polls for runtime blinds.
    this->poll_runtime_blinds_();
    // 4. Periodic radio health check — detect stuck CC1101 states.
    this->check_radio_state_();
  }

  // 5. Recompute dead-reckoning positions for runtime-adopted blinds.
  this->recompute_runtime_positions_();
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
    if (this->packet_dump_mode_) {
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
static const uint32_t TX_STATE_TIMEOUT_MS = 50;   // per-state watchdog
static const uint32_t TX_COOLDOWN_MS = 10;         // settle time after TX before next TX
static const uint32_t RADIO_WATCHDOG_MS = 5000;    // radio health check interval

void Elero::advance_tx() {
  uint32_t now = millis();
  uint32_t elapsed = now - this->tx_state_entered_ms_;

  switch (this->tx_state_) {

    case TxState::TRANSMITTING: {
      // TX completion: CC1101 auto-transitions to RX (MCSM1 TXOFF_MODE=0x3).
      // Detect by polling MARCSTATE — when it leaves TX the packet is sent.
      uint8_t marc = this->read_status(CC1101_MARCSTATE) & 0x1F;
      if (marc == CC1101_MARCSTATE_TX) {
        // Still transmitting — check for timeout
        if (elapsed > TX_STATE_TIMEOUT_MS) {
          ESP_LOGW(TAG, "TX timeout in TRANSMITTING (%lums), aborting",
                   (unsigned long) elapsed);
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
          this->tx_count_++;
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
    this->watchdog_recovery_count_++;
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
    this->watchdog_recovery_count_++;
    this->consecutive_watchdog_failures_ = 0;  // IDLE is a valid CC1101 state, not SPI failure
    this->write_cmd(CC1101_SRX);
    return;
  }

  // Any other unexpected state — full reset + reinitialize
  // flush_and_rx() is insufficient when the CC1101 is unresponsive (e.g.
  // MARCSTATE reads 0x1f = SPI returning 0xFF).  A hardware reset via
  // SRES followed by full register configuration is needed.
  ESP_LOGW(TAG, "Radio watchdog: unexpected state 0x%02x, full reinit", marc);
  this->watchdog_recovery_count_++;
  this->consecutive_watchdog_failures_++;

  // If the watchdog has failed 2+ times in a row without recovery, SPI is
  // likely permanently broken (e.g. GPIO12 strapping pin issue).  Stop
  // retrying to avoid flooding the log.
  if (this->consecutive_watchdog_failures_ >= 2) {
    ESP_LOGE(TAG, "Radio watchdog: %d consecutive failures — SPI appears permanently broken.",
             this->consecutive_watchdog_failures_);
    ESP_LOGE(TAG, "  If GPIO12 is used for SPI MISO, it may be pulling VDD_SDIO to 1.8V at boot.");
    ESP_LOGE(TAG, "  Use non-strapping pins for SPI (e.g. CLK=18, MISO=19, MOSI=23).");
    this->spi_failed_ = true;
    this->mark_failed();
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

void Elero::dump_config() {
  ESP_LOGCONFIG(TAG, "Elero CC1101:");
  LOG_PIN("  GDO0 Pin: ", this->gdo0_pin_);
  ESP_LOGCONFIG(TAG, "  freq2: 0x%02x, freq1: 0x%02x, freq0: 0x%02x", this->freq2_, this->freq1_, this->freq0_);
  ESP_LOGCONFIG(TAG, "  Send repeats: %d, send delay: %d ms", this->send_repeats_, this->send_delay_);
  ESP_LOGCONFIG(TAG, "  RadioLib: standby() + SPI register access with verify-readback");
  if (this->spi_failed_) {
    ESP_LOGCONFIG(TAG, "  SPI Status: FAILED — CC1101 communication broken");
    ESP_LOGCONFIG(TAG, "  Check SPI pin assignments — avoid ESP32 strapping pins (GPIO0/2/5/12/15)");
  }
  ESP_LOGCONFIG(TAG, "  Registered covers: %d", this->address_to_cover_mapping_.size());
}

void Elero::setup() {
  ESP_LOGI(TAG, "Setting up Elero Component...");
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
    this->mark_failed();
    return;
  } else {
    ESP_LOGW(TAG, "RadioLib begin() returned rc=%d, continuing with custom init", rc);
  }

  this->gdo0_pin_->setup();
  this->gdo0_pin_->attach_interrupt(Elero::interrupt, this, gpio::INTERRUPT_FALLING_EDGE);
  this->reset();
  if (!this->init()) {
    ESP_LOGE(TAG, "CC1101 SPI communication is broken — the radio is non-functional.");
    ESP_LOGE(TAG, "Common cause: GPIO12 is an ESP32 strapping pin that controls flash voltage.");
    ESP_LOGE(TAG, "  If GPIO12 is used as SPI MISO, the CC1101 can pull it HIGH at boot,");
    ESP_LOGE(TAG, "  setting VDD_SDIO to 1.8V and breaking all SPI communication.");
    ESP_LOGE(TAG, "  Solution: use non-strapping pins for SPI (e.g. CLK=18, MISO=19, MOSI=23).");
    ESP_LOGE(TAG, "  ESP32 strapping pins to avoid: GPIO0, GPIO2, GPIO5, GPIO12, GPIO15.");
    this->spi_failed_ = true;
    this->mark_failed();
    return;
  }

#ifdef USE_LOGGER
  // Forward all ESP_LOG messages into the ring buffer so the web UI Log tab
  // can display them when capture is enabled.
  if (logger::global_logger != nullptr) {
    this->log_listener_ = new EleroLogListener(this);
    logger::global_logger->add_log_listener(this->log_listener_);
  }
#endif
}

float Elero::registers_to_mhz(uint8_t freq2, uint8_t freq1, uint8_t freq0) {
  return (26.0f / 65536.0f) * ((static_cast<uint32_t>(freq2) << 16) |
                                (static_cast<uint32_t>(freq1) << 8) |
                                 static_cast<uint32_t>(freq0));
}

bool Elero::reinit_frequency(uint8_t freq2, uint8_t freq1, uint8_t freq0) {
  if (!this->is_tx_idle()) {
    ESP_LOGW(TAG, "Cannot reinit frequency while TX is in progress");
    return false;
  }
  this->rx_ready_.store(false, std::memory_order_release);
  this->tx_state_.store(TxState::IDLE, std::memory_order_release);
  this->freq2_ = freq2;
  this->freq1_ = freq1;
  this->freq0_ = freq0;
  this->reset();
  this->init();
  float mhz = registers_to_mhz(freq2, freq1, freq0);
  ESP_LOGI(TAG, "CC1101 re-initialised: %.2f MHz (0x%02x 0x%02x 0x%02x)", mhz, freq2, freq1, freq0);
  return true;
}

bool Elero::reinit_frequency_mhz(float mhz) {
  if (!this->is_tx_idle()) {
    ESP_LOGW(TAG, "Cannot reinit frequency while TX is in progress");
    return false;
  }
  this->rx_ready_.store(false, std::memory_order_release);
  this->tx_state_.store(TxState::IDLE, std::memory_order_release);

  // Use RadioLib's setFrequency() to set the CC1101 frequency directly in MHz.
  int16_t rc = this->radio_->setFrequency(mhz);
  if (rc != RADIOLIB_ERR_NONE) {
    ESP_LOGE(TAG, "setFrequency(%.2f MHz) failed: %d", mhz, rc);
    return false;
  }

  // Read back the register values so our cached freq bytes stay in sync.
  this->freq2_ = this->read_reg(CC1101_FREQ2);
  this->freq1_ = this->read_reg(CC1101_FREQ1);
  this->freq0_ = this->read_reg(CC1101_FREQ0);

  // Full reinit to restore all custom register settings (setFrequency may
  // alter modem config).
  this->reset();
  this->init();

  float actual_mhz = registers_to_mhz(this->freq2_, this->freq1_, this->freq0_);
  ESP_LOGI(TAG, "CC1101 re-initialised via setFrequency: %.2f MHz (0x%02x 0x%02x 0x%02x)",
           actual_mhz, this->freq2_, this->freq1_, this->freq0_);
  return true;
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
  delay_microseconds_safe(1000);
  this->transfer_byte(CC1101_SIDLE);
  delay_microseconds_safe(100);
  this->disable();
}

bool Elero::init() {
  // Early SPI health check: write one register and verify readback.
  // If SPI is broken (e.g. GPIO12 strapping pin pulling VDD_SDIO to 1.8V),
  // this catches it immediately instead of logging ~25 individual failures.
  this->write_reg(CC1101_FSCTRL1, 0x08);
  uint8_t check = this->read_reg(CC1101_FSCTRL1);
  if (check != 0x08) {
    ESP_LOGE(TAG, "init: SPI health check failed (wrote 0x08, read 0x%02x) — aborting init", check);
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
  // RadioLib SPIsetRegValue does verify-readback for config registers (0x00-0x2E).
  // Status registers and FIFOs are write-only — use raw write for those.
  if (addr <= CC1101_TEST0) {
    int16_t rc = this->radio_module_->SPIsetRegValue(addr, data);
    if (rc != RADIOLIB_ERR_NONE) {
      ESP_LOGW(TAG, "SPI write verify failed: reg=0x%02x val=0x%02x rc=%d", addr, data, rc);
    }
  } else {
    this->radio_module_->SPIwriteRegister(addr, data);
  }
}

void Elero::write_burst(uint8_t addr, uint8_t *data, uint8_t len) {
  // Use RadioLib's burst write — handles SPI framing and burst flag internally.
  // SPIwriteRegisterBurst returns void; no error code to check.
  this->radio_module_->SPIwriteRegisterBurst(addr, data, len);
}

void Elero::write_cmd(uint8_t cmd) {
  // Command strobes are single-byte SPI transactions — use direct SPI
  // since RadioLib's Module class has no dedicated command-strobe method.
  this->enable();
  this->transfer_byte(cmd);
  this->disable();
  delay_microseconds_safe(15);
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
  int16_t val = this->radio_module_->SPIgetRegValue(addr);
  if (val < 0) {
    ESP_LOGW(TAG, "SPI read failed: reg=0x%02x rc=%d", addr, val);
    return 0;
  }
  return (uint8_t) val;
}

uint8_t Elero::read_status(uint8_t addr) {
  // CC1101 status registers require the READ_BURST flag (0xC0) to distinguish
  // them from command strobes at the same addresses (0x30-0x3D).
  // Use direct SPI for exact control in the TX state machine critical path.
  this->enable();
  this->transfer_byte(addr | CC1101_READ_BURST);
  uint8_t data = this->transfer_byte(0x00);
  this->disable();
  delay_microseconds_safe(15);
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
  delay_microseconds_safe(15);
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
  this->rx_count_++;
  ESP_LOGD(TAG, "rcv'd from 0x%06x: state=0x%02x rssi=%.1f", src, payload[6], rssi);
  ESP_LOGV(TAG, "rcv'd: len=%02d, cnt=%02d, typ=0x%02x, typ2=0x%02x, hop=0x%02x, syst=0x%02x, chl=%02d, src=0x%06x, bwd=0x%06x, fwd=0x%06x, #dst=%02d, dst=0x%06x, rssi=%2.1f, lqi=%2d, crc=%2d, payload=[0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x]", length, cnt, typ, typ2, hop, syst, chl, src, bwd, fwd, num_dests, dst, rssi, lqi, crc, payload1, payload2, payload[0], payload[1], payload[2], payload[3], payload[4], payload[5], payload[6], payload[7]);

  // Update RSSI sensor for any message from a known blind
#ifdef USE_SENSOR
  {
    auto rssi_it = this->address_to_rssi_sensor_.find(src);
    if (rssi_it != this->address_to_rssi_sensor_.end()) {
      rssi_it->second->publish_state(rssi);
    }
  }
#endif

  // Track in discovery mode
  if (this->scan_mode_) {
    if ((typ == 0xca) || (typ == 0xc9)) {
      // Status response FROM the blind: src = blind addr, fwd = remote addr.
      // The params here (pck_inf, hop, channel, payload) belong to the blind's
      // response packet format — not to the command format we need to send.
      // Store them as a fallback (params_from_command=false); they will be
      // upgraded automatically once we see a matching 6a/69 command packet.
      this->track_discovered_blind(src, fwd, chl, typ, typ2, hop,
                                   payload1, payload2, rssi, payload[6], false);
    } else if ((typ == 0x6a) || (typ == 0x69)) {
      // Command FROM remote TO blind(s): src = remote addr.
      // The channel, pck_inf, hop and payload bytes here are exactly what must
      // be replicated when we send commands — iterate every destination and
      // register it as a discovered blind with the correct command params.
      for (uint8_t i = 0; i < num_dests; i++) {
        uint32_t dest_addr;
        if (typ > 0x60) {  // 3-byte addressing
          dest_addr = ((uint32_t)this->msg_rx_[17 + i * 3] << 16) |
                      ((uint32_t)this->msg_rx_[18 + i * 3] << 8) |
                      this->msg_rx_[19 + i * 3];
        } else {            // 1-byte addressing
          dest_addr = this->msg_rx_[17 + i];
        }
        this->track_discovered_blind(dest_addr, src, chl, typ, typ2, hop,
                                     payload1, payload2, rssi, 0, true);
      }
    }
  }

  if((typ == 0xca) || (typ == 0xc9)) { // Status message from a blind
    // Update text sensor
#ifdef USE_TEXT_SENSOR
    {
      auto text_it = this->address_to_text_sensor_.find(src);
      if (text_it != this->address_to_text_sensor_.end()) {
        text_it->second->publish_state(elero_state_to_string(payload[6]));
      }
    }
#endif

    // Check if we know the blind (configured ESPHome cover)
    auto search = this->address_to_cover_mapping_.find(src);
    if(search != this->address_to_cover_mapping_.end()) {
      search->second->notify_rx_meta(millis(), rssi);
      search->second->set_rx_state(payload[6]);
    }

    // Check if we know the address as a configured ESPHome light
    auto light_search = this->address_to_light_mapping_.find(src);
    if(light_search != this->address_to_light_mapping_.end()) {
      light_search->second->notify_rx_meta(millis(), rssi);
      light_search->second->set_rx_state(payload[6]);
    }

    // Update runtime adopted blinds
    auto it = this->runtime_blinds_.find(src);
    if (it != this->runtime_blinds_.end()) {
      it->second.last_seen_ms = millis();
      it->second.last_rssi = rssi;
      it->second.last_state = payload[6];
      this->update_runtime_blind_direction_(it->second, payload[6]);
    }
  } else {
    // Non-status packets: still update RSSI/last_seen for any known blind
    auto search = this->address_to_cover_mapping_.find(src);
    if(search != this->address_to_cover_mapping_.end()) {
      search->second->notify_rx_meta(millis(), rssi);
    }
    auto light_search = this->address_to_light_mapping_.find(src);
    if(light_search != this->address_to_light_mapping_.end()) {
      light_search->second->notify_rx_meta(millis(), rssi);
    }
    auto rb_it = this->runtime_blinds_.find(src);
    if (rb_it != this->runtime_blinds_.end()) {
      rb_it->second.last_seen_ms = millis();
      rb_it->second.last_rssi = rssi;
    }

    // Remote command packets (0x6a/0x69): src = remote addr, dst = blind addr(s).
    // Trigger an immediate status poll on each configured blind/light that is
    // targeted, so HA state updates within ~50 ms instead of waiting for the
    // normal poll interval.
    if ((typ == 0x6a) || (typ == 0x69)) {
      for (uint8_t i = 0; i < num_dests; i++) {
        uint32_t dest_addr;
        if (typ > 0x60) {  // 3-byte addressing
          dest_addr = ((uint32_t)this->msg_rx_[17 + i * 3] << 16) |
                      ((uint32_t)this->msg_rx_[18 + i * 3] << 8) |
                      this->msg_rx_[19 + i * 3];
        } else {            // 1-byte addressing
          dest_addr = this->msg_rx_[17 + i];
        }
        auto c_it = this->address_to_cover_mapping_.find(dest_addr);
        if (c_it != this->address_to_cover_mapping_.end()) {
          c_it->second->schedule_immediate_poll();
        }
        auto l_it = this->address_to_light_mapping_.find(dest_addr);
        if (l_it != this->address_to_light_mapping_.end()) {
          l_it->second->schedule_immediate_poll();
        }
      }
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
  cover->set_poll_offset((this->address_to_cover_mapping_.size() - 1) * ELERO_POLL_STAGGER_MS);
}

void Elero::register_light(EleroLightBase *light) {
  uint32_t address = light->get_blind_address();
  if(this->address_to_light_mapping_.find(address) != this->address_to_light_mapping_.end()) {
    ESP_LOGE(TAG, "A light with this address is already registered - this is currently not supported");
    return;
  }
  this->address_to_light_mapping_.insert({address, light});
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
  packet_dump_mode_ = true;
  ESP_LOGI(TAG, "Packet dump mode started");
}

void Elero::stop_packet_dump() {
  packet_dump_mode_ = false;
  ESP_LOGI(TAG, "Packet dump mode stopped");
}

void Elero::clear_raw_packets() {
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

  if (raw_packets_.size() < ELERO_MAX_RAW_PACKETS) {
    raw_packets_.push_back(pkt);
    raw_packet_write_idx_ = (uint16_t)(raw_packets_.size() - 1);
  } else {
    raw_packet_write_idx_ = (raw_packet_write_idx_ + 1) % ELERO_MAX_RAW_PACKETS;
    raw_packets_[raw_packet_write_idx_] = pkt;
  }
}

void Elero::mark_last_raw_packet_(bool valid, const char *reason) {
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
        ESP_LOGI(TAG, "Upgraded blind 0x%06x params from command packet: ch=%d, pck_inf=0x%02x/0x%02x, hop=0x%02x",
                 src, channel, pck_inf0, pck_inf1, hop);
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

bool Elero::send_command(t_elero_command *cmd) {
  // Reject if SPI is permanently broken or TX is already in progress.
  if (this->spi_failed_)
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
           cmd->blind_addr, cmd->payload[0], cmd->channel, cmd->counter);
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
      if (!this->spi_failed_) {
        ESP_LOGE(TAG, "send_command: %d consecutive reinit failures — SPI appears permanently broken.",
                 this->send_cmd_reinit_failures_);
        ESP_LOGE(TAG, "  If GPIO12 is used for SPI MISO, it may be pulling VDD_SDIO to 1.8V at boot.");
        ESP_LOGE(TAG, "  Use non-strapping pins for SPI (e.g. CLK=18, MISO=19, MOSI=23).");
        this->spi_failed_ = true;
        this->mark_failed();
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
  // SIDLE interrupted.
  this->write_cmd(CC1101_SFTX);
  this->write_cmd(CC1101_SFRX);
  this->rx_ready_.store(false, std::memory_order_release);

  // Load TX FIFO and start transmission
  delay_microseconds_safe(100);
  this->write_burst(CC1101_TXFIFO, this->msg_tx_, this->msg_tx_[0] + 1);
  this->write_cmd(CC1101_STX);

  this->tx_state_.store(TxState::TRANSMITTING, std::memory_order_release);
  this->tx_state_entered_ms_ = millis();
  return true;
}

// ─── Runtime blind adoption ───────────────────────────────────────────────

bool Elero::adopt_blind(const DiscoveredBlind &discovered, const std::string &name,
                        DeviceType type) {
  if (this->is_cover_configured(discovered.blind_address))
    return false;
  if (this->is_light_configured(discovered.blind_address))
    return false;
  if (this->is_blind_adopted(discovered.blind_address))
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
  ESP_LOGI(TAG, "Adopted runtime %s 0x%06x as \"%s\"",
           type == DeviceType::LIGHT ? "light" : "blind",
           discovered.blind_address, rb.name.c_str());
  return true;
}

bool Elero::remove_runtime_blind(uint32_t addr) {
  auto it = this->runtime_blinds_.find(addr);
  if (it != this->runtime_blinds_.end()) {
    ESP_LOGI(TAG, "Removed runtime blind 0x%06x", addr);
    this->runtime_blinds_.erase(it);
    return true;
  }
  return false;
}

bool Elero::send_runtime_command(uint32_t addr, uint8_t cmd_byte) {
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
