# Implementation Plan: Dual-Core TX/RX Split for esphome-elero

## Context

The ESP32 has two Xtensa LX6 cores. Currently, all Elero radio operations (TX state machine, RX packet processing, SPI communication) run on **Core 1** within ESPHome's single-threaded cooperative loop. This means RX packet processing is delayed by other components (WiFi, web server, sensors), and TX commands compete with everything else in the loop.

**Goal**: Move all CC1101 radio operations to a dedicated FreeRTOS task on **Core 0**, communicating with Core 1 via FreeRTOS queues. This eliminates loop-latency bottlenecks for TX/RX while keeping all ESPHome entity logic safely on Core 1.

---

## Phase 1: New Types & Queue Messages

Add to `elero.h`:

```cpp
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

/// Result of a decoded RX packet, sent from Core 0 → Core 1
struct RxResult {
  uint32_t blind_address;
  uint32_t remote_address;
  uint8_t  channel;
  uint8_t  pck_inf[2];
  uint8_t  hop;
  uint8_t  state;           // ELERO_STATE_*
  float    rssi;
  uint32_t timestamp_ms;
  uint8_t  payload[10];
  bool     is_command;       // true = 0x6a/0x69 command packet
  // Discovery data (only populated in scan mode)
  bool     scan_hit;
  uint8_t  payload_1;
  uint8_t  payload_2;
  bool     params_from_command;
};

/// TX command request, sent from Core 1 → Core 0
struct TxRequest {
  t_elero_command cmd;
  uint8_t  repeat_count;     // how many RF repetitions
  uint32_t repeat_delay_ms;  // delay between repetitions
};

/// Special control messages for the radio task (multiplexed on TX queue)
enum class RadioControlType : uint8_t {
  TX_COMMAND,       // Normal TX command
  START_SCAN,       // Enter scan mode
  STOP_SCAN,        // Exit scan mode
  REINIT_FREQ,      // Change frequency
  START_DUMP,       // Start packet dump
  STOP_DUMP,        // Stop packet dump
  SHUTDOWN,         // Graceful task shutdown
};

struct RadioMessage {
  RadioControlType type;
  union {
    TxRequest tx;
    struct { uint8_t freq2, freq1, freq0; } freq;
  };
};
```

---

## Phase 2: Radio Task Design

### Task Parameters
- **Core**: 0 (pinned via `xTaskCreatePinnedToCore`)
- **Priority**: 19 (below WiFi at 23, above most ESP-IDF tasks)
- **Stack**: 8192 bytes (crypto operations + SPI buffers need headroom)
- **Loop interval**: 1ms (`vTaskDelay(1 / portTICK_PERIOD_MS)` or notification-based wake)

### Task Function (`radio_task_`)

```
static void radio_task_func(void *param) {
  Elero *hub = static_cast<Elero *>(param);
  while (true) {
    // 1. Check for TX commands from Core 1 (non-blocking queue read)
    RadioMessage msg;
    if (xQueueReceive(hub->tx_queue_, &msg, 0) == pdTRUE) {
      handle_radio_message(msg);  // send_command, reinit, etc.
    }

    // 2. Process RX if ISR flag set and TX idle
    if (hub->rx_ready_.load(acquire) && hub->tx_state_ == IDLE) {
      process_rx_on_radio_core();  // read FIFO, decode, push RxResult to rx_queue_
    }

    // 3. Advance TX state machine
    if (hub->tx_state_ != IDLE) {
      advance_tx();
    }

    // 4. Radio health watchdog (every 5s)
    check_radio_state_();

    // 5. Yield to WiFi/system tasks
    vTaskDelay(1);
  }
}
```

### Key Principle: ALL SPI Access on Core 0 Only

After `setup()` completes and spawns the radio task, **no SPI calls from Core 1**. This eliminates the SPI thread-safety issue entirely — no mutex needed for the SPI bus itself.

The following methods move to radio-task-only execution:
- `write_reg()`, `write_burst()`, `write_cmd()`, `read_reg()`, `read_status()`, `read_buf()`
- `send_command()` (the actual SPI portion)
- `process_rx()`, `advance_tx()`, `check_radio_state_()`
- `flush_and_rx()`, `flush_rx()`
- `interpret_msg()`, `msg_decode()`, `msg_encode()`
- `reset()`, `init()`, `reinit_frequency()`

---

## Phase 3: Queue Design

### TX Queue (Core 1 → Core 0)
- **Type**: `QueueHandle_t tx_queue_`
- **Item**: `RadioMessage` (~50 bytes)
- **Depth**: 16 items (covers burst scenarios: multi-blind commands, scan start/stop)
- **Producer**: `Elero::send_command()` on Core 1 (becomes queue-push instead of direct SPI)
- **Consumer**: Radio task on Core 0
- **Overflow**: `xQueueSend` with `pdMS_TO_TICKS(10)` timeout; if full, return false (same as current TX-busy behavior)

### RX Queue (Core 0 → Core 1)
- **Type**: `QueueHandle_t rx_queue_`
- **Item**: `RxResult` (~40 bytes)
- **Depth**: 16 items (handles burst of 4+ simultaneous blind responses)
- **Producer**: Radio task after `interpret_msg()` decodes a valid packet
- **Consumer**: `Elero::loop()` on Core 1 — drains queue, dispatches to covers/lights/sensors
- **Overflow**: Drop oldest (overwrite) — stale RX data is less critical than fresh data

### TX-Idle Notification
- **Type**: `std::atomic<bool> tx_idle_` (already exists as `tx_state_`)
- Radio task stores after TX completes; Core 1 reads before enqueuing
- No change needed — atomics with acquire/release already correct

---

## Phase 4: Thread-Safety Strategy

### Critical Findings from Verification

Two independent review agents identified **4 critical issues** that must be addressed:

1. **AsyncWebServer handlers run on FreeRTOS worker tasks, NOT Core 1** — `adopt_blind()`, `remove_runtime_blind()`, `update_runtime_blind_settings()` called from web handlers can race with `Elero::loop()` iterating over the same maps. **This is a pre-existing bug** even without multi-core, but multi-core makes it worse.
2. **`reinit_frequency()` does SPI from web handler context** — Must go through RadioMessage queue, but the web handler needs a synchronous success/failure response.
3. **Destructor must stop the radio task before freeing RadioLib** — Otherwise use-after-free when task accesses deleted pointers.
4. **ESPHome's SPIDevice bus locking is not core-aware** — All SPI must be on Core 0 exclusively; no SPI from any other context after task spawn.

### Fix 1: Mutex for `runtime_blinds_` and `discovered_blinds_`

Add `std::mutex state_mutex_` protecting all access to `runtime_blinds_` and `discovered_blinds_`:
- `Elero::loop()` → `drain_runtime_queues()`, `poll_runtime_blinds_()`, `recompute_runtime_positions_()`: lock before iterating
- Web handlers → `adopt_blind()`, `remove_runtime_blind()`, `update_runtime_blind_settings()`: lock before modifying
- Web handlers → `get_runtime_blinds()`, `get_discovered_blinds()`: return **copy** (not const reference) under lock
- `dispatch_rx_result_()` → runtime blind state update: lock before modifying

### Fix 2: Synchronous `reinit_frequency()` via Semaphore

```cpp
// In RadioMessage:
struct RadioMessage {
  RadioControlType type;
  union { TxRequest tx; struct { uint8_t freq2, freq1, freq0; } freq; };
  SemaphoreHandle_t completion_sem{nullptr};  // Optional: signaled when done
  bool *result_ptr{nullptr};                   // Optional: success/failure output
};

// Web handler for /api/frequency/set:
SemaphoreHandle_t sem = xSemaphoreCreateBinary();
bool result = false;
RadioMessage msg{.type = REINIT_FREQ, .freq = {f2, f1, f0},
                 .completion_sem = sem, .result_ptr = &result};
xQueueSend(tx_queue_, &msg, pdMS_TO_TICKS(100));
xSemaphoreTake(sem, pdMS_TO_TICKS(5000));  // wait up to 5s
vSemaphoreDelete(sem);
// Now `result` holds success/failure
```

### Fix 3: Radio Task Shutdown in Destructor

```cpp
Elero::~Elero() {
  // 1. Signal radio task to stop
  if (this->radio_task_handle_ != nullptr) {
    this->task_shutdown_.store(true, std::memory_order_release);
    // Send SHUTDOWN message to unblock task if waiting on queue
    RadioMessage msg{.type = RadioControlType::SHUTDOWN};
    xQueueSend(this->tx_queue_, &msg, pdMS_TO_TICKS(100));
    // Wait for task to exit (up to 1s)
    for (int i = 0; i < 100 && this->radio_task_handle_ != nullptr; i++) {
      delay(10);
    }
    if (this->radio_task_handle_ != nullptr) {
      vTaskDelete(this->radio_task_handle_);  // force-kill as last resort
    }
  }
  // 2. Now safe to detach interrupt and delete RadioLib
  if (this->gdo0_pin_ != nullptr) this->gdo0_pin_->detach_interrupt();
  delete this->radio_;
  delete this->radio_module_;
  // 3. Clean up FreeRTOS resources
  if (this->tx_queue_) vQueueDelete(this->tx_queue_);
  if (this->rx_queue_) vQueueDelete(this->rx_queue_);
}
```

Radio task checks shutdown flag each iteration:
```cpp
if (hub->task_shutdown_.load(std::memory_order_acquire)) {
  hub->radio_task_handle_ = nullptr;  // signal destructor we're done
  vTaskDelete(nullptr);  // delete self
}
```

### Fix 4: No mark_failed() from Core 0

The radio task must **never** call `Component::mark_failed()` directly. Instead, set an atomic flag that Core 1's `loop()` checks:
```cpp
std::atomic<bool> radio_fatal_error_{false};
// Radio task: this->radio_fatal_error_.store(true, release);
// Core 1 loop: if (radio_fatal_error_.load(acquire)) { this->mark_failed(); return; }
```

### Complete Resource Table

| Resource | Current Location | After Split | Synchronization |
|---|---|---|---|
| `rx_ready_` (atomic) | ISR → Core 1 loop | ISR → Core 0 task | No change (atomic) |
| `tx_state_` (atomic) | Core 1 loop | Core 0 task writes, Core 1 reads | No change (atomic) |
| `msg_rx_[]` / `msg_tx_[]` | Core 1 loop | Core 0 task exclusively | None needed (single-owner) |
| `radio_`, `radio_module_`, `radio_hal_` | Core 1 loop | Core 0 task exclusively | None needed (single-owner) |
| All SPI methods | Core 1 loop | Core 0 task exclusively | None needed (single-owner) |
| `discovered_blinds_` | Core 1 loop + web handlers | Core 1 loop + web handlers | **New `state_mutex_`** |
| `runtime_blinds_` | Core 1 loop + web handlers | Core 1 loop + web handlers | **New `state_mutex_`** |
| `address_to_cover_mapping_` | Setup + Core 1 loop | Core 1 loop only (read-only after setup) | None needed |
| `address_to_light_mapping_` | Setup + Core 1 loop | Core 1 loop only (read-only after setup) | None needed |
| RSSI/text sensor publish | Core 1 loop | Core 1 loop only (via RX queue) | None needed |
| `log_entries_` | Any core (logger callback) | No change | `log_mutex_` (already exists) |
| `raw_packets_` (packet dump) | Core 1 loop | Core 0 captures raw, Core 1 reads | New `packet_dump_mutex_` |
| `scan_mode_` | Core 1 loop | Core 0 reads, Core 1 writes via RadioMessage | `std::atomic<bool>` |
| `packet_dump_mode_` | Core 1 loop | Core 0 reads, Core 1 writes via RadioMessage | `std::atomic<bool>` |
| Diagnostic counters (`rx_count_`, etc.) | Core 1 loop | Core 0 increments, Core 1 reads | `std::atomic<uint32_t>` |
| `spi_failed_` | Core 1 loop | Core 0 sets, Core 1 reads | `std::atomic<bool>` |
| `radio_fatal_error_` | N/A (new) | Core 0 sets, Core 1 reads | `std::atomic<bool>` |
| `task_shutdown_` | N/A (new) | Core 1 sets, Core 0 reads | `std::atomic<bool>` |

---

## Phase 5: Modified `send_command()` Flow

**Before** (direct SPI on Core 1):
```
EleroCover::loop() → dispatch_commands() → parent_->send_command(&cmd)
  → standby() [1-2ms block] → flush FIFO → load FIFO → STX strobe
```

**After** (queue-based):
```
EleroCover::loop() → dispatch_commands() → parent_->send_command(&cmd)
  → Build RadioMessage{TX_COMMAND, cmd, repeats, delay}
  → xQueueSend(tx_queue_, &msg, timeout)
  → return true if queued, false if queue full
```

The `is_tx_idle()` check remains — covers/lights still check before enqueuing. The radio task processes commands FIFO.

---

## Phase 6: Modified `Elero::loop()` (Core 1)

```cpp
void Elero::loop() {
  if (this->spi_failed_.load(std::memory_order_acquire))
    return;

  // 1. Drain RX results from radio task
  RxResult rx;
  uint8_t rx_count = 0;
  while (rx_count < ELERO_MAX_RX_PER_LOOP &&
         xQueueReceive(this->rx_queue_, &rx, 0) == pdTRUE) {
    this->dispatch_rx_result_(rx);  // route to covers/lights/sensors
    rx_count++;
  }

  // 2. Drain runtime blind command queues (enqueues via TX queue)
  if (this->is_tx_idle()) {
    this->drain_runtime_queues();
    this->poll_runtime_blinds_();
  }

  // 3. Recompute positions (unchanged)
  this->recompute_runtime_positions_();
}
```

New `dispatch_rx_result_()` method handles:
- Lookup in `address_to_cover_mapping_` → call `set_rx_state()`, `notify_rx_meta()`
- Lookup in `address_to_light_mapping_` → call `set_rx_state()`, `notify_rx_meta()`
- RSSI sensor publish
- Text sensor publish
- Discovery tracking (if `rx.scan_hit`)
- Runtime blind state updates

---

## Phase 7: Setup Sequence

```cpp
void Elero::setup() {
  // 1. Initialize SPI, RadioLib, CC1101 (same as current — runs on Core 1)
  this->radio_hal_.set_spi_parent(this);
  this->radio_module_ = new Module(...);
  this->radio_ = new CC1101(this->radio_module_);
  radio_->begin(...);
  // ... register writes, GDO0 interrupt attach ...

  // 2. Create FreeRTOS queues
  this->tx_queue_ = xQueueCreate(16, sizeof(RadioMessage));
  this->rx_queue_ = xQueueCreate(16, sizeof(RxResult));

  // 3. Spawn radio task on Core 0
  xTaskCreatePinnedToCore(
    radio_task_func,     // function
    "elero_radio",       // name
    8192,                // stack
    this,                // parameter
    19,                  // priority (below WiFi=23)
    &this->radio_task_handle_,  // handle
    0                    // Core 0
  );

  // 4. After this point, NO SPI calls from Core 1
}
```

---

## Phase 8: Impact on EleroCover / EleroLight

**Minimal changes required:**

- `dispatch_commands()` already calls `parent_->send_command()` which will become a queue push — **no interface change**
- `is_tx_idle()` already used for gating — **no change** (still reads atomic)
- `set_rx_state()` still called from Core 1 (via `dispatch_rx_result_()`) — **no change**
- `notify_rx_meta()` still called from Core 1 — **no change**

The only difference is `send_command()` returns immediately after queueing (no 1-2ms block), which is strictly better.

---

## Phase 9: Impact on EleroWebServer

**IMPORTANT**: AsyncWebServer handlers run on FreeRTOS worker tasks, **NOT on Core 1's main loop**. This means all web handlers that read/modify shared state need synchronization. This is a pre-existing latent bug that must be fixed as part of this work.

**Changes needed:**

| Endpoint | Impact | Fix |
|---|---|---|
| `/api/scan/start` | Currently calls `hub->start_scan()` directly | Send `RadioMessage{START_SCAN}` via TX queue |
| `/api/scan/stop` | Currently calls `hub->stop_scan()` directly | Send `RadioMessage{STOP_SCAN}` via TX queue |
| `/api/frequency/set` | Currently calls `reinit_frequency()` (SPI!) | Send `RadioMessage{REINIT_FREQ}` via TX queue + semaphore for sync response |
| `/api/frequency/set_mhz` | Same as above | Same fix |
| `/api/dump/start` | Currently sets `packet_dump_mode_` | Atomic flag or RadioMessage |
| `/api/discovered` | Reads `discovered_blinds_` | Lock `state_mutex_`, return copy |
| `/api/configured` | Reads cover/light maps | Safe — read-only after setup |
| `/api/runtime` | Reads `runtime_blinds_` | Lock `state_mutex_`, return copy |
| `/api/runtime/.../settings` | Calls `update_runtime_blind_settings()` | Lock `state_mutex_` |
| `/api/discovered/.../adopt` | Calls `adopt_blind()` | Lock `state_mutex_` |
| `/api/runtime/.../remove` | Calls `remove_runtime_blind()` | Lock `state_mutex_` |
| `/api/packets` | Reads `raw_packets_` | Lock `packet_dump_mutex_` |
| `/api/diagnostics/reset` | Resets counters | Atomic stores |
| `/api/covers/.../command` | Calls `send_command()` | Now queue-based, safe |
| `/api/lights/.../command` | Calls `send_command()` on light | Now queue-based, safe |

---

## Phase 10: Migration Strategy (3 Incremental PRs)

### PR 1: Prepare for Multi-Core (no behavioral change)
- Convert `scan_mode_`, `packet_dump_mode_`, `spi_failed_` to `std::atomic`
- Convert diagnostic counters to `std::atomic<uint32_t>`
- Add `state_mutex_` for `runtime_blinds_` and `discovered_blinds_` access (fixes pre-existing web handler race)
- Add `packet_dump_mutex_` for `raw_packets_` access
- Change `get_runtime_blinds()` and `get_discovered_blinds()` to return copies (not const refs)
- Extract `dispatch_rx_result_()` method from current `interpret_msg()` dispatch code
- Extract radio-only methods into a clearly marked section
- Add `RxResult`, `RadioMessage`, `TxRequest` struct definitions
- Add `task_shutdown_`, `radio_fatal_error_` atomic flags
- **Test**: Everything works exactly as before (single-core)

### PR 2: Add Radio Task with TX Queue
- Create radio task function with shutdown flag check
- Convert `send_command()` to queue producer
- Radio task consumes TX queue and executes SPI commands
- `advance_tx()` moves to radio task
- `check_radio_state_()` moves to radio task
- Keep RX processing on Core 1 for now (ISR → Core 1 loop)
- Add task health monitoring (watchdog)
- Update destructor: stop radio task before freeing RadioLib
- Replace `mark_failed()` calls in radio-path code with `radio_fatal_error_` atomic flag
- **Test**: TX commands work via queue, RX still works via loop

### PR 3: Move RX to Radio Task + Web Server Sync
- `process_rx()` moves to radio task
- `interpret_msg()` runs on Core 0, pushes `RxResult` to RX queue
- `Elero::loop()` drains RX queue via `dispatch_rx_result_()`
- Update web server endpoints (scan/dump/frequency) to use RadioMessage
- Add semaphore-based sync for `reinit_frequency()` (web handler waits for result)
- Lock `state_mutex_` in all web handlers that access `runtime_blinds_` / `discovered_blinds_`
- Remove all SPI calls from Core 1 code paths
- **Test**: Full dual-core operation, verify all entity updates work

---

## Verification Plan

1. **Compile test**: `esphome compile` with the changes
2. **Boot test**: Verify CC1101 init succeeds, radio task starts (log messages)
3. **RX test**: Operate blinds with original remote, verify status updates arrive
4. **TX test**: Send open/close/stop from Home Assistant, verify blinds respond
5. **Scan test**: Start/stop discovery scan, verify blinds found
6. **Position tracking**: Verify dead-reckoning still accurate after dual-core
7. **Web UI**: Test all REST endpoints, especially scan/frequency/dump
8. **Stress test**: Send rapid commands to multiple blinds simultaneously
9. **WiFi stability**: Verify no WiFi disconnects under radio load
10. **Long-run soak**: Leave running 24h, check for memory leaks or hangs

---

## Key Files to Modify

| File | Changes |
|---|---|
| `components/elero/elero.h` | Add FreeRTOS includes, new structs, queue handles, task handle, `dispatch_rx_result_()`, atomic upgrades |
| `components/elero/elero.cpp` | Radio task function, modified `setup()`, modified `loop()`, modified `send_command()`, queue-based dispatch |
| `components/elero/__init__.py` | No changes needed (task creation is in C++) |
| `components/elero/cover/EleroCover.cpp` | No changes (interface unchanged) |
| `components/elero/light/EleroLight.cpp` | No changes (interface unchanged) |
| `components/elero_web/elero_web_server.cpp` | Scan/dump/frequency via RadioMessage, packet dump mutex |

---

## Risks & Mitigations

| Risk | Likelihood | Mitigation |
|---|---|---|
| WiFi starvation from radio task | Medium | Priority 19 (below WiFi 23), 1ms yield per loop |
| Queue overflow drops commands | Low | 16-deep queues, `is_tx_idle()` gating, timeout returns false |
| Radio task crash | Low | Task watchdog via `esp_task_wdt`, Core 1 detects via `radio_fatal_error_` flag and restarts |
| SPI called from wrong core (bug) | Medium | Code review, `assert()` in debug builds, clear code separation |
| Stack overflow in radio task | Low | 8192 bytes (actual usage <1KB); monitor via `uxTaskGetStackHighWaterMark()` |
| Deadlock between queues | Very Low | No bidirectional blocking waits; TX queue uses timeout, RX queue is non-blocking |
| Subtle timing changes break position tracking | Medium | Position tracking stays on Core 1, only data source changes (queue vs direct) |
| Destructor use-after-free | Low | Shutdown flag + SHUTDOWN message + wait loop before freeing RadioLib (Fix 3) |
| Web handler race on `runtime_blinds_` | Medium | `state_mutex_` protects all access (Fix 1); pre-existing bug fixed |
| `reinit_frequency()` async response | Low | Semaphore-based sync with 5s timeout (Fix 2) |
| `mark_failed()` from wrong core | Low | Deferred via `radio_fatal_error_` atomic, checked in Core 1 `loop()` (Fix 4) |

---

## Dual-Agent Verification Summary

Two independent verification agents reviewed this plan against the actual codebase:

**Agent 1 (FreeRTOS/ESP32)**: 7 PASS, 1 FAIL
- All FreeRTOS primitives available, priority 19 safe, atomics correct on Xtensa, stack/queue sizing adequate
- FAIL: SPI race with `reinit_frequency()` from web handler → **Fixed** (RadioMessage + semaphore)

**Agent 2 (ESPHome Compatibility)**: 4 PASS, 2 FAIL, 4 CONCERN
- `dispatch_commands()` compatible, `dump_config()` safe, sensor publishes all on Core 1
- FAIL: Destructor use-after-free → **Fixed** (task shutdown protocol)
- FAIL: `reinit_frequency()` no sync response → **Fixed** (semaphore)
- CONCERN: Web handlers on worker tasks → **Fixed** (`state_mutex_`)
- CONCERN: SPIDevice not core-aware → **Fixed** (exclusive Core 0 SPI ownership)

**All 4 critical issues have been addressed in the plan.**
