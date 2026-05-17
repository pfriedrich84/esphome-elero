# Development Notes — esphome-elero

This document preserves detailed development guidance for this repository. Canonical tool-neutral agent instructions start at [`../../AGENTS.md`](../../AGENTS.md).

---

## Project Overview

`esphome-elero` is a custom **ESPHome external component** that enables Home Assistant to control Elero wireless motor blinds (rollers, shutters, awnings) and lights (dimmers) via a **CC1101 868 MHz (or 433 MHz) RF transceiver** connected to an ESP32 over SPI.

The component is loaded directly from GitHub in an ESPHome YAML configuration:

```yaml
external_components:
  - source: github://pfriedrich84/esphome-elero
```

**Key capabilities:**
- Send open/close/stop/tilt commands to Elero blinds
- On/off and brightness control for Elero wireless lights (dimmers)
- Receive status feedback (top, bottom, moving, blocked, overheated, etc.)
- Track cover position based on movement timing (dead-reckoning)
- RSSI signal strength monitoring per blind
- RF discovery scan to find nearby blinds (web UI and log-based)
- Runtime blind adoption from web UI without reflashing
- RF packet dump and log capture for diagnostics
- Optional web UI served at `http://<device-ip>/elero` for discovery, control, and YAML generation

**Upstream credits:**
- Encryption/decryption: [QuadCorei8085/elero_protocol](https://github.com/QuadCorei8085/elero_protocol) (MIT)
- Remote handling: [stanleypa/eleropy](https://github.com/stanleypa/eleropy) (GPLv3)
- Based on the no-longer-maintained [andyboeh/esphome-elero](https://github.com/pfriedrich84/esphome-elero)

---

## Repository Structure

```
esphome-elero/
├── .github/
│   ├── FUNDING.yml                    # GitHub Sponsors config
│   └── workflows/
│       └── ci.yml                     # CI pipeline (lint + 8-config compile matrix + auto-issue creation)
├── .claude/
│   └── skills/                        # Claude Code slash commands
│       ├── compile.md                 # /compile — ESPHome compile test
│       ├── test.md                    # /test — run C++ + Python tests
│       ├── build-web-ui.md            # /build-web-ui — rebuild frontend
│       ├── review.md                  # /review — quality gate checks
│       ├── elero-protocol.md          # /elero-protocol — RF protocol reference
│       └── validate.md               # /validate — YAML schema validation
├── .clang-format                      # C++ formatting (Google-based, 120 col, 2-space)
├── .clang-tidy                        # C++ static analysis (bugprone-*, performance-*)
├── .gitignore                         # Python cache, .esphome/ exclusions
├── CLAUDE.md                          # Tool-specific shim pointing to AGENTS.md
├── README.md                          # Main documentation (German + English)
├── example.yaml                       # Complete ESPHome config example
├── pyproject.toml                     # Ruff linting + pytest config
├── docs/
│   ├── README.md                      # Documentation index
│   ├── user/
│   │   ├── installation.md            # Step-by-step hardware and software setup
│   │   ├── configuration.md           # Full parameter reference
│   │   └── examples/                  # Additional YAML examples
│   ├── developer/
│   │   ├── architecture.md            # Module seams and architecture notes
│   │   ├── development.md             # This developer guide
│   │   └── adr/                       # Architecture decision records
│   └── agent/                         # Tool-neutral agent operating docs
├── tests/
│   └── configs/                       # ESPHome compile test variants (CI matrix)
│       ├── compile_test.yaml          # Full features: hub + cover + light + web + monitoring sensors
│       ├── minimal.yaml               # Hub + 1 cover only
│       ├── multi_cover.yaml           # 3 covers with different settings
│       ├── light_only.yaml            # 2 lights (on/off + dimmable)
│       ├── no_web.yaml                # No elero_web component
│       ├── no_auto_sensors.yaml       # All auto_sensors disabled, explicit sensors
│       ├── custom_frequency.yaml      # Non-default frequency + send params
│       └── esp32s3-idf.yaml           # ESP32-S3 + esp-idf + shared SPI + display
└── components/
    ├── elero/                         # Main hub component
    │   ├── __init__.py                # ESPHome component schema & code-gen (hub)
    │   ├── elero.h                    # C++ hub class header (~686 lines)
    │   ├── elero.cpp                  # Core lifecycle, HAL, setup, loop, radio task (~558 lines)
    │   ├── elero_cc1101.cpp           # CC1101 register access, TX/RX state machines, watchdog (~589 lines)
    │   ├── elero_protocol.cpp         # Protocol crypto, message dispatch, runtime blinds (~786 lines)
    │   ├── cc1101.h                   # CC1101 register map & command strobes
    │   ├── cover/                     # Cover (blind) platform
    │   │   ├── __init__.py            # Cover schema, auto-sensors, validation
    │   │   ├── EleroCover.h           # Cover class header (~142 lines)
    │   │   └── EleroCover.cpp         # Cover logic, position tracking (~547 lines)
    │   ├── light/                     # Light (dimmer) platform
    │   │   ├── __init__.py            # Light schema, auto-sensors, validation
    │   │   ├── EleroLight.h           # Light class header (~134 lines)
    │   │   └── EleroLight.cpp         # Light logic, brightness tracking (~269 lines)
    │   ├── button/                    # Scan button platform
    │   │   ├── __init__.py            # Button schema (scan + light command)
    │   │   ├── elero_button.h         # Button class header
    │   │   └── elero_button.cpp       # Button press handler (46 lines)
    │   ├── sensor/                    # RSSI sensor platform
    │   │   └── __init__.py            # Registers sensor with hub by blind address
    │   └── text_sensor/               # Blind status text sensor platform
    │       └── __init__.py            # Registers text sensor with hub by blind address
    └── elero_web/                     # Optional web UI component
        ├── __init__.py                # Web server schema & code-gen
        ├── elero_web_server.h         # Web server class header
        ├── elero_web_server.cpp       # REST API + CORS (~1356 lines)
        ├── elero_web_ui.h             # AUTO-GENERATED: embedded HTML/JS/CSS
        ├── switch/                    # Web UI enable/disable switch sub-platform
        │   ├── __init__.py            # Switch schema (depends on elero_web)
        │   ├── elero_web_switch.h     # Switch class header
        │   └── elero_web_switch.cpp   # Switch logic (31 lines)
        ├── frontend/                  # Active Web UI source (Svelte 5 + Vite + Flowbite/Tailwind)
        │   ├── package.json           # npm project
        │   ├── package-lock.json      # Dependency lockfile
        │   ├── svelte.config.js       # Svelte configuration
        │   ├── vite.config.js         # Vite bundler config (single-file output)
        │   ├── index.html             # HTML template
        │   ├── scripts/
        │   │   └── generate_header.mjs  # Post-build: HTML → elero_web_ui.h
        │   └── src/
        │       ├── App.svelte         # Main Svelte application
        │       ├── app.css            # App-level styles
        │       ├── main.js            # Frontend entry point
        │       ├── style.css          # Global styles
        │       └── lib/               # API, stores, and utility helpers
        └── frontend-legacy/           # Retained Alpine.js frontend reference/rollback source
```

---

## Architecture

### Two-layer design

1. **Python layer** (`__init__.py` files) — ESPHome code-generation time
   - Defines and validates YAML configuration schemas using `esphome.config_validation`
   - Generates C++ constructor calls via `esphome.codegen`
   - Declares ESPHome component dependencies (`DEPENDENCIES`, `AUTO_LOAD`)

2. **C++ layer** (`.h`/`.cpp` files) — compiled firmware running on ESP32
   - Implements the actual RF protocol, SPI communication, and entity logic
   - Runs inside the ESPHome `Component` lifecycle (`setup()`, `loop()`)

### Component hierarchy

```
Elero (hub, SPIDevice + Component)
├── EspHomeRadioLibHal (RadioLib HAL adapter, bridges SPIDevice → RadioLib)
│   └── CC1101 (RadioLib, standby/SPI register access)
├── EleroBlindBase (abstract interface for covers)
│   └── EleroCover (cover::Cover + Component + EleroBlindBase)
├── EleroLightBase (abstract interface for lights)
│   └── EleroLight (light::LightOutput + Component + EleroLightBase)
├── EleroScanButton (button::Button + Component)
├── EleroRefreshButton (button::Button + Component, auto-created per cover/light)
├── sensor::Sensor (RSSI, registered per blind address)
├── text_sensor::TextSensor (status, registered per blind address)
├── EleroWebServer (Component + AsyncWebHandler, wraps web_server_base)
│   └── EleroWebSwitch (switch::Switch + Component)
├── RuntimeBlind (adopted from web UI, stored in std::map, supports DeviceType)
└── Auto-registered sensors/text sensors per cover and light (optional via auto_sensors)
```

The abstract base classes `EleroBlindBase` and `EleroLightBase` decouple the hub (`Elero`) from the cover/light implementations so `elero.h` never needs to `#include` the cover or light headers. All communication between hub and entities goes through virtual methods.

### RadioLib integration

The hub uses [RadioLib](https://github.com/jgromes/RadioLib) v7.1.2 (added via PlatformIO `cg.add_library()`) as a hardware abstraction layer for the CC1101 transceiver. A custom `EspHomeRadioLibHal` adapter class bridges ESPHome's `SPIDevice` to RadioLib's HAL interface — GPIO, interrupt, and SPI lifecycle operations are forwarded to ESPHome primitives while SPI transfers delegate to the parent `Elero` component. RadioLib provides:

- `radio_->standby()` — synchronous IDLE transition (~1 ms), replacing the old multi-state async SIDLE approach
- `radio_module_->SPIsetRegValue()` / `SPIgetRegValue()` — register access with verify-readback for init/config
- `radio_->begin()` — initial CC1101 configuration (frequency, bandwidth, data rate, etc.)

The `Elero` class owns the RadioLib instances (`radio_hal_`, `radio_module_`, `radio_`) and cleans them up in its destructor.

### Dual-core architecture

The component uses both ESP32 cores for improved RF responsiveness:

```
Core 0: Radio Task (FreeRTOS, priority 19, 8KB stack)
  ├─ process_rx()           — read CC1101 FIFO, decode, push RxResult to rx_queue_
  ├─ advance_tx()           — poll TX state machine (TRANSMITTING → COOLDOWN → IDLE)
  ├─ send_command_internal_()  — execute SPI TX (standby, flush, load FIFO, STX)
  ├─ check_radio_state_()   — 5s periodic radio health watchdog
  └─ RadioMessage handler   — REINIT_FREQ, START/STOP_SCAN, START/STOP_DUMP, SHUTDOWN

Core 1: ESPHome Main Loop (Elero::loop())
  ├─ Drain rx_queue_ via dispatch_rx_result_()  — route to covers/lights/sensors
  ├─ send_command()         — queue producer (enqueues RadioMessage to tx_queue_)
  ├─ drain_runtime_queues() — runtime blind command scheduling
  ├─ poll_runtime_blinds_() — periodic status checks
  └─ recompute_runtime_positions_()  — dead-reckoning position updates
```

**ALL SPI access is exclusively on Core 0** after `setup()` completes. No SPI mutex is needed because only one core touches the SPI bus.

Communication between cores uses three FreeRTOS queues:
- `tx_queue_` (Core 1 → Core 0): 16-deep `RadioMessage` queue (normal TX commands + control)
- `tx_priority_queue_` (Core 1 → Core 0): 8-deep `RadioMessage` queue (time-critical stop commands, drained first)
- `rx_queue_` (Core 0 → Core 1): 16-deep `RxResult` queue (decoded RX packets)

### Non-blocking TX state machine

The radio uses a simplified 3-state non-blocking TX state machine on Core 0:

```
IDLE → TRANSMITTING → COOLDOWN → IDLE
```

RadioLib's `standby()` handles the IDLE transition synchronously in `send_command_internal_()`. See `TxState` enum in `elero.h`. Commands are buffered in FreeRTOS queues (normal + priority) and consumed by Core 0, so Core 1 callers no longer need to check `is_tx_idle()` before enqueuing.

TX initiation in `send_command_internal_()` (Core 0):
1. `radio_->standby()` — blocks until CC1101 is in IDLE (~1 ms)
2. Flush both TX and RX FIFOs (valid in IDLE per CC1101 spec)
3. Load TX FIFO via burst write
4. Issue `STX` strobe → state transitions to `TRANSMITTING`

TX completion is detected via the `tx_done_` ISR flag (fast path) or by polling MARCSTATE (fallback) — when it leaves TX, the CC1101 has auto-transitioned to RX via MCSM1 TXOFF_MODE.

### Interrupt handling

Two separate `std::atomic<bool>` flags handle GDO0 ISR signals: `rx_ready_` (set when GDO0 fires in RX mode) and `tx_done_` (set when GDO0 fires in TX mode). The ISR reads `radio_mode_` to route the signal to the correct flag, preventing the race where clearing `rx_ready_` before TX preparation loses a concurrent RX interrupt. A `std::atomic<uint8_t> radio_mode_` enum (`RX` or `TX`) tracks the half-duplex radio state — it uses relaxed memory ordering since the ISR may run on a different core than the radio task (ESP-IDF routes GPIO ISRs to the core that installed the service). All other atomic operations use `std::memory_order_acquire` for loads and `std::memory_order_release` for stores to ensure correct multi-core ESP32 synchronization.

### Radio health and FIFO recovery

The CC1101 can enter unrecoverable states (RXFIFO_OVERFLOW, stuck IDLE) during TX operations. Several mechanisms prevent and recover from these:

- **FIFO flush before TX** — `send_command()` uses `standby()` to enter IDLE, then flushes both TX and RX FIFOs. The RX flush discards any partial packet data from the reception that SIDLE interrupted.
- **No SFTX after TX completion** — The CC1101 auto-transitions to RX via MCSM1 TXOFF_MODE after TX. Issuing SFTX in this state is invalid per the CC1101 datasheet (only valid in IDLE or TXFIFO_UNDERFLOW) and can corrupt radio state.
- **Post-TX FIFO health check** — After COOLDOWN, before resuming normal RX, the code reads RXBYTES to detect overflow or pending data that arrived during TX.
- **Escalating radio watchdog** (`check_radio_state_()`, every 5 s) — Reads CC1101 MARCSTATE and applies 3-level recovery within a 60-second window: L1 = flush FIFO (up to 3×), L2 = full chip reset (up to 3×), L3 = mark permanently failed. Stuck IDLE is handled separately with a simple SRX restart. Only runs when TX is idle.
- **TX cooldown** — 1 ms settling time after TX before resuming RX (CC1101 PLL settles in ~75µs).
- **Minimum packet validation** — Packets shorter than `ELERO_MIN_PACKET_SIZE` (17 bytes) are rejected as non-Elero RF noise.

### Data flow

1. `Elero::setup()` (Core 1) configures CC1101 via RadioLib's `begin()` and direct register writes, attaches GDO0 interrupt, creates FreeRTOS queues, then spawns the radio task on Core 0.
2. When the CC1101 signals a received packet (GDO0 interrupt), the ISR routes to `rx_ready_` (RX mode) or `tx_done_` (TX mode) based on `radio_mode_`.
3. The radio task (Core 0) calls `process_rx()` when TX is idle — reads FIFO, decodes, decrypts, builds an `RxResult`, and pushes it to `rx_queue_`.
4. `Elero::loop()` (Core 1) drains `rx_queue_` via `dispatch_rx_result_()` — routes decoded packets to covers/lights/sensors/discovery/runtime blinds.
5. `EleroCover::loop()` / `EleroLight::loop()` (Core 1) handle polling timers and drain command queues by calling `parent_->send_command()` (normal) or `parent_->send_command_priority()` (stop commands), which enqueues a `RadioMessage` to `tx_queue_` or `tx_priority_queue_`.
6. The radio task (Core 0) drains `tx_priority_queue_` first, then `tx_queue_`, and executes `send_command_internal_()` — the actual SPI TX.
7. The radio task advances the TX state machine and runs `check_radio_state_()` for periodic health monitoring.

---

## Key Classes and Files

### `components/elero/elero.h` / `elero.cpp` / `elero_cc1101.cpp` / `elero_protocol.cpp`

**Class:** `Elero : public spi::SPIDevice<...>, public Component`
**Namespace:** `esphome::elero`

The implementation is split across three `.cpp` files (all part of the same `Elero` class):
- **`elero.cpp`** (~558 lines) — core lifecycle: HAL adapter, `setup()`, `loop()`, radio task, FreeRTOS queue management
- **`elero_cc1101.cpp`** (~589 lines) — CC1101 hardware: register access, TX/RX state machines, FIFO handling, radio watchdog
- **`elero_protocol.cpp`** (~786 lines) — Elero protocol: encryption/decryption, message encoding/decoding, RX dispatch, runtime blind management

Critical public API:
- `register_cover(EleroBlindBase*)` — called by each `EleroCover` at setup
- `register_light(EleroLightBase*)` — called by each `EleroLight` at setup
- `send_command(t_elero_command*)` → `SendResult` — encodes, encrypts, and transmits a command via normal TX queue
- `send_command_priority(t_elero_command*)` — priority TX queue for time-critical commands (e.g. stop), bypasses the normal queue
- `is_tx_idle()` — check if TX state machine is ready for a new command
- `get_tx_queue_depth()` — current normal TX queue depth (for dynamic latency compensation)
- `increment_stop_urgent()` / `decrement_stop_urgent()` / `is_stop_urgent()` — atomic counter for multi-cover auto-stop coordination; other covers defer non-stop TX while urgent
- `start_scan()` / `stop_scan()` / `is_scanning()` — toggle RF discovery mode
- `register_rssi_sensor(uint32_t addr, sensor::Sensor*)` — link RSSI sensor to a blind address
- `register_text_sensor(uint32_t addr, text_sensor::TextSensor*)` — link text sensor to a blind address
- `interrupt(Elero *arg)` — static ISR, sets interrupt flags

Cover/light access (for web server):
- `is_cover_configured(addr)` / `get_configured_covers()` — check/list configured covers
- `is_light_configured(addr)` / `get_configured_lights()` — check/list configured lights

Discovery and runtime:
- `get_discovered_blinds()` / `get_discovered_count()` / `clear_discovered()` — manage discovered blinds
- `adopt_blind(DiscoveredBlind&, name, DeviceType)` — adopt discovered blind/light for runtime control
- `remove_runtime_blind(addr)` / `send_runtime_command(addr, cmd)` — manage runtime-adopted blinds
- `update_runtime_blind_settings(addr, open, close, poll)` — update timing at runtime
- `get_runtime_blinds()` / `is_blind_adopted(addr)` — query runtime blinds

Radio health:
- `check_radio_state_()` — periodic watchdog (every 5 s); recovers RXFIFO_OVERFLOW, stuck IDLE, and unexpected MARCSTATE
- FIFO flush in `send_command()` via `standby()` + SFTX/SFRX — prevents stale data from corrupting post-TX RX
- Post-TX FIFO health check in `COOLDOWN→IDLE` transition — detects overflow/pending data missed during TX

Hub-level diagnostic sensors (auto-generated when `auto_sensors: true`, default):
- `set_frequency_sensor()` / `set_rx_count_sensor()` / `set_tx_count_sensor()` / `set_watchdog_recovery_sensor()` — register hub-level diagnostic sensors (Frequency MHz, RX Count, TX Count, Watchdog Recovery Count)

Diagnostics:
- `start_packet_dump()` / `stop_packet_dump()` / `get_raw_packets()` — RF packet capture (ring buffer, max 50)
- `append_log()` / `get_log_entries_copy()` / `set_log_capture()` — persistent log buffer (max 200 entries, mutex-protected)
- `reinit_frequency(freq2, freq1, freq0)` — change CC1101 frequency at runtime
- `get_rx_count()` / `get_tx_count()` / `get_watchdog_recovery_count()` / `get_tx_drop_count()` — diagnostic counters for radio activity
- `increment_tx_drop_count()` — track dropped TX commands
- `reset_diagnostic_counters()` — reset all diagnostic counters to zero (including tx_drop_count)

Key constants (defined in `elero.h` unless noted):

| Constant | Value | Purpose |
|---|---|---|
| `ELERO_MAX_PACKET_SIZE` | 57 | Maximum RF packet length (FCC spec) |
| `ELERO_MIN_PACKET_SIZE` | 17 | Minimum valid Elero packet (shorter = RF noise) — defined in `elero.cpp` |
| `ELERO_POLL_INTERVAL_MOVING` | 5 000 ms | Status poll while blind is moving (blinds broadcast status on their own) |
| `ELERO_TIMEOUT_MOVEMENT` | 120 000 ms | Give up movement tracking after 2 min |
| `ELERO_POST_MOVEMENT_POLL_DELAY` | 5 000 ms | Poll delay after open/close duration elapses |
| `ELERO_SEND_RETRIES` | 3 | Command retry count |
| `ELERO_DEFAULT_SEND_REPEATS` | 1 | RF packets per command (configurable 1–20); 1 means no repeats |
| `ELERO_DEFAULT_SEND_DELAY` | 0 ms | Default delay between repeated packets (configurable) |
| `ELERO_RADIO_TASK_STACK_SIZE` | 16 384 bytes | FreeRTOS stack for the Core 0 radio task |
| `ELERO_MAX_COMMAND_QUEUE` | 10 | Max queued commands per blind (prevents OOM) |
| `ELERO_COMMAND_QUEUE_MAX_AGE_MS` | 30 000 ms | Clear stale command queue after 30 s without successful send |
| `ELERO_TX_QUEUE_DEPTH` | 16 | Normal TX FreeRTOS queue depth |
| `ELERO_TX_PRIORITY_QUEUE_DEPTH` | 8 | Priority TX queue depth (stop commands) |
| `ELERO_MAX_DISCOVERED` | 20 | Max blinds tracked in scan mode |
| `ELERO_MAX_RAW_PACKETS` | 50 | Max raw packets in dump ring buffer |
| `ELERO_MAX_RX_PER_LOOP` | 8 | Max packets drained per `dispatch_rx_result_()` cycle |
| `ELERO_POLL_STAGGER_MS` | 5 000 ms | Stagger offset between cover poll timers |
| `ELERO_IMMEDIATE_POLL_MIN_INTERVAL_MS` | 2 000 ms | Minimum interval between `schedule_immediate_poll()` calls per blind |
| `ELERO_DEDUP_WINDOW_MS` | 500 ms | Default packet deduplication time window (src, cnt pairs), configurable via `dedup_window` |
| `ELERO_STOP_REPEAT_COUNT` | 2 | Stop commands queued on auto-stop (x2 RF packets each) |
| `ELERO_TX_LATENCY_COMPENSATION_MS` | 300 ms | Position check lead time (accounts for multi-cover queue contention) |
| `ELERO_STOP_VERIFY_DELAY_MS` | 2 000 ms | Delay before polling to verify motor stopped (give blind time to broadcast) |
| `ELERO_STOP_VERIFY_MAX_RETRIES` | 1 | Single verify poll — blinds broadcast status, no need to hammer |
| `ELERO_MSG_LENGTH` | 0x1d (29) | Fixed message length for TX |
| `ELERO_CRYPTO_MULT` | 0x708f | Encryption multiplier for counter-based code |
| `TX_STATE_TIMEOUT_MS` | 50 ms | Per-state watchdog timeout for TX state machine — defined in `elero.cpp` |
| `TX_COOLDOWN_MS` | 1 ms | Post-TX settle time before resuming RX — defined in `elero_cc1101.cpp` |
| `RADIO_WATCHDOG_MS` | 5 000 ms | Periodic radio health check interval — defined in `elero_cc1101.cpp` |
| `WATCHDOG_ESCALATION_WINDOW_MS` | 60 000 ms | Escalating recovery window — defined in `elero_cc1101.cpp` |
| `WATCHDOG_MAX_FLUSHES_PER_WINDOW` | 3 | L1 flush attempts before escalating to reset — defined in `elero_cc1101.cpp` |
| `WATCHDOG_MAX_RESETS_PER_WINDOW` | 3 | L2 reset attempts before marking failed — defined in `elero_cc1101.cpp` |

State constants (`ELERO_STATE_*`): `UNKNOWN`, `TOP`, `BOTTOM`, `INTERMEDIATE`, `TILT`, `BLOCKING`, `OVERHEATED`, `TIMEOUT`, `START_MOVING_UP`, `START_MOVING_DOWN`, `MOVING_UP`, `MOVING_DOWN`, `STOPPED`, `TOP_TILT`, `BOTTOM_TILT`, `OFF` (0x0f, same as `BOTTOM_TILT`), `ON` (0x10)

Key enums:
- `RadioMode` — half-duplex radio mode (`RX`, `TX`) — tracked on Core 0 only, used by ISR to route GDO0 signals
- `TxState` — TX state machine states (`IDLE`, `TRANSMITTING`, `COOLDOWN`)
- `SendResult` — return type of `send_command()` (`OK`, `QUEUE_FULL`, `FAILED`) — lets callers distinguish transient queue-full from permanent SPI failure
- `DeviceType` — device classification (`COVER = 0`, `LIGHT = 1`)

Key structs:
- `t_elero_command` — RF command parameters (counter, addresses, channel, pck_inf, hop, payload)
- `DiscoveredBlind` — discovered blind data (address, remote, channel, RSSI, state, `params_from_command` flag)
- `RuntimeBlind` — adopted blind data (extends discovered with name, `device_type`, timing config, command queue)
- `RawPacket` — captured RF packet (timestamp, FIFO data, valid flag, reject reason)
- `LogEntry` — captured log line (timestamp, level, tag, message)

Thread-safety:
- `state_mutex_` (`std::mutex`) protects `runtime_blinds_` and `discovered_blinds_` (web handler vs loop access)
- `packet_dump_mutex_` (`std::mutex`) protects `raw_packets_` (radio task vs web handler access)
- `log_mutex_` (`std::mutex`) protects all log buffer access (`append_log`, `get_log_entries_copy`, `clear_log_entries`)
- `scan_mode_`, `packet_dump_mode_`, `spi_failed_` are `std::atomic<bool>`
- `rx_ready_`, `tx_done_` are `std::atomic<bool>` — ISR-set flags routed by `radio_mode_`
- `radio_mode_` is `std::atomic<uint8_t>` with relaxed ordering — ISR may run on a different core than the radio task
- `rx_count_`, `tx_count_`, `watchdog_recovery_count_`, `tx_drop_count_`, parser drop counters, and latency metrics are `std::atomic<uint32_t>`
- `stop_urgent_count_` is `std::atomic<uint8_t>` (multi-cover auto-stop coordination)
- `task_shutdown_`, `radio_fatal_error_` are `std::atomic<bool>` for radio task lifecycle
- All `std::atomic` operations use explicit `std::memory_order_acquire`/`release` for correct multi-core ESP32 synchronization
- `get_runtime_blinds()`, `get_discovered_blinds()`, `get_raw_packets()` return copies (not const refs) for thread safety

### `components/elero/cover/EleroCover.h` / `EleroCover.cpp`

**Class:** `EleroCover : public cover::Cover, public Component, public EleroBlindBase`

Key behaviors:
- Maintains an internal `std::queue<uint8_t> commands_to_send_` for reliable delivery (capped at `ELERO_MAX_COMMAND_QUEUE`)
- Polls blind status at a configurable interval (`poll_intvl_`, default 5 min); while moving, polls every `ELERO_POLL_INTERVAL_MOVING` (5 s). When position tracking is enabled (`open_duration`/`close_duration` set), movement CHECKs are skipped because blinds broadcast their own status — this reduces RF traffic and prevents blind lockout.
- Tracks cover `position` (0.0–1.0) by dead-reckoning against `open_duration_` / `close_duration_` timestamps
- Supports tilt as a separate operation via `command_tilt_`
- Staggered poll offsets (`poll_offset_`) prevent all covers from polling simultaneously
- Auto-generates RSSI and status text sensors unless `auto_sensors: false` is set
- Stop-verify loop: after auto-stop, verifies motor actually stopped (`stop_verify_at_`, `stop_verify_retries_`)
- Runtime settings update via `apply_runtime_settings()` from the web API
- `schedule_immediate_poll()` — triggers an immediate status check (called by hub when remote command detected)

### `components/elero/light/EleroLight.h` / `EleroLight.cpp`

**Class:** `EleroLight : public light::LightOutput, public Component, public EleroLightBase`

Key behaviors:
- Implements on/off and brightness control for Elero wireless lights (dimmers)
- `dim_duration_` parameter controls brightness range: `0` = on/off only, `>0` = brightness control with dead-reckoning
- Shares the same RF protocol and command structure as covers
- Tracks brightness (0.0–1.0) by dead-reckoning during dimming operations
- Configurable command bytes: `command_on_`, `command_off_`, `command_dim_up_`, `command_dim_down_`, `command_stop_`, `command_check_`
- `ignore_write_state_` flag prevents feedback loops when `set_rx_state()` triggers `write_state()`
- Supports optional status checking via `command_check_`
- Auto-generates RSSI and status text sensors unless `auto_sensors: false` is set (same pattern as covers)
- Full web API support: `EleroLightBase` exposes identity, state, and configuration getters used by `EleroWebServer` for JSON serialization and the web UI

### `components/elero/button/elero_button.h` / `elero_button.cpp`

**Class:** `EleroScanButton : public button::Button, public Component`

Key behaviors:
- Pressing triggers `start_scan()` or `stop_scan()` on the hub depending on `scan_start_` flag
- Optional: can be linked to an `EleroLightBase` via `light_id` to send a custom `command_byte` (default `0x44`) to a light on press

### `EleroRefreshButton` (defined in `elero.h` / `elero.cpp`)

**Class:** `EleroRefreshButton : public button::Button, public Component`

Key behaviors:
- Diagnostic button entity (entity_category: diagnostic, icon: mdi:refresh)
- Sends a single CHECK command to the associated cover or light, requesting an immediate status update
- Auto-created by cover/light platforms when `auto_sensors: true` (default)
- Named `"<entity_name> Refresh"` by default
- No state machine side effects — purely a status query

### `components/elero_web/elero_web_server.h` / `elero_web_server.cpp`

**Class:** `EleroWebServer : public Component, public AsyncWebHandler`
**Optional sub-platform:** `EleroWebSwitch : public switch::Switch, public Component`

Key behaviors:
- Hosts the web UI at `http://<device-ip>/elero` (redirects `/` → `/elero`)
- Exposes REST API for RF scanning, blind/light discovery, control, runtime adoption, and diagnostics
- All endpoints support CORS via `add_cors_headers()`
- Optional HTTP Basic Auth (`username`/`password` in YAML) — when both are set, all endpoints require authentication (401 if missing/incorrect)
- `EleroWebSwitch` allows runtime enable/disable of all `/elero` endpoints (returns 503 when disabled)
- URL parsing helper: `parse_addr_url()` extracts hex address from URLs like `/elero/api/covers/0xABCDEF/command` and `/elero/api/lights/0xABCDEF/command`
- JSON fragment builders (`build_configured_json_()`, `build_discovered_array_json_()`, etc.) are reused by individual handlers and the combined status endpoint
- Frontend uses a request serialization queue (max 1 in-flight request) to prevent ESP32 socket exhaustion (ENFILE error 23)

### RF replay fixtures

Parser regression fixtures live in `tests/fixtures/rf_replay/*.replay`. Each non-comment line uses:

```text
name|hex bytes from CC1101 FIFO|expectation tokens
```

The hex bytes include the CC1101 FIFO length byte plus appended RSSI/LQI status bytes. Expectations use `key=value` tokens such as `ok=1`, `typ=0xca`, `src=0xa831e5`, `state=0x01`, or `reason=bad_crc`. `tests/unit/test_packet_replay.cpp` replays these fixtures through the pure packet parser and also covers dedup, stale-counter, and drop-bucket helper logic.

### REST API Endpoints

All endpoints are served at `http://<device-ip>/elero`. CORS is restricted to same-origin (no `Access-Control-Allow-Origin` header is set) to prevent CSRF attacks; local browser access via IP is unaffected. A 503 response is returned if the optional `elero_web` switch is disabled. All POST/DELETE endpoints also respond to OPTIONS for preflight.

**Core endpoints:**

| Endpoint | Method | Description |
|---|---|---|
| `/` | GET | Redirect to `/elero` |
| `/elero` | GET | HTML web UI |
| `/elero/api/scan/start` | POST | Start RF discovery scan |
| `/elero/api/scan/stop` | POST | Stop RF discovery scan |
| `/elero/api/discovered` | GET | JSON array of discovered blinds |
| `/elero/api/configured` | GET | JSON object with configured covers and lights |
| `/elero/api/status` | GET | Combined status: covers, lights, runtime, diagnostics (single poll). Diagnostics include RX/TX/watchdog/drop counters plus TX queue and dispatch latency last/max metrics. |
| `/elero/api/yaml` | GET | YAML snippet ready to paste into ESPHome config |
| `/elero/api/info` | GET | Device info (version, discovery count, etc.) |
| `/elero/api/runtime` | GET | JSON array of runtime-adopted blinds |

**Cover/Light control (requires address):**

| Endpoint | Method | Description |
|---|---|---|
| `/elero/api/covers/0xADDRESS/command` | POST | Send command to cover (`{"cmd": "up"\|"down"\|"stop"\|"tilt"}`) |
| `/elero/api/covers/0xADDRESS/settings` | POST | Update cover settings at runtime (timing/poll) |
| `/elero/api/lights/0xADDRESS/command` | POST | Send command to light (`{"cmd": "on"\|"off"\|"stop"}`) |

**Runtime blind adoption:**

| Endpoint | Method | Description |
|---|---|---|
| `/elero/api/discovered/0xADDRESS/adopt` | POST | Adopt a discovered blind into runtime blinds |
| `/elero/api/runtime/0xADDRESS/command` | POST | Send command to runtime-adopted blind |
| `/elero/api/runtime/0xADDRESS` | DELETE | Remove a runtime-adopted blind |
| `/elero/api/runtime/0xADDRESS/settings` | POST | Update runtime blind settings |

**Diagnostics:**

| Endpoint | Method | Description |
|---|---|---|
| `/elero/api/frequency` | GET | Current CC1101 frequency settings |
| `/elero/api/frequency/set` | POST | Update CC1101 frequency (`{"freq0": 0x7a, "freq1": 0x71, "freq2": 0x21}`) |
| `/elero/api/frequency/set_mhz` | POST | Set CC1101 frequency by MHz value (`{"mhz": 868.35}`) |
| `/elero/api/logs` | GET | Recent log entries (supports `since` query parameter) |
| `/elero/api/logs/clear` | POST | Clear captured logs |
| `/elero/api/logs/capture/start` | POST | Start capturing logs |
| `/elero/api/logs/capture/stop` | POST | Stop capturing logs |
| `/elero/api/dump/start` | POST | Start RF packet dump |
| `/elero/api/dump/stop` | POST | Stop RF packet dump |
| `/elero/api/packets` | GET | Recent captured RF packets |
| `/elero/api/packets/clear` | POST | Clear captured packets |
| `/elero/api/packets/download` | GET | Download captured RF packets as file |
| `/elero/api/diagnostics/reset` | POST | Reset diagnostic counters and latency maxima (RX, TX, watchdog recovery, parser drops, TX drops, queue/dispatch latency) |

**Web UI state (elero_web switch sub-platform):**

| Endpoint | Method | Description |
|---|---|---|
| `/elero/api/ui/status` | GET | Get web UI enabled/disabled state |
| `/elero/api/ui/enable` | POST | Enable/disable web UI (`{"enabled": true\|false}`) |

**HTTP Error Codes:**

| Code | Meaning |
|---|---|
| 200 | Success |
| 401 | Unauthorized (HTTP Basic Auth required but not provided/incorrect) |
| 409 | Conflict (e.g., trying to start scan when one is already running) |
| 503 | Service Unavailable (returned when web UI is disabled via switch) |

### Web UI Frontend Build System

The active web UI is built from source files in `components/elero_web/frontend/` using **Svelte 5**, **Vite**, **Flowbite/Flowbite-Svelte**, **Tailwind CSS**, and the `vite-plugin-singlefile` plugin:

- **Build command:** `cd components/elero_web/frontend && npm install && npm run build`
- **Build pipeline:** `vite build` → produces `dist/index.html` (single file with inlined CSS/JS) → `scripts/generate_header.mjs` → writes `../elero_web_ui.h` (C++ raw string literal wrapped in `PROGMEM`)
- **Output:** `elero_web_ui.h` is auto-generated and should not be edited by hand
- **Dev server:** `npm run dev` starts Vite dev server for frontend development
- **Legacy source:** `components/elero_web/frontend-legacy/` contains the previous Alpine.js implementation for reference/rollback only; do not build or edit it unless a maintenance task explicitly targets the legacy frontend.

---

## Naming Conventions

| Item | Convention | Example |
|---|---|---|
| C++ classes | PascalCase | `EleroCover`, `EleroWebServer`, `EleroLightBase` |
| C++ namespaces | lowercase | `esphome::elero` |
| C++ constants | `UPPER_SNAKE_CASE` with `ELERO_` prefix | `ELERO_COMMAND_COVER_UP`, `ELERO_TX_LATENCY_COMPENSATION_MS` |
| C++ enums | `PascalCase` enum class with `UPPER_CASE` values | `TxState::TRANSMITTING`, `DeviceType::COVER` |
| C++ private members | trailing underscore | `gdo0_pin_`, `scan_mode_`, `tx_state_` |
| C++ structs | PascalCase | `DiscoveredBlind`, `RuntimeBlind`, `RawPacket` |
| Python config keys | `snake_case` string constants | `"blind_address"`, `"gdo0_pin"` |
| YAML keys | `snake_case` | `blind_address`, `open_duration` |

---

## ESPHome Platform Conventions

When adding a new platform sub-component (e.g., a new sensor type):

1. Create `components/elero/<platform>/__init__.py` with:
   - `DEPENDENCIES = ["elero"]`
   - A `CONFIG_SCHEMA` using the appropriate platform schema builder
   - An `async def to_code(config)` that registers the component and connects it to the parent `Elero` hub
2. Create the corresponding `.h` and `.cpp` files in the same directory.
3. Add a `register_<platform>()` method to `Elero` in `elero.h` / `elero.cpp` if the hub needs to dispatch data to it.
4. If the hub needs to dispatch to the new entity type, create an abstract base class (like `EleroBlindBase` or `EleroLightBase`) to avoid header circular dependencies.

The `CONF_ELERO_ID` pattern is used throughout to resolve the parent hub:
```python
cv.GenerateID(CONF_ELERO_ID): cv.use_id(elero),
```
```python
parent = await cg.get_variable(config[CONF_ELERO_ID])
cg.add(var.set_elero_parent(parent))
```

### Component dependencies

| Component | `DEPENDENCIES` | `AUTO_LOAD` |
|---|---|---|
| `elero` (hub) | `["spi"]` | `["sensor"]` |
| `elero` cover | `["elero"]` | `["sensor", "text_sensor", "button"]` |
| `elero` light | `["elero"]` | `["sensor", "text_sensor", "button"]` |
| `elero` button | `["elero"]` | — |
| `elero` sensor | `["elero"]` | — |
| `elero` text_sensor | `["elero"]` | — |
| `elero_group` | `["elero"]` | `["cover"]` |
| `elero_web` | `["elero"]` | `["web_server_base"]` |
| `elero_web` switch | `["elero_web"]` | — |

### Schema validation patterns

- **Auto-sensors:** `_auto_sensor_validator()` injects RSSI sensor, status text sensor, and refresh button sub-configs at validation time when `auto_sensors: true` (default). Used by both cover and light platforms. This ensures `cv.declare_id()` is called in the correct ESPHome phase.
- **Duration consistency:** `_validate_duration_consistency()` ensures position tracking has both `open_duration` AND `close_duration` set, or both at zero.
- **Cross-platform address validation:** The light platform's `FINAL_VALIDATE_SCHEMA` checks for duplicate `blind_address` usage across covers and lights, preventing two entities from sharing the same address.
- **Poll interval:** The `poll_interval()` function converts the string `"never"` to `uint32_t` max (4 294 967 295 ms).

---

## Configuration Reference (Summary)

### Hub (`elero:`)

```yaml
elero:
  cs_pin: GPIO5          # SPI chip select (required)
  gdo0_pin: GPIO26       # CC1101 GDO0 interrupt pin (required)
  freq0: 0x7a            # CC1101 FREQ0 register (optional, default 868.35 MHz)
  freq1: 0x71            # CC1101 FREQ1 register
  freq2: 0x21            # CC1101 FREQ2 register
  send_repeats: 1        # RF packets per command (1–20, default 1 = no repeats)
  send_delay: 0ms        # Delay between repeated packets (default 0ms)
  auto_sensors: true     # Auto-generate hub diagnostic sensors (default true)
```

When `auto_sensors: true` (default), the hub auto-generates four diagnostic sensors:
- **Elero Frequency** (MHz) — current CC1101 frequency
- **Elero RX Count** — total received packets (total_increasing)
- **Elero TX Count** — total transmitted packets (total_increasing)
- **Elero Watchdog Recovery Count** — radio watchdog recovery events (total_increasing)

These can be overridden individually via `frequency_sensor`, `rx_count_sensor`, `tx_count_sensor`, `watchdog_recovery_sensor` sub-configs, or disabled entirely with `auto_sensors: false`.

Default frequency registers (`freq2=0x21, freq1=0x71, freq0=0x7a`) correspond to **868.35 MHz**. Use `freq0=0xc0` for 868.95 MHz variants.

SPI bus must be declared separately:
```yaml
spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19
```

### Cover (`cover: platform: elero`)

Required parameters:
- `blind_address` — 3-byte hex address of the motor (e.g., `0xa831e5`)
- `channel` — RF channel number of the blind (0–255)
- `remote_address` — 3-byte hex address of the remote control paired with the blind

Optional parameters (with defaults):
- `poll_interval` (default `5min`, or `never`) — how often to query blind status
- `open_duration` / `close_duration` (default `0s`) — enables position tracking (both must be set or both zero)
- `supports_tilt` (default `false`)
- `auto_sensors` (default `true`) — auto-generate RSSI sensor, status text sensor, and refresh button for this cover
- `rssi_sensor` / `status_sensor` / `refresh_button` — explicit sensor/button config (overrides auto-generated ones)
- `payload_1` (default `0x00`), `payload_2` (default `0x04`)
- `pck_inf1` (default `0x6a`), `pck_inf2` (default `0x00`)
- `hop` (default `0x0a`)
- `command_up` (0x20) / `command_down` (0x40) / `command_stop` (0x10) / `command_check` (0x00) / `command_tilt` (0x24) — override RF command bytes

### Light (`light: platform: elero`)

Required parameters:
- `blind_address` — 3-byte hex address of the light receiver (e.g., `0xc41a2b`)
- `channel` — RF channel number of the light (0–255)
- `remote_address` — 3-byte hex address of the remote control paired with the light

Optional parameters (with defaults):
- `dim_duration` (default `0s`) — time for dimming from 0% to 100%; `0s` = on/off only, `>0` = brightness control
- `auto_sensors` (default `true`) — auto-generate RSSI sensor, status text sensor, and refresh button for this light
- `rssi_sensor` / `status_sensor` / `refresh_button` — explicit sensor/button config (overrides auto-generated ones)
- `payload_1` (default `0x00`), `payload_2` (default `0x04`)
- `pck_inf1` (default `0x6a`), `pck_inf2` (default `0x00`)
- `hop` (default `0x0a`)
- `command_on` (0x20) / `command_off` (0x40) / `command_dim_up` (0x20) / `command_dim_down` (0x40) / `command_stop` (0x10) / `command_check` (0x00) — override RF command bytes

Uses `light.BRIGHTNESS_ONLY_LIGHT_SCHEMA` (no RGB/color temperature support).

### Sensors

```yaml
sensor:
  - platform: elero
    blind_address: 0xa831e5   # Required: which blind
    name: "Blind RSSI"        # Unit: dBm, device_class: signal_strength

text_sensor:
  - platform: elero
    blind_address: 0xa831e5
    name: "Blind Status"      # Values: see state constants above
```

### Buttons (RF scan + light commands)

```yaml
button:
  - platform: elero
    name: "Start Scan"
    scan_start: true           # true = start scan, false = stop scan
  - platform: elero
    name: "Stop Scan"
    scan_start: false
  # Optional: trigger a command on a light
  - platform: elero
    name: "Light Command"
    light_id: my_light         # Reference to an EleroLight
    command_byte: 0x44         # RF command byte to send (default 0x44)
```

### Group Cover (`elero_group`)

```yaml
elero_group:
  - name: "All Blinds"
    assumed_state: true    # Optional: true = buttons always enabled (default true)
    members:
      - cover_bedroom
      - cover_living_room
```

Optional parameters:
- `assumed_state` (default `true`) — when true, open/close buttons are always enabled in HA (no position feedback from group)

Requires at least 2 and at most 10 member covers. When all members share the same `remote_address` and `channel`, a single native multi-destination RF packet is sent (more efficient). Otherwise, falls back to sequential individual commands.

### Web UI (`elero_web`)

```yaml
# Use web_server_base (not web_server) to keep only the /elero UI
# web_server_base is auto-loaded by elero_web, but you can declare it
# explicitly to configure the port:
web_server_base:
  port: 80

elero_web:
  id: elero_web_ui   # Optional ID
  username: admin     # Optional: HTTP Basic Auth username
  password: secret    # Optional: HTTP Basic Auth password
```

When both `username` and `password` are set, all `/elero` endpoints require HTTP Basic Authentication.

Navigating to `http://<device-ip>/` will redirect to `/elero` automatically.

### Web UI Switch (`switch: platform: elero_web`)

Optional runtime control to enable/disable the web UI:

```yaml
switch:
  - platform: elero_web
    name: "Elero Web UI"
    restore_mode: RESTORE_DEFAULT_ON
```

When the switch is OFF, all `/elero` endpoints return HTTP 503 (Service Unavailable).

---

## Development Workflow

### Prerequisites

- ESPHome installed (`pip install esphome`)
- An ESP32 with a CC1101 module wired to SPI pins + GDO0 GPIO
- An existing Elero wireless blind system nearby for testing
- Node.js (for web UI frontend development only)

### External dependencies

- **RadioLib v7.1.2** — added automatically via `cg.add_library("jgromes/RadioLib", "7.1.2")` in the hub's `to_code()`
- **ESPHome 2026.1.0+ compatibility** — the hub's `to_code()` calls `request_log_listener()` to reserve a log listener slot for the StaticVector migration; gracefully falls back for older ESPHome versions

### Local development

Since this is an external component consumed from GitHub, local iteration requires pointing ESPHome at a local path:

```yaml
external_components:
  - source:
      type: local
      path: /path/to/esphome-elero
```

### Building and flashing

```bash
# Validate config
esphome config my_device.yaml

# Compile only
esphome compile my_device.yaml

# Compile and flash via USB
esphome run my_device.yaml

# Stream logs over serial
esphome logs my_device.yaml

# Stream logs over Wi-Fi (OTA)
esphome logs --device <ip-address> my_device.yaml
```

### Rebuilding the web UI

When modifying frontend files in `components/elero_web/frontend/`:

```bash
cd components/elero_web/frontend
npm install          # Install/update frontend dependencies
npm run build        # Vite build → generate_header.mjs → elero_web_ui.h
```

The generated `elero_web_ui.h` must be committed. Do not edit it manually.

### Claude Skills (slash commands)

The repository includes 6 Claude Code skills in `.claude/skills/`. Use these during development:

| Skill | Purpose |
|-------|---------|
| `/compile` | Run `esphome compile` on a config. Args: `minimal`, `s3`, `all`, or a YAML path. Default: `tests/configs/compile_test.yaml` |
| `/test` | Run C++ unit tests (GoogleTest) and/or Python tests (pytest). Args: `cpp`, `python`, or both |
| `/build-web-ui` | Rebuild frontend: `npm run build` → regenerate `elero_web_ui.h` |
| `/review` | Check pending changes against all 6 quality gates (CI, thread safety, buffer safety, web API, ESPHome compat, docs) |
| `/elero-protocol` | Load RF protocol reference into context: packet structure, encryption chain, state/command bytes, RSSI/frequency formulas |
| `/validate` | Run `esphome config` (schema validation only, no compile — much faster). Args: `all` or a YAML path |

### Finding blind addresses

The typical workflow for a new installation:

1. Add scan buttons and the web UI to `example.yaml`
2. Flash the device
3. Open `http://<device-ip>/elero` and press "Start Scan"
4. Operate each blind with its original remote
5. Discovered blinds appear in the web UI with addresses pre-filled
6. Download the generated YAML snippet and add it to your config

---

## CI Pipeline

CI runs automatically on pushes to `main`, `dev`, `feat/**`, `fix/**`, and `docs/**`, and on pull requests targeting `main` or `dev`. Pushes to `docs/**` run markdown validation only; full CI runs on implementation branches and PRs to `main` or `dev`. `docs/**` branches are documentation/governance-only and must not carry code, dependency, generated artifact, or runtime workflow changes. Defined in `.github/workflows/ci.yml`.

### Jobs

| Job | What it does | Trigger |
|-----|-------------|---------|
| **markdown** | `python3 scripts/check_markdown_links.py` | Every push/PR |
| **lint** | `ruff check components/` + `ruff format --check components/` | Every implementation push/PR; skipped on direct `docs/**` pushes |
| **esphome-compile** | `esphome compile` across 8 config variants (matrix, `fail-fast: false`) | Every implementation push/PR; skipped on direct `docs/**` pushes |
| **frontend-build** | `npm install` + `npm run build` from `components/elero_web/frontend/`, then verifies generated `elero_web_ui.h` | Every implementation push/PR; skipped on direct `docs/**` pushes |
| **unit-tests** | CMake configure/build plus `ctest --output-on-failure -V` | Every implementation push/PR; skipped on direct `docs/**` pushes |
| **python-tests** | `pytest tests/python/ -v --tb=short` with pinned ESPHome | Every implementation push/PR; skipped on direct `docs/**` pushes |
| **ci-ok** | Verifies markdown and required heavy jobs passed; accepts intentional heavy-job skips for direct `docs/**` pushes | Every push/PR |
| **auto-issue creation** | Parses compile log for GCC warnings/errors, creates GitHub issues labeled `ci-detected` on push events | After each compile job |

### Compile Test Matrix (8 configs)

| Config | What it tests |
|--------|--------------|
| `tests/configs/compile_test.yaml` | Full features: hub + cover + light + web + sensors + monitoring |
| `tests/configs/minimal.yaml` | Smallest valid config: hub + 1 cover, no web/light/sensors |
| `tests/configs/multi_cover.yaml` | 3 covers: position tracking, no-duration, tilt |
| `tests/configs/light_only.yaml` | 2 lights: on/off + dimmable, no covers |
| `tests/configs/no_web.yaml` | Hub + cover + light, no `elero_web` |
| `tests/configs/no_auto_sensors.yaml` | All `auto_sensors: false`, explicit sensor declarations |
| `tests/configs/custom_frequency.yaml` | Non-default frequency (868.35 MHz), `send_repeats: 3`, `send_delay: 50ms` |
| `tests/configs/esp32s3-idf.yaml` | ESP32-S3 + esp-idf framework, shared SPI (CC1101 + TFT display), 6 covers, deep sleep, `compile_process_limit: 1` |

Test configs in `tests/configs/` use `path: ../../components` to reference the local component source relative to their location.

### Auto-Issue Creation

After each compile job, the CI parses the build log for GCC warnings and errors from `components/` code. For each unique finding:
1. Deduplicates by message pattern (ignoring line numbers)
2. Checks existing open issues labeled `ci-detected` to avoid duplicates
3. Creates a GitHub issue with the label `ci-detected` + `bug` (errors) or `enhancement` (warnings)
4. Includes the compiler message, config name, commit SHA, and link to the CI run

This ensures compile warnings (like deprecation notices) automatically become trackable issues.

### Linting

**Python (Ruff):** Configured in `pyproject.toml`. Rules: `E`, `F`, `W`, `I`, `UP`, `B`. Line length 120. Run locally: `ruff check components/` and `ruff format components/`.

**C++ (clang-format / clang-tidy):** Configured in `.clang-format` (Google-based, 120 col, 2-space indent) and `.clang-tidy` (bugprone-\*, performance-\*, naming conventions). Not yet enforced in CI — baseline application pending.

### Branch Protection

The `main` branch is protected via GitHub branch protection rules:

- **Required status check:** `ci-ok` must pass (gates on all CI jobs: lint, compile, frontend-build, unit-tests, python-tests)
- **Strict mode:** PRs must be up-to-date with `main` before merging
- **Force pushes:** blocked
- **Branch deletion:** blocked
- **PR reviews:** not required (solo maintainer)
- **Admin bypass:** allowed for emergencies

Protection settings are codified in `.github/scripts/protect-main.sh`. To apply or update rules, run:

```bash
# Requires gh CLI authenticated as repo admin
bash .github/scripts/protect-main.sh
```

---

## Testing

### Automated (CI)

The CI pipeline runs lint + 8-config compile matrix on every push. See [CI Pipeline](#ci-pipeline) above.

**Planned but not yet implemented:**
- C++ unit tests with GoogleTest (see GitHub issue #133)
- Python schema validation tests with pytest (see GitHub issue #134)

### Manual Hardware Testing

Validation on real hardware before release:

1. Flash the firmware and verify the CC1101 initialises (check `esphome logs` for `[I][elero:...]` messages)
2. Use the RF scan to confirm blind discovery
3. Test each cover entity (open, close, stop) from Home Assistant
4. Test light entities (on, off, dim) if applicable
5. Verify RSSI and status text sensors update correctly
6. Test web UI discovery, adoption, and control workflows
7. Verify position tracking accuracy with `open_duration`/`close_duration`

---

## Quality Gates

These rules are enforced by the `/review` Claude skill and should be checked before merging.

### Gate 1: CI Green
All CI jobs must pass: lint clean + all 8 compile configs succeed.

### Gate 2: Thread Safety
- `std::atomic` loads use `std::memory_order_acquire`, stores use `std::memory_order_release`
- Shared data (`runtime_blinds_`, `discovered_blinds_`) accessed under `state_mutex_`
- Log buffer accessed under `log_mutex_`
- No SPI access outside Core 0 after `setup()` completes

### Gate 3: Buffer Safety
- Array accesses in protocol code preceded by length checks
- `msg_rx_` indices validated against `ELERO_MAX_PACKET_SIZE` (57)
- Packet length ≥ `ELERO_MIN_PACKET_SIZE` (17) before parsing
- FreeRTOS queue operations check return values

### Gate 4: Web API Consistency
- New endpoints have CORS headers via `add_cors_headers()`
- Auth check guarded with `#ifdef USE_WEBSERVER_AUTH`
- POST/DELETE endpoints also handle OPTIONS for preflight
- User-controlled strings passed through `json_escape()` before embedding in JSON

### Gate 5: ESPHome Compatibility
- Python imports from `esphome.components` must be aliased to avoid shadowing by local sub-packages (e.g., `from esphome.components import sensor as esphome_sensor`)
- Conditional defines (`USE_WEBSERVER_AUTH`) added in Python codegen when features are configured
- Watch for deprecated ESPHome APIs — CI auto-creates issues for deprecation warnings

### Gate 6: Documentation
- New YAML parameters documented in `docs/user/configuration.md`
- New constants added to the key constants table in this document
- New REST API endpoints added to the REST API table in this document

---

## ESP32 System Monitoring Sensors

The `tests/configs/compile_test.yaml` includes system monitoring sensors for Home Assistant to track dual-core health:

### RAM
- **Free Heap** / **Largest Free Block** — via `debug` platform (bytes, 10s)
- **Free RAM (KB)** — `ESP.getFreeHeap()` template sensor (10s)
- **Min Free RAM (KB)** — `ESP.getMinFreeHeap()` — tracks memory leaks over time (30s)

### Flash Storage
- **Flash Used (KB)** — `ESP.getSketchSize()` (5min)
- **Flash Free (KB)** — `ESP.getFreeSketchSpace()` — available for OTA (5min)

### Core Utilization
- **Core 0 Usage (%)** — FreeRTOS idle task runtime stats (radio core, 10s)
- **Core 1 Usage (%)** — FreeRTOS idle task runtime stats (app core, 10s)
- **Loop Time** — `debug` platform, main loop iteration time (10s)

### Radio Task Health
- **Radio Task Stack Free** — `uxTaskGetStackHighWaterMark("elero_radio")` in bytes (30s)

### General
- **WiFi Signal** (dBm, 30s), **Uptime** (seconds, 60s), **ESP32 Temperature** (30s)
- **Device Info**, **Reset Reason** (text sensors, at boot)

**Important**: The `Core 0/1 Usage (%)` sensors require `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS` which is available on the ESP-IDF framework. On Arduino framework these sensors return `NAN` (show as "Unknown" in HA). To enable them, switch to `framework: type: esp-idf` in your YAML config.

---

## Common Pitfalls

- **Wrong frequency**: Most European Elero motors use 868.35 MHz (`freq0=0x7a`). Some use 868.95 MHz (`freq0=0xc0`). If discovery finds nothing, try the alternate frequency. Use the `/elero/api/frequency/set` endpoint to test at runtime.
- **ESP32 strapping pins and SPI**: Do **not** use GPIO12 as SPI MISO (or any SPI signal). GPIO12 is a strapping pin that controls VDD_SDIO voltage at boot. If the CC1101 module pulls it HIGH, VDD_SDIO is set to 1.8V, breaking all SPI communication (symptoms: all SPI write verify fail with `rc=-16`, MARCSTATE stuck at `0x00`). Safe SPI pins: CLK=GPIO18, MISO=GPIO19, MOSI=GPIO23. Avoid GPIO0, GPIO2, GPIO5, GPIO12, GPIO15 for SPI signals. The component detects persistent SPI failure at runtime and marks itself as failed with a diagnostic error message.
- **SPI conflicts**: The CC1101 CS pin must not be shared with any other SPI device.
- **Using `web_server:` instead of `web_server_base:`**: Adding `web_server:` to your YAML re-enables the default ESPHome entity UI at `/`. Use `web_server_base:` (or rely on its auto-load via `elero_web`) to serve only the Elero UI at `/elero`. Navigating to `/` will redirect automatically to `/elero`.
- **Position tracking**: Leave `open_duration` and `close_duration` at `0s` if you only need open/close without position — setting incorrect durations causes wrong position estimates. Both must be set or both zero (enforced by `_validate_duration_consistency`).
- **Poll interval `never`**: Set `poll_interval: never` for blinds that reliably push state updates (avoids unnecessary RF traffic). Internally this maps to `uint32_t` max (4 294 967 295 ms).
- **TX busy**: `send_command()` and `send_command_priority()` return `false` when the respective FreeRTOS queue is full. Callers should handle the rejection (the `tx_drop_count` diagnostic counter tracks dropped commands).
- **CC1101 SFTX/SFRX validity**: SFTX is only valid in IDLE or TXFIFO_UNDERFLOW states; SFRX is only valid in IDLE or RXFIFO_OVERFLOW states (per CC1101 datasheet). Issuing these strobes in other states silently corrupts radio state. The TX state machine must respect this.
- **RX FIFO stale data after TX**: When `standby()` interrupts an in-progress packet reception, partial data remains in the RX FIFO. `send_command()` flushes both FIFOs after entering IDLE to prevent `process_rx()` from misinterpreting stale data after TX completes.
- **RXFIFO_OVERFLOW during TX**: While TX is active, `process_rx()` does not run, so RX FIFO overflow goes undetected until the post-TX FIFO health check or the 5-second radio watchdog catches it.
- **Command queue overflow**: Each blind's command queue is capped at `ELERO_MAX_COMMAND_QUEUE` (10) to prevent OOM on ESP32.
- **ESP32-S3 compile OOM**: On memory-constrained build machines, the ESP32-S3 toolchain (`xtensa-esp-elf-g++`) can be killed by the OS during parallel compilation of large `.cpp` files (symptoms: `fatal error: Killed signal terminated program cc1plus`). Fix by limiting parallel compile jobs: add `compile_process_limit: 1` under `esphome:` in your YAML config. This serializes compilation, trading speed for reliability.
- **Web UI `elero_web_ui.h`**: This file is auto-generated by the frontend build system. Always rebuild via `npm run build` in the `frontend/` directory — never edit by hand.

---

## Contributing

- Follow the existing naming conventions for C++ and Python code.
- Keep the `EleroBlindBase` and `EleroLightBase` interfaces minimal — the hub should not depend on cover/light internals.
- Test changes on real hardware when the change depends on RF/device behavior; otherwise state clearly which local/CI checks were run and that hardware validation was not performed.
- Document new configuration parameters in both `README.md` and `docs/user/configuration.md`.
- When modifying the active web UI, rebuild `elero_web_ui.h` and commit it alongside the source changes.
- Use `dev` as the shared integration branch and prefer `feat/<short-topic>`, `fix/<short-topic>`, or `docs/<short-topic>` branches so push CI runs predictably. Use `docs/<short-topic>` only for documentation/governance changes because direct pushes there run markdown validation only; move to `feat/**` or `fix/**` if code, dependencies, generated artifacts, or runtime workflows need to change.

### Before merging

- **CI must pass** — all 8 compile configs green + lint clean.
- **Run `ruff check components/`** before committing Python changes. Use `ruff check --fix` for auto-fixable issues.
- **Python imports** — when importing `sensor`, `text_sensor`, or other platform modules from `esphome.components` inside a component that has a same-named sub-package, alias the import (e.g., `from esphome.components import sensor as esphome_sensor`).
- **Compile warnings become issues** — CI auto-creates GitHub issues from GCC warnings/errors (labeled `ci-detected`). Fix them or close with explanation.
- **Use `/review`** — the Claude skill checks all 6 quality gates before you submit.
