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
static const uint8_t SPI_SETTLE_US = 5;  // inter-transaction settling (CC1101 needs ~1-2µs)

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
                       void (*increase_counter_fn)(void *ctx), void *ctx,
                       bool stop_urgent_self) {
  // Skip immediately if hub SPI is permanently broken — no point retrying.
  if (parent->is_failed()) return;

  // When THIS cover is in stop-verification, defer its own non-stop commands
  // so the stop packets transmit without self-contention.  Other covers are
  // no longer blocked — only the cover that triggered auto-stop defers.
  if (stop_urgent_self && !queue.empty()) {
    uint8_t front_cmd = queue.front();
    if (front_cmd != ELERO_COMMAND_COVER_STOP) {
      return;  // defer until this cover's stop verification completes
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

  // 1. Drain runtime blind command queues (enqueues to TX queue).
  //    TX-first: prioritise outgoing commands over incoming results so group
  //    commands fill the TX pipeline as early as possible.
  //    No is_tx_idle() gate: the FreeRTOS TX queue (depth 16) buffers commands
  //    while the radio is busy.  send_command() returns false if the queue is
  //    full, and callers retry next loop.
  this->drain_runtime_queues();
  this->poll_runtime_blinds_();

  // 2. Drain RX results from radio task (Core 0 → Core 1 via queue).
  if (this->rx_queue_) {
    RxResult rx;
    uint8_t rx_drain_count = 0;
    while (rx_drain_count < ELERO_MAX_RX_PER_LOOP &&
           xQueueReceive(this->rx_queue_, &rx, 0) == pdTRUE) {
      this->dispatch_rx_result_(rx);
      rx_drain_count++;
    }
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

  // Allow the CC1101 to stabilize after power-on.
  delay(150);

  this->spi_setup();

  // Initialize RadioLib HAL adapter
  this->radio_hal_.set_spi_parent(this);
  this->radio_module_ = new Module(&this->radio_hal_, RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC);
  this->radio_ = new CC1101(this->radio_module_);

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
    ESP_LOGW(TAG, "First init failed, retrying reset+init with extended delay...");
    delay(10);
    this->reset();
    if (!this->init()) {
      uint8_t partnum = this->read_status(CC1101_PARTNUM);
      uint8_t version = this->read_status(CC1101_VERSION);
      ESP_LOGE(TAG, "CC1101 SPI diagnostic: PARTNUM=0x%02x (expect 0x00), VERSION=0x%02x (expect 0x14)", partnum, version);

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
  if (logger::global_logger != nullptr) {
    logger::global_logger->add_log_callback(this, elero_log_callback);
    this->log_listener_ = this;
  }
#endif

  // Create FreeRTOS queues for dual-core communication
  this->tx_queue_ = xQueueCreate(ELERO_TX_QUEUE_DEPTH, sizeof(RadioMessage));
  this->tx_priority_queue_ = xQueueCreate(ELERO_TX_PRIORITY_QUEUE_DEPTH, sizeof(RadioMessage));
  this->rx_queue_ = xQueueCreate(32, sizeof(RxResult));
  if (!this->tx_queue_ || !this->tx_priority_queue_ || !this->rx_queue_) {
    ESP_LOGE(TAG, "Failed to create FreeRTOS queues for radio task");
    this->mark_failed(LOG_STR("Failed to allocate radio task queues"));
    return;
  }

  // Spawn radio task on Core 0
  BaseType_t rc_task = xTaskCreatePinnedToCore(
    Elero::radio_task_func_,
    "elero_radio",
    8192,
    this,
    19,
    &this->radio_task_handle_,
    0
  );
  if (rc_task != pdPASS) {
    ESP_LOGE(TAG, "Failed to create radio task on Core 0");
    this->mark_failed(LOG_STR("Failed to create radio task"));
    return;
  }
  ESP_LOGI(TAG, "Radio task spawned on Core 0 (priority 19, stack 8192)");

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

}  // namespace elero
}  // namespace esphome
