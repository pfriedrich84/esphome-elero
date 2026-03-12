# CLAUDE.md — esphome-elero

This file provides guidance for AI assistants working in this repository. It describes the project structure, key conventions, and development workflows.

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
│   └── FUNDING.yml                    # GitHub Sponsors config
├── .gitignore                         # Python cache, .esphome/ exclusions
├── CLAUDE.md                          # This file
├── IMPROVEMENT_PLAN.md                # Identified improvement issues (German)
├── README.md                          # Main documentation (German + English)
├── example.yaml                       # Complete ESPHome config example
├── docs/
│   ├── INSTALLATION.md                # Step-by-step hardware and software setup
│   ├── CONFIGURATION.md               # Full parameter reference
│   ├── RADIOLIB_EVALUATION.md         # RadioLib integration evaluation
│   └── examples/                      # Additional YAML examples (.gitkeep)
└── components/
    ├── elero/                         # Main hub component
    │   ├── __init__.py                # ESPHome component schema & code-gen (hub)
    │   ├── elero.h                    # C++ hub class header (~457 lines)
    │   ├── elero.cpp                  # C++ RF protocol implementation (~1200 lines)
    │   ├── cc1101.h                   # CC1101 register map & command strobes
    │   ├── cover/                     # Cover (blind) platform
    │   │   ├── __init__.py            # Cover schema, auto-sensors, validation
    │   │   ├── EleroCover.h           # Cover class header (120 lines)
    │   │   └── EleroCover.cpp         # Cover logic, position tracking (405 lines)
    │   ├── light/                     # Light (dimmer) platform
    │   │   ├── __init__.py            # Light schema & code-gen
    │   │   ├── EleroLight.h           # Light class header (~127 lines)
    │   │   └── EleroLight.cpp         # Light logic, brightness tracking (~236 lines)
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
        ├── elero_web_server.cpp       # REST API + CORS (878 lines)
        ├── elero_web_ui.h             # AUTO-GENERATED: embedded HTML/JS/CSS
        ├── switch/                    # Web UI enable/disable switch sub-platform
        │   ├── __init__.py            # Switch schema (depends on elero_web)
        │   ├── elero_web_switch.h     # Switch class header
        │   └── elero_web_switch.cpp   # Switch logic (31 lines)
        └── frontend/                  # Web UI source (Vite + Alpine.js build)
            ├── package.json           # npm project (Vite + Alpine.js)
            ├── package-lock.json      # Dependency lockfile
            ├── vite.config.js         # Vite bundler config (single-file output)
            ├── index.html             # HTML template (336 lines)
            ├── scripts/
            │   └── generate_header.mjs  # Post-build: HTML → elero_web_ui.h
            └── src/
                ├── main.js            # Frontend application logic (408 lines)
                └── style.css          # Frontend styles (386 lines)
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
├── sensor::Sensor (RSSI, registered per blind address)
├── text_sensor::TextSensor (status, registered per blind address)
├── EleroWebServer (Component + AsyncWebHandler, wraps web_server_base)
│   └── EleroWebSwitch (switch::Switch + Component)
├── RuntimeBlind (adopted from web UI, stored in std::map, supports DeviceType)
└── Auto-registered sensors/text sensors per cover (optional via auto_sensors)
```

The abstract base classes `EleroBlindBase` and `EleroLightBase` decouple the hub (`Elero`) from the cover/light implementations so `elero.h` never needs to `#include` the cover or light headers. All communication between hub and entities goes through virtual methods.

### RadioLib integration

The hub uses [RadioLib](https://github.com/jgromes/RadioLib) v7.1.2 (added via PlatformIO `cg.add_library()`) as a hardware abstraction layer for the CC1101 transceiver. A custom `EspHomeRadioLibHal` adapter class bridges ESPHome's `SPIDevice` to RadioLib's HAL interface — GPIO, interrupt, and SPI lifecycle operations are forwarded to ESPHome primitives while SPI transfers delegate to the parent `Elero` component. RadioLib provides:

- `radio_->standby()` — synchronous IDLE transition (~1 ms), replacing the old multi-state async SIDLE approach
- `radio_module_->SPIsetRegValue()` / `SPIgetRegValue()` — register access with verify-readback for init/config
- `radio_->begin()` — initial CC1101 configuration (frequency, bandwidth, data rate, etc.)

The `Elero` class owns the RadioLib instances (`radio_hal_`, `radio_module_`, `radio_`) and cleans them up in its destructor.

### Non-blocking TX state machine

The radio uses a simplified 3-state non-blocking TX state machine that processes one step per `loop()` iteration:

```
IDLE → TRANSMITTING → COOLDOWN → IDLE
```

RadioLib's `standby()` handles the IDLE transition synchronously in `send_command()`, eliminating the previous `GOING_IDLE`, `LOADING`, `FIRING`, and `VERIFYING` states. See `TxState` enum in `elero.h:27–31`. The hub checks `is_tx_idle()` before accepting new commands.

TX initiation in `send_command()`:
1. `radio_->standby()` — blocks until CC1101 is in IDLE (~1 ms)
2. Flush both TX and RX FIFOs (valid in IDLE per CC1101 spec)
3. Load TX FIFO via burst write
4. Issue `STX` strobe → state transitions to `TRANSMITTING`

TX completion is detected by polling MARCSTATE — when it leaves TX, the CC1101 has auto-transitioned to RX via MCSM1 TXOFF_MODE.

### Interrupt handling

A single `std::atomic<bool>` flag `rx_ready_` is set by the GDO0 ISR when a packet is received. All atomic operations use `std::memory_order_acquire` for loads and `std::memory_order_release` for stores to ensure correct multi-core ESP32 synchronization. The ISR always sets `rx_ready_`, but `process_rx()` only runs when TX is idle, so stale flags during TX are harmlessly ignored.

### Radio health and FIFO recovery

The CC1101 can enter unrecoverable states (RXFIFO_OVERFLOW, stuck IDLE) during TX operations. Several mechanisms prevent and recover from these:

- **FIFO flush before TX** — `send_command()` uses `standby()` to enter IDLE, then flushes both TX and RX FIFOs. The RX flush discards any partial packet data from the reception that SIDLE interrupted.
- **No SFTX after TX completion** — The CC1101 auto-transitions to RX via MCSM1 TXOFF_MODE after TX. Issuing SFTX in this state is invalid per the CC1101 datasheet (only valid in IDLE or TXFIFO_UNDERFLOW) and can corrupt radio state.
- **Post-TX FIFO health check** — After COOLDOWN, before resuming normal RX, the code reads RXBYTES to detect overflow or pending data that arrived during TX.
- **Radio watchdog** (`check_radio_state_()`, every 5 s) — Reads CC1101 MARCSTATE register and recovers from: RXFIFO_OVERFLOW (flush + restart RX), stuck IDLE (restart RX), or any other unexpected state (full radio reinit). Only runs when TX is idle.
- **TX cooldown** — 10 ms settling time after TX before resuming RX, allowing the CC1101 PLL to stabilize.
- **Minimum packet validation** — Packets shorter than `ELERO_MIN_PACKET_SIZE` (17 bytes) are rejected as non-Elero RF noise.

### Data flow

1. `Elero::setup()` configures CC1101 via RadioLib's `begin()` and direct register writes, then attaches a GPIO interrupt on `gdo0_pin`.
2. When the CC1101 signals a received packet (GDO0 interrupt), the ISR sets `rx_ready_`.
3. `Elero::loop()` calls `process_rx()` when TX is idle or in cooldown (drains up to `ELERO_MAX_RX_PER_LOOP` packets per call), reads the FIFO, decodes and decrypts the packet, then dispatches the state to the matching `EleroBlindBase`/`EleroLightBase` via `set_rx_state()`.
4. `Elero::loop()` calls `advance_tx()` to progress the TX state machine one step.
5. When TX is idle, `Elero::loop()` drains runtime blind command queues and runs `check_radio_state_()` for periodic health monitoring.
6. `EleroCover::loop()` / `EleroLight::loop()` handle polling timers, position/brightness recomputation, and drain the command queue by calling `parent_->send_command()`.

---

## Key Classes and Files

### `components/elero/elero.h` / `elero.cpp`

**Class:** `Elero : public spi::SPIDevice<...>, public Component`
**Namespace:** `esphome::elero`

Critical public API:
- `register_cover(EleroBlindBase*)` — called by each `EleroCover` at setup
- `register_light(EleroLightBase*)` — called by each `EleroLight` at setup
- `send_command(t_elero_command*)` — encodes, encrypts, and transmits a command (returns false if TX busy)
- `is_tx_idle()` — check if TX state machine is ready for a new command
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

Diagnostics:
- `start_packet_dump()` / `stop_packet_dump()` / `get_raw_packets()` — RF packet capture (ring buffer, max 50)
- `append_log()` / `get_log_entries_copy()` / `set_log_capture()` — persistent log buffer (max 200 entries, mutex-protected)
- `reinit_frequency(freq2, freq1, freq0)` — change CC1101 frequency at runtime

Key constants (defined in `elero.h`):

| Constant | Value | Purpose |
|---|---|---|
| `ELERO_MAX_PACKET_SIZE` | 57 | Maximum RF packet length (FCC spec) |
| `ELERO_MIN_PACKET_SIZE` | 17 | Minimum valid Elero packet (shorter = RF noise) |
| `ELERO_POLL_INTERVAL_MOVING` | 2 000 ms | Status poll while blind is moving |
| `ELERO_TIMEOUT_MOVEMENT` | 120 000 ms | Give up movement tracking after 2 min |
| `ELERO_POST_MOVEMENT_POLL_DELAY` | 5 000 ms | Poll delay after open/close duration elapses |
| `ELERO_SEND_RETRIES` | 3 | Command retry count |
| `ELERO_DEFAULT_SEND_REPEATS` | 5 | Default RF packet repetitions per command (configurable 1–20) |
| `ELERO_DEFAULT_SEND_DELAY` | 1 ms | Default delay between repeated packets (configurable) |
| `ELERO_MAX_COMMAND_QUEUE` | 10 | Max queued commands per blind (prevents OOM) |
| `ELERO_MAX_DISCOVERED` | 20 | Max blinds tracked in scan mode |
| `ELERO_MAX_RAW_PACKETS` | 50 | Max raw packets in dump ring buffer |
| `ELERO_MAX_RX_PER_LOOP` | 4 | Max packets drained per `process_rx()` call |
| `ELERO_POLL_STAGGER_MS` | 5 000 ms | Stagger offset between cover poll timers |
| `ELERO_STOP_REPEAT_COUNT` | 2 | Stop commands queued on auto-stop (x2 RF packets each) |
| `ELERO_TX_LATENCY_COMPENSATION_MS` | 150 ms | Position check lead time for TX pipeline delay |
| `ELERO_STOP_VERIFY_DELAY_MS` | 500 ms | Delay before polling to verify motor stopped |
| `ELERO_STOP_VERIFY_MAX_RETRIES` | 3 | Max stop-verify cycles before giving up |
| `ELERO_MSG_LENGTH` | 0x1d (29) | Fixed message length for TX |
| `ELERO_CRYPTO_MULT` | 0x708f | Encryption multiplier for counter-based code |
| `TX_STATE_TIMEOUT_MS` | 50 ms | Per-state watchdog timeout for TX state machine |
| `TX_COOLDOWN_MS` | 10 ms | Post-TX settle time before resuming RX |
| `RADIO_WATCHDOG_MS` | 5 000 ms | Periodic radio health check interval |

State constants (`ELERO_STATE_*`): `UNKNOWN`, `TOP`, `BOTTOM`, `INTERMEDIATE`, `TILT`, `BLOCKING`, `OVERHEATED`, `TIMEOUT`, `START_MOVING_UP`, `START_MOVING_DOWN`, `MOVING_UP`, `MOVING_DOWN`, `STOPPED`, `TOP_TILT`, `BOTTOM_TILT`, `OFF` (0x0f, same as `BOTTOM_TILT`), `ON` (0x10)

Key enums:
- `TxState` — TX state machine states (`IDLE`, `TRANSMITTING`, `COOLDOWN`)
- `DeviceType` — device classification (`COVER = 0`, `LIGHT = 1`)

Key structs:
- `t_elero_command` — RF command parameters (counter, addresses, channel, pck_inf, hop, payload)
- `DiscoveredBlind` — discovered blind data (address, remote, channel, RSSI, state, `params_from_command` flag)
- `RuntimeBlind` — adopted blind data (extends discovered with name, `device_type`, timing config, command queue)
- `RawPacket` — captured RF packet (timestamp, FIFO data, valid flag, reject reason)
- `LogEntry` — captured log line (timestamp, level, tag, message)

Thread-safety:
- `log_mutex_` (`std::mutex`) protects all log buffer access (`append_log`, `get_log_entries_copy`, `clear_log_entries`)
- All `std::atomic` operations use explicit `std::memory_order_acquire`/`release` for correct multi-core ESP32 ISR synchronization

### `components/elero/cover/EleroCover.h` / `EleroCover.cpp`

**Class:** `EleroCover : public cover::Cover, public Component, public EleroBlindBase`

Key behaviors:
- Maintains an internal `std::queue<uint8_t> commands_to_send_` for reliable delivery (capped at `ELERO_MAX_COMMAND_QUEUE`)
- Polls blind status at a configurable interval (`poll_intvl_`, default 5 min); while moving, polls every `ELERO_POLL_INTERVAL_MOVING` (2 s)
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
- Full web API support: `EleroLightBase` exposes identity, state, and configuration getters used by `EleroWebServer` for JSON serialization and the web UI

### `components/elero/button/elero_button.h` / `elero_button.cpp`

**Class:** `EleroScanButton : public button::Button, public Component`

Key behaviors:
- Pressing triggers `start_scan()` or `stop_scan()` on the hub depending on `scan_start_` flag
- Optional: can be linked to an `EleroLightBase` via `light_id` to send a custom `command_byte` (default `0x44`) to a light on press

### `components/elero_web/elero_web_server.h` / `elero_web_server.cpp`

**Class:** `EleroWebServer : public Component, public AsyncWebHandler`
**Optional sub-platform:** `EleroWebSwitch : public switch::Switch, public Component`

Key behaviors:
- Hosts the web UI at `http://<device-ip>/elero` (redirects `/` → `/elero`)
- Exposes REST API for RF scanning, blind/light discovery, control, runtime adoption, and diagnostics
- All endpoints support CORS via `add_cors_headers()`
- `EleroWebSwitch` allows runtime enable/disable of all `/elero` endpoints (returns 503 when disabled)
- URL parsing helper: `parse_addr_url()` extracts hex address from URLs like `/elero/api/covers/0xABCDEF/command` and `/elero/api/lights/0xABCDEF/command`
- JSON fragment builders (`build_configured_json_()`, `build_discovered_array_json_()`, etc.) are reused by individual handlers and the combined status endpoint
- Frontend uses a request serialization queue (max 1 in-flight request) to prevent ESP32 socket exhaustion (ENFILE error 23)

### REST API Endpoints

All endpoints are served at `http://<device-ip>/elero` and support CORS. A 503 response is returned if the optional `elero_web` switch is disabled. All POST endpoints also respond to OPTIONS for CORS preflight.

**Core endpoints:**

| Endpoint | Method | Description |
|---|---|---|
| `/` | GET | Redirect to `/elero` |
| `/elero` | GET | HTML web UI |
| `/elero/api/scan/start` | POST | Start RF discovery scan |
| `/elero/api/scan/stop` | POST | Stop RF discovery scan |
| `/elero/api/discovered` | GET | JSON array of discovered blinds |
| `/elero/api/configured` | GET | JSON object with configured covers and lights |
| `/elero/api/status` | GET | Combined status: covers, lights, runtime, diagnostics (single poll) |
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
| `/elero/api/runtime/0xADDRESS/remove` | POST | Remove a runtime-adopted blind |
| `/elero/api/runtime/0xADDRESS/settings` | POST | Update runtime blind settings |

**Diagnostics:**

| Endpoint | Method | Description |
|---|---|---|
| `/elero/api/frequency` | GET | Current CC1101 frequency settings |
| `/elero/api/frequency/set` | POST | Update CC1101 frequency (`{"freq0": 0x7a, "freq1": 0x71, "freq2": 0x21}`) |
| `/elero/api/logs` | GET | Recent log entries (supports `since` query parameter) |
| `/elero/api/logs/clear` | POST | Clear captured logs |
| `/elero/api/logs/capture/start` | POST | Start capturing logs |
| `/elero/api/logs/capture/stop` | POST | Stop capturing logs |
| `/elero/api/dump/start` | POST | Start RF packet dump |
| `/elero/api/dump/stop` | POST | Stop RF packet dump |
| `/elero/api/packets` | GET | Recent captured RF packets |
| `/elero/api/packets/clear` | POST | Clear captured packets |

**Web UI state (elero_web switch sub-platform):**

| Endpoint | Method | Description |
|---|---|---|
| `/elero/api/ui/status` | GET | Get web UI enabled/disabled state |
| `/elero/api/ui/enable` | POST | Enable/disable web UI (`{"enabled": true\|false}`) |

**HTTP Error Codes:**

| Code | Meaning |
|---|---|
| 200 | Success |
| 409 | Conflict (e.g., trying to start scan when one is already running) |
| 503 | Service Unavailable (returned when web UI is disabled via switch) |

### Web UI Frontend Build System

The web UI is built from source files in `components/elero_web/frontend/` using **Vite** with the `vite-plugin-singlefile` plugin and **Alpine.js** for reactivity:

- **Build command:** `cd components/elero_web/frontend && npm run build`
- **Build pipeline:** `vite build` → produces `dist/index.html` (single file with inlined CSS/JS) → `scripts/generate_header.mjs` → writes `../elero_web_ui.h` (C++ raw string literal wrapped in `PROGMEM`)
- **Output:** `elero_web_ui.h` is auto-generated and should not be edited by hand
- **Dev server:** `npm run dev` starts Vite dev server for frontend development

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
| `elero` (hub) | `["spi"]` | — |
| `elero` cover | `["elero"]` | `["sensor", "text_sensor"]` |
| `elero` light | `["elero"]` | — |
| `elero` button | `["elero"]` | — |
| `elero` sensor | `["elero"]` | — |
| `elero` text_sensor | `["elero"]` | — |
| `elero_web` | `["elero"]` | `["web_server_base"]` |
| `elero_web` switch | `["elero_web"]` | — |

### Schema validation patterns

- **Cover auto-sensors:** `_auto_sensor_validator()` injects RSSI and status sensor sub-configs at validation time when `auto_sensors: true` (default). This ensures `cv.declare_id()` is called in the correct ESPHome phase.
- **Duration consistency:** `_validate_duration_consistency()` ensures position tracking has both `open_duration` AND `close_duration` set, or both at zero.
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
  send_repeats: 5        # RF packet repetitions per command (1–20, default 5)
  send_delay: 1ms        # Delay between repeated packets (default 1ms)
```

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
- `auto_sensors` (default `true`) — auto-generate RSSI and status text sensors for this cover
- `rssi_sensor` / `status_sensor` — explicit sensor config (overrides auto-generated ones)
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

### Web UI (`elero_web`)

```yaml
# Use web_server_base (not web_server) to keep only the /elero UI
# web_server_base is auto-loaded by elero_web, but you can declare it
# explicitly to configure the port:
web_server_base:
  port: 80

elero_web:
  id: elero_web_ui   # Optional ID
```

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
npm install          # First time only
npm run build        # Vite build → generate_header.mjs → elero_web_ui.h
```

The generated `elero_web_ui.h` must be committed. Do not edit it manually.

### Finding blind addresses

The typical workflow for a new installation:

1. Add scan buttons and the web UI to `example.yaml`
2. Flash the device
3. Open `http://<device-ip>/elero` and press "Start Scan"
4. Operate each blind with its original remote
5. Discovered blinds appear in the web UI with addresses pre-filled
6. Download the generated YAML snippet and add it to your config

---

## Testing

There are no automated tests in this repository. Validation is done manually on real hardware:

1. Flash the firmware and verify the CC1101 initialises (check `esphome logs` for `[I][elero:...]` messages)
2. Use the RF scan to confirm blind discovery
3. Test each cover entity (open, close, stop) from Home Assistant
4. Test light entities (on, off, dim) if applicable
5. Verify RSSI and status text sensors update correctly
6. Test web UI discovery, adoption, and control workflows
7. Verify position tracking accuracy with `open_duration`/`close_duration`

---

## Common Pitfalls

- **Wrong frequency**: Most European Elero motors use 868.35 MHz (`freq0=0x7a`). Some use 868.95 MHz (`freq0=0xc0`). If discovery finds nothing, try the alternate frequency. Use the `/elero/api/frequency/set` endpoint to test at runtime.
- **ESP32 strapping pins and SPI**: Do **not** use GPIO12 as SPI MISO (or any SPI signal). GPIO12 is a strapping pin that controls VDD_SDIO voltage at boot. If the CC1101 module pulls it HIGH, VDD_SDIO is set to 1.8V, breaking all SPI communication (symptoms: all SPI write verify fail with `rc=-16`, MARCSTATE stuck at `0x00`). Safe SPI pins: CLK=GPIO18, MISO=GPIO19, MOSI=GPIO23. Avoid GPIO0, GPIO2, GPIO5, GPIO12, GPIO15 for SPI signals. The component detects persistent SPI failure at runtime and marks itself as failed with a diagnostic error message.
- **SPI conflicts**: The CC1101 CS pin must not be shared with any other SPI device.
- **Using `web_server:` instead of `web_server_base:`**: Adding `web_server:` to your YAML re-enables the default ESPHome entity UI at `/`. Use `web_server_base:` (or rely on its auto-load via `elero_web`) to serve only the Elero UI at `/elero`. Navigating to `/` will redirect automatically to `/elero`.
- **Position tracking**: Leave `open_duration` and `close_duration` at `0s` if you only need open/close without position — setting incorrect durations causes wrong position estimates. Both must be set or both zero (enforced by `_validate_duration_consistency`).
- **Poll interval `never`**: Set `poll_interval: never` for blinds that reliably push state updates (avoids unnecessary RF traffic). Internally this maps to `uint32_t` max (4 294 967 295 ms).
- **TX busy**: `send_command()` returns `false` when the TX state machine is not idle. Callers must check `is_tx_idle()` or handle the rejection.
- **CC1101 SFTX/SFRX validity**: SFTX is only valid in IDLE or TXFIFO_UNDERFLOW states; SFRX is only valid in IDLE or RXFIFO_OVERFLOW states (per CC1101 datasheet). Issuing these strobes in other states silently corrupts radio state. The TX state machine must respect this.
- **RX FIFO stale data after TX**: When `standby()` interrupts an in-progress packet reception, partial data remains in the RX FIFO. `send_command()` flushes both FIFOs after entering IDLE to prevent `process_rx()` from misinterpreting stale data after TX completes.
- **RXFIFO_OVERFLOW during TX**: While TX is active, `process_rx()` does not run, so RX FIFO overflow goes undetected until the post-TX FIFO health check or the 5-second radio watchdog catches it.
- **Command queue overflow**: Each blind's command queue is capped at `ELERO_MAX_COMMAND_QUEUE` (10) to prevent OOM on ESP32.
- **Web UI `elero_web_ui.h`**: This file is auto-generated by the frontend build system. Always rebuild via `npm run build` in the `frontend/` directory — never edit by hand.

---

## Contributing

- Follow the existing naming conventions for C++ and Python code.
- Keep the `EleroBlindBase` and `EleroLightBase` interfaces minimal — the hub should not depend on cover/light internals.
- Test changes on real hardware before opening a pull request.
- Document new configuration parameters in both `README.md` and `docs/CONFIGURATION.md`.
- When modifying the web UI, rebuild `elero_web_ui.h` and commit it alongside the source changes.
- The primary development branch convention used by automation is `claude/<session-id>`.
