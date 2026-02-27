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
- Control Elero wireless lights (on/off and brightness dimming)
- Receive status feedback (top, bottom, moving, blocked, overheated, etc.)
- Track cover position based on movement timing (dead-reckoning)
- Post-movement status poll 5 s after travel time elapses to confirm final position
- Immediate status poll when a physical remote command is detected over RF
- RSSI signal strength monitoring per blind
- RF discovery scan to find nearby blinds (web UI and log-based)
- Runtime blind adoption via web UI (no reflash needed)
- RF packet dump mode for low-level debugging
- Realtime log capture accessible via REST API
- Runtime CC1101 frequency switching
- Optional web UI served at `http://<device-ip>/elero` for discovery and YAML generation
- Optional persistent event logging (LittleFS ring buffer) for RF packets, state transitions, and commands

**Upstream credits:**
- Encryption/decryption: [QuadCorei8085/elero_protocol](https://github.com/QuadCorei8085/elero_protocol) (MIT)
- Remote handling: [stanleypa/eleropy](https://github.com/stanleypa/eleropy) (GPLv3)
- Based on the no-longer-maintained [andyboeh/esphome-elero](https://github.com/pfriedrich84/esphome-elero)

---

## Repository Structure

```
esphome-elero/
├── CLAUDE.md                          # This file
├── README.md                          # Main documentation (German + English)
├── example.yaml                       # Complete ESPHome config example
├── docs/
│   ├── INSTALLATION.md                # Step-by-step hardware and software setup
│   ├── CONFIGURATION.md               # Full parameter reference
│   └── examples/                      # Additional YAML examples (currently empty)
└── components/
    ├── elero/                         # Main hub component
    │   ├── __init__.py                # ESPHome component schema & code-gen (hub)
    │   ├── elero.h                    # C++ hub class header
    │   ├── elero.cpp                  # C++ RF protocol implementation (~1065 lines)
    │   ├── elero_log.h                # Persistent event log header (EleroEventLog, PersistentLogEntry)
    │   ├── elero_log.cpp              # LittleFS ring buffer implementation
    │   ├── elero_storage.h            # LittleFS persistent storage header (EleroStorage)
    │   ├── elero_storage.cpp          # Persistent storage: runtime blinds, RF packets, blind states
    │   ├── cc1101.h                   # CC1101 register map & command strobes
    │   ├── cover/                     # Cover (blind) platform
    │   │   ├── __init__.py            # Cover schema & code-gen
    │   │   ├── EleroCover.h           # Cover class header
    │   │   └── EleroCover.cpp         # Cover logic, position tracking (~360 lines)
    │   ├── light/                     # Light (dimmer) platform
    │   │   ├── __init__.py            # Light schema & code-gen
    │   │   ├── EleroLight.h           # Light class header
    │   │   └── EleroLight.cpp         # Light logic, dimming (~225 lines)
    │   ├── button/                    # Scan button platform
    │   │   ├── __init__.py
    │   │   ├── elero_button.h
    │   │   └── elero_button.cpp
    │   ├── sensor/                    # RSSI sensor platform
    │   │   └── __init__.py
    │   └── text_sensor/               # Blind status text sensor platform
    │       └── __init__.py
    └── elero_web/                     # Optional web UI component
        ├── __init__.py
        ├── elero_web_server.h         # Web server header
        ├── elero_web_server.cpp       # REST API + CORS (~930 lines)
        ├── elero_web_ui.h             # Inline web UI HTML/JS
        ├── frontend/                  # Build-time web UI source
        │   ├── index.html
        │   ├── src/main.js
        │   └── vite.config.js
        └── switch/                    # Web UI enable/disable switch sub-platform
            ├── __init__.py
            ├── elero_web_switch.h
            └── elero_web_switch.cpp
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
├── EleroBlindBase (abstract interface for covers)
│   └── EleroCover (cover::Cover + Component + EleroBlindBase)
├── EleroLightBase (abstract interface for lights)
│   └── EleroLight (light::LightOutput + Component + EleroLightBase)
├── EleroScanButton (button::Button + Component)
├── sensor::Sensor (RSSI, registered per blind address)
├── text_sensor::TextSensor (status, registered per blind address)
├── EleroWebServer (Component, wraps web_server_base)
│   └── EleroWebSwitch (switch::Switch + Component)
└── Auto-registered sensors/text sensors per cover (optional)
```

The `EleroBlindBase` and `EleroLightBase` abstract classes decouple the hub (`Elero`) from the cover/light implementations so `elero.h` never needs to `#include` the cover or light headers. All communication between hub and entities goes through virtual methods.

### Data flow

1. `Elero::setup()` configures CC1101 registers over SPI, mounts LittleFS storage, and attaches a GPIO interrupt on `gdo0_pin`.
2. When the CC1101 signals a received packet (GDO0 interrupt), `Elero::interrupt()` sets `received_ = true`.
3. `Elero::loop()` detects `received_`, reads the FIFO, decodes and decrypts the packet, then:
   - If it is a **status packet**, dispatches the state to the matching `EleroBlindBase` / `EleroLightBase` via `set_rx_state()`.
   - If it is a **command packet** from a physical remote, calls `schedule_immediate_poll()` on the targeted device so the hub can observe the new state quickly.
4. Transmissions use a **non-blocking TX state machine** (`TxPhase` enum: `IDLE → WAIT_CC_IDLE → WAIT_CC_TX → WAIT_DONE`). `Elero::advance_tx_()` is called from `loop()` each tick and polls CC1101 state without spin-blocking the main loop.
5. `EleroCover::loop()` handles polling timers, position recomputation, and drains the command queue by calling `parent_->send_command()`.
6. `EleroLight::loop()` handles dimming timers and drains its command queue similarly.
7. Blind states are periodically persisted to LittleFS (every `STATE_SAVE_INTERVAL_MS` = 5 min) and restored on boot.

---

## Key Classes and Files

### `components/elero/elero.h` / `elero.cpp`

**Class:** `Elero : public spi::SPIDevice<...>, public Component`
**Namespace:** `esphome::elero`

Critical public API:
- `register_cover(EleroBlindBase*)` — called by each `EleroCover` at setup
- `register_light(EleroLightBase*)` — called by each `EleroLight` at setup
- `send_command(t_elero_command*)` — encodes, encrypts, and transmits a command via the non-blocking TX state machine
- `start_scan()` / `stop_scan()` / `is_scanning()` — toggle RF discovery mode
- `get_discovered_blinds()` / `get_discovered_count()` / `clear_discovered()` — discovered device info
- `adopt_blind(const DiscoveredBlind&, const std::string& name)` — promote a discovered blind into a runtime-managed blind (persisted to flash)
- `remove_runtime_blind(uint32_t addr)` — drop a runtime-adopted blind (persisted to flash)
- `send_runtime_command(uint32_t addr, uint8_t cmd)` — send a command to a runtime blind
- `update_runtime_blind_settings(uint32_t addr, uint32_t open_dur_ms, uint32_t close_dur_ms, uint32_t poll_intvl_ms)` — update timing/poll settings for a runtime blind (persisted to flash)
- `register_rssi_sensor(uint32_t addr, sensor::Sensor*)` — link RSSI sensor to a blind address
- `register_text_sensor(uint32_t addr, text_sensor::TextSensor*)` — link text sensor to a blind address
- `start_packet_dump()` / `stop_packet_dump()` — toggle raw RF packet capture (ring buffer)
- `save_runtime_blinds()` / `save_packet_log()` / `load_packet_log()` — on-demand LittleFS persistence
- `save_blind_states()` / `load_blind_states()` — persist/restore last-known state of every known blind
- `append_log(level, tag, msg)` / `get_log_entries()` / `set_log_capture(bool)` — web UI in-memory log helpers
- `interrupt(Elero *arg)` — static ISR, sets `received_` flag

Key constants (defined in `elero.h`):

| Constant | Value | Purpose |
|---|---|---|
| `ELERO_MAX_PACKET_SIZE` | 57 | Maximum RF packet length (FCC spec) |
| `ELERO_POLL_INTERVAL_MOVING` | 2000 ms | Status poll while blind is moving |
| `ELERO_TIMEOUT_MOVEMENT` | 120 000 ms | Give up movement tracking after 2 min |
| `ELERO_POST_MOVEMENT_POLL_DELAY` | 5000 ms | Extra poll 5 s after travel time elapses |
| `ELERO_SEND_RETRIES` | 3 | Command retry count |
| `ELERO_SEND_PACKETS` | 2 | Packets sent per command |
| `ELERO_DELAY_SEND_PACKETS` | 50 ms | Delay between packet repeats |
| `ELERO_MAX_DISCOVERED` | 20 | Max blinds tracked in scan mode |
| `ELERO_MAX_RAW_PACKETS` | 50 | Max entries in the packet dump ring buffer |
| `ELERO_MAX_DESTINATIONS` | 20 | Max destination addresses per RF packet |
| `ELERO_MAX_COMMAND_QUEUE` | 10 | Max queued commands per blind (OOM guard) |
| `ELERO_MSG_LENGTH` | 0x1d | Fixed TX message length |
| `ELERO_CRYPTO_MULT` | 0x708f | Encryption multiplier |
| `ELERO_CRYPTO_MASK` | 0xffff | 16-bit encryption mask |

State constants (`ELERO_STATE_*`):

| Constant | Value | Meaning |
|---|---|---|
| `ELERO_STATE_UNKNOWN` | 0x00 | Unknown |
| `ELERO_STATE_TOP` | 0x01 | Fully open |
| `ELERO_STATE_BOTTOM` | 0x02 | Fully closed |
| `ELERO_STATE_INTERMEDIATE` | 0x03 | Partial position |
| `ELERO_STATE_TILT` | 0x04 | Tilted |
| `ELERO_STATE_BLOCKING` | 0x05 | Blocked (error) |
| `ELERO_STATE_OVERHEATED` | 0x06 | Overheated (error) |
| `ELERO_STATE_TIMEOUT` | 0x07 | Timeout (error) |
| `ELERO_STATE_START_MOVING_UP` | 0x08 | Starting to move up |
| `ELERO_STATE_START_MOVING_DOWN` | 0x09 | Starting to move down |
| `ELERO_STATE_MOVING_UP` | 0x0a | Moving up |
| `ELERO_STATE_MOVING_DOWN` | 0x0b | Moving down |
| `ELERO_STATE_STOPPED` | 0x0d | Stopped at intermediate position |
| `ELERO_STATE_TOP_TILT` | 0x0e | Open and tilted |
| `ELERO_STATE_BOTTOM_TILT` | 0x0f | Closed and tilted |
| `ELERO_STATE_OFF` | 0x0f | Light off — alias for `ELERO_STATE_BOTTOM_TILT` (protocol reuses same byte) |
| `ELERO_STATE_ON` | 0x10 | Light on |

**`TxPhase`** — non-blocking TX state machine (replaces blocking spin-loops):

```cpp
enum class TxPhase : uint8_t {
  IDLE,         // No TX in progress; ready to accept new commands
  WAIT_CC_IDLE, // SIDLE sent; polling MARCSTATE until CC1101 reaches IDLE
  WAIT_CC_TX,   // STX sent; polling MARCSTATE until CC1101 enters TX
  WAIT_DONE,    // TX active; waiting for GDO0 TX-end signal (received_ flag)
};
```

Key internal data structures:

**`t_elero_command`** — 29-byte command packet to be encrypted and transmitted:
```cpp
struct t_elero_command {
  uint8_t  counter;          // Message counter (1–255, increments per command)
  uint32_t blind_addr;       // Target blind RF address
  uint32_t remote_addr;      // Simulated remote address
  uint8_t  channel;
  uint8_t  pck_inf[2];       // Packet info bytes (typ, typ2) — typically 0x6a, 0x00
  uint8_t  hop;              // Hop count — typically 0x0a
  uint8_t  payload[10];      // Command payload (payload[4] = command byte)
};
```

**`DiscoveredBlind`** — blind found during RF scan:
```cpp
struct DiscoveredBlind {
  uint32_t blind_address, remote_address;
  uint8_t  channel, pck_inf[2], hop;
  uint8_t  payload_1, payload_2;
  float    rssi;
  uint32_t last_seen;           // millis() timestamp
  uint8_t  last_state;
  uint16_t times_seen;
  bool     params_from_command; // true = from 6a/69 cmd pkt (preferred), false = from CA/C9 status
};
```

**`RuntimeBlind`** — blind adopted at runtime via web UI (no firmware reflash):
```cpp
struct RuntimeBlind {
  uint32_t blind_address, remote_address;
  uint8_t  channel, pck_inf[2], hop;
  uint8_t  payload_1, payload_2;
  std::string name;
  uint32_t open_duration_ms, close_duration_ms, poll_intvl_ms;
  uint32_t last_seen_ms;
  float    last_rssi;
  uint8_t  last_state;
  uint8_t  cmd_counter;
  std::queue<uint8_t> command_queue;
};
```

**`RawPacket`** — single captured RF packet (packet dump mode, ring buffer of `ELERO_MAX_RAW_PACKETS` = 50):
```cpp
struct RawPacket {
  uint32_t timestamp_ms;            // millis() when captured
  uint8_t  fifo_len;                // bytes actually read from CC1101 FIFO
  uint8_t  data[CC1101_FIFO_LENGTH]; // raw CC1101 FIFO bytes
  bool     valid;                   // true = passed validation and decoded
  char     reject_reason[32];       // empty when valid
};
```

**`LogEntry`** — in-memory captured log message (circular buffer, 200 entries, for the web UI `/elero/api/logs` endpoint):
```cpp
struct LogEntry {       // defined inside class Elero
  uint32_t timestamp_ms;
  uint8_t  level;
  char     tag[24];
  char     message[160];
};
```

> **Note:** `LogEntry` is the **in-memory** log buffer (volatile, lost on reboot). The **persistent** log uses `PersistentLogEntry` (see `elero_log.h` below). Both are exposed through `/elero/api/logs`, with the persistent variant taking precedence when `logging: enable: true` is set.

### `components/elero/cover/EleroCover.h` / `EleroCover.cpp`

**Class:** `EleroCover : public cover::Cover, public Component, public EleroBlindBase`

Key behaviors:
- Maintains an internal `std::queue<uint8_t> commands_to_send_` (capped at `ELERO_MAX_COMMAND_QUEUE`) for reliable delivery
- Polls blind status at a configurable interval (`poll_intvl_`, default 5 min); while moving, polls every `ELERO_POLL_INTERVAL_MOVING` (2 s)
- **Post-movement poll**: 5 s after `open_duration_` / `close_duration_` elapses, sends an extra status check to confirm final state
- **Immediate poll on remote**: when the hub detects a physical remote command for this blind, `schedule_immediate_poll()` queues a CHECK command within ~50 ms
- Tracks cover `position` (0.0–1.0) by dead-reckoning against `open_duration_` / `close_duration_` timestamps
- Supports tilt as a separate operation via `command_tilt_`
- Staggered poll offsets (`poll_offset_`, auto-incremented by 5 s per cover) prevent all covers from polling simultaneously
- Auto-generates RSSI and status text sensors unless `auto_sensors: false` is set

### `components/elero/light/EleroLight.h` / `EleroLight.cpp`

**Class:** `EleroLight : public light::LightOutput, public Component, public EleroLightBase`

Key behaviors:
- Implements on/off and optional brightness control for Elero wireless lights (dimmers)
- `dim_duration: 0s` → `ON_OFF` color mode only (no brightness slider)
- `dim_duration: Xs` → `BRIGHTNESS` color mode; X is the time to ramp from 0 % to 100 % using timed `dim_up` / `dim_down` + `stop` commands
- Uses an `ignore_write_state_` flag to break feedback loops when `set_rx_state()` updates the internal state
- Shares the same RF protocol and `t_elero_command` structure as covers
- Polls blind status when `command_check: 0x00` is set (optional; omit for lights that do not respond)

### `components/elero/elero_log.h` / `elero_log.cpp`

**Class:** `EleroEventLog`
**Namespace:** `esphome::elero`

Key behaviors:
- LittleFS-based persistent ring buffer storing 64-byte fixed-size `PersistentLogEntry` records
- Logs RF received packets, cover state transitions, commands sent, and system events
- Survives reboots — entries persist on flash at `/littlefs/elero_events.bin`
- Header flush batching (every 10 appends via `HEADER_FLUSH_INTERVAL`) reduces flash wear
- Enabled via `logging: enable: true` in the hub config; disabled by default
- File layout: 64-byte `PersistentLogHeader` + N × 64-byte `PersistentLogEntry` records

**`EleroEventType`** — event type codes stored in each log entry:
```cpp
enum EleroEventType : uint8_t {
  EVENT_RF_RECEIVED  = 0x01,  // RF packet decoded from a blind
  EVENT_STATE_CHANGE = 0x02,  // Cover state transition (old -> new)
  EVENT_COMMAND_SENT = 0x03,  // Command transmitted to a blind
  EVENT_SYSTEM       = 0x04,  // System event (boot, error, scan)
};
```

**`PersistentLogEntry`** — 64-byte packed struct (compile-time size assertion enforced):
```cpp
struct __attribute__((packed)) PersistentLogEntry {
  uint32_t sequence;        // Monotonic sequence number
  uint32_t timestamp_ms;    // millis() at event time
  uint8_t  event_type;      // EleroEventType
  uint32_t blind_address;   // 3-byte blind address (0 for system events)
  uint8_t  data1;           // old_state / state_byte / cmd_byte
  uint8_t  data2;           // new_state / 0 / 0
  int8_t   rssi;            // RSSI in dBm (0 if N/A)
  uint8_t  operation;       // Cover operation (0=idle,1=opening,2=closing)
  uint16_t position_x100;   // Position * 100 (0-10000); 0xFFFF if unknown
  char     message[45];     // Human-readable summary, null-terminated
};  // total: 64 bytes
```

**`PersistentLogHeader`** — 64-byte file header at offset 0:
```cpp
struct __attribute__((packed)) PersistentLogHeader {
  uint32_t magic;          // 0x454C4F47 ("ELOG")
  uint16_t version;        // Format version (1)
  uint16_t max_entries;    // Max entries in ring buffer
  uint32_t write_idx;      // Next write index (ring buffer slot)
  uint32_t total_written;  // Monotonic total entries ever written
  uint8_t  reserved[48];   // Future use, zero-filled
};  // total: 64 bytes
```

**Convenience logging methods** on `EleroEventLog`:
```cpp
void log_rf_received(uint32_t blind_addr, uint8_t state_byte, int8_t rssi);
void log_state_change(uint32_t blind_addr, uint8_t old_state, uint8_t new_state,
                      uint8_t operation, float position);
void log_command_sent(uint32_t blind_addr, uint8_t cmd_byte);
void log_system(const char *message);
```

### `components/elero/elero_storage.h` / `elero_storage.cpp`

**Class:** `EleroStorage`
**Namespace:** `esphome::elero`

Key behaviors:
- Mounts LittleFS partition once during `Elero::setup()` via `begin()`
- Persists runtime-adopted blinds to flash so they survive reboots (saved on adopt/remove/settings change)
- Optionally persists the RF packet dump ring buffer on demand (not every packet, to reduce flash wear)
- Persists last-known blind states periodically and on significant state changes

**`PersistedBlindState`** — compact state snapshot saved per blind address:
```cpp
struct PersistedBlindState {
  uint32_t blind_address;
  uint8_t  last_state;
  float    last_rssi;
  uint32_t timestamp_ms;
};
```

**`RuntimeBlindRecord`** — fixed-size serialisation of `RuntimeBlind` for LittleFS:
```cpp
struct RuntimeBlindRecord {
  uint32_t blind_address, remote_address;
  uint8_t  channel, pck_inf[2], hop;
  uint8_t  payload_1, payload_2;
  char     name[32];
  uint32_t open_duration_ms, close_duration_ms, poll_intvl_ms;
  uint8_t  last_state, cmd_counter;
};
```

**Storage magic numbers** (detect corrupt/wrong files):

| Constant | Value | File |
|---|---|---|
| `STORAGE_MAGIC_BLINDS` | `0x454C4252` ("ELBR") | Runtime blinds |
| `STORAGE_MAGIC_PACKETS` | `0x454C504B` ("ELPK") | RF packet dump |
| `STORAGE_MAGIC_STATES` | `0x454C5354` ("ELST") | Blind state snapshots |

### `components/elero_web/elero_web_server.h` / `elero_web_server.cpp`

**Class:** `EleroWebServer : public Component`
**Optional sub-platform:** `EleroWebSwitch : public switch::Switch, public Component`

Key behaviors:
- Hosts the web UI at `http://<device-ip>/elero`
- Exposes REST API for RF scanning, blind discovery, control, and runtime diagnostics
- `EleroWebSwitch` allows runtime enable/disable of all `/elero` endpoints (returns 503 when disabled)
- All endpoints include `Access-Control-Allow-Origin: *` CORS headers
- User-controlled strings (blind names) are JSON-escaped before embedding in responses

### REST API Endpoints

All endpoints are served at `http://<device-ip>/elero`. A 503 response is returned if the optional `elero_web` switch is disabled.

**Core endpoints:**

| Endpoint | Method | Description |
|---|---|---|
| `/` | GET | Redirect to `/elero` |
| `/elero` | GET | HTML web UI |
| `/elero/api/scan/start` | POST | Start RF discovery scan |
| `/elero/api/scan/stop` | POST | Stop RF discovery scan |
| `/elero/api/discovered` | GET | JSON array of discovered blinds |
| `/elero/api/configured` | GET | JSON array of configured covers with current state |
| `/elero/api/yaml` | GET | YAML snippet ready to paste into ESPHome config |
| `/elero/api/info` | GET | Device info (version, discovery count, etc.) |
| `/elero/api/runtime` | GET | Runtime status (scan active, blinds count, etc.) |

**Cover/Light control (requires address):**

| Endpoint | Method | Description |
|---|---|---|
| `/elero/api/covers/0xADDRESS/command` | POST | Send command to cover/light (body: `{"cmd": "up"\|"down"\|"stop"\|"tilt"}`) |
| `/elero/api/covers/0xADDRESS/settings` | POST | Update cover settings at runtime (body: JSON with timing/poll settings) |
| `/elero/api/discovered/0xADDRESS/adopt` | POST | Adopt a discovered blind into runtime-managed blinds |

**Diagnostics:**

| Endpoint | Method | Description |
|---|---|---|
| `/elero/api/frequency` | GET | Current CC1101 frequency settings |
| `/elero/api/frequency/set` | POST | Update CC1101 frequency at runtime (body: `{"freq0": 0x7a, "freq1": 0x71, "freq2": 0x21}`) |
| `/elero/api/logs` | GET | Recent log entries (supports `since` query parameter) |
| `/elero/api/logs/clear` | POST | Clear captured logs |
| `/elero/api/logs/capture/start` | POST | Start capturing logs |
| `/elero/api/logs/capture/stop` | POST | Stop capturing logs |
| `/elero/api/logs/status` | GET | Log status (persistent mode, entry count, max entries) |
| `/elero/api/dump/start` | POST | Start RF packet dump |
| `/elero/api/dump/stop` | POST | Stop RF packet dump |
| `/elero/api/packets` | GET | Recent captured RF packets |
| `/elero/api/packets/clear` | POST | Clear captured packets |

**Web UI state (elero_web switch sub-platform):**

| Endpoint | Method | Description |
|---|---|---|
| `/elero/api/ui/status` | GET | Get web UI enabled/disabled state |
| `/elero/api/ui/enable` | POST | Enable/disable web UI (body: `{"enabled": true\|false}`) |

**HTTP Error Codes:**

| Code | Meaning |
|---|---|
| 200 | Success |
| 400 | Bad Request — missing or invalid parameters (e.g., missing `cmd`, invalid `since` value) |
| 404 | Not Found — blind address not found (neither configured nor adopted) |
| 409 | Conflict — e.g., starting scan when already running |
| 500 | Internal Server Error — event log not ready |
| 503 | Service Unavailable — web UI disabled via the `elero_web` switch |

Error responses are returned as JSON: `{"error": "description"}`

**Parameter format for POST endpoints:**

POST endpoints accept parameters as **URL query parameters** (not JSON body). Example:
```
POST /elero/api/covers/0xa831e5/command?cmd=up
POST /elero/api/covers/0xa831e5/settings?open_duration=25000&close_duration=22000&poll_interval=300000
POST /elero/api/frequency/set?freq2=0x21&freq1=0x71&freq0=0x7a
POST /elero/api/ui/enable?enabled=true
```

---

## RF Protocol Notes

### Command byte locations

Commands are placed at `payload[4]` inside `t_elero_command` (index 4 of the 10-byte payload). Default command bytes:

| Command | Byte |
|---|---|
| UP | `0x20` |
| DOWN | `0x40` |
| STOP | `0x10` |
| CHECK (poll) | `0x00` |
| TILT | `0x24` |

### Encryption

- Encryption is counter-based: `code = 0x00 - (counter * 0x708f) & 0xffff`
- The hub recognises both **command packets** (type `0x6a`/`0x69`) from physical remotes and **status packets** (type `0xCA`/`0xC9`) from blinds
- Discovery preferentially uses parameters extracted from command packets over status packets (`params_from_command` flag)

---

## Naming Conventions

| Item | Convention | Example |
|---|---|---|
| C++ classes | PascalCase | `EleroCover`, `EleroWebServer` |
| C++ namespaces | lowercase | `esphome::elero` |
| C++ constants | `UPPER_SNAKE_CASE` with `ELERO_` prefix | `ELERO_COMMAND_COVER_UP` |
| C++ private members | trailing underscore | `gdo0_pin_`, `scan_mode_` |
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

The `CONF_ELERO_ID` pattern is used throughout to resolve the parent hub:
```python
cv.GenerateID(CONF_ELERO_ID): cv.use_id(elero),
```
```python
parent = await cg.get_variable(config[CONF_ELERO_ID])
cg.add(var.set_elero_parent(parent))
```

The cover `__init__.py` validates that `open_duration` and `close_duration` must both be set or both be `0s` — it is invalid to set only one.

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
  logging:               # Optional persistent event log
    enable: true         # default: true when section present
    max_entries: 1000    # default: 1000, range 100-5000
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
- `channel` — RF channel number of the blind
- `remote_address` — 3-byte hex address of the remote control paired with the blind

Optional parameters (with defaults):

| Parameter | Default | Notes |
|---|---|---|
| `poll_interval` | `5min` | Use `never` to disable polling |
| `open_duration` | `0s` | Must set both or neither |
| `close_duration` | `0s` | Must set both or neither |
| `supports_tilt` | `false` | |
| `auto_sensors` | `true` | Auto-generate RSSI + status sensors |
| `payload_1` | `0x00` | |
| `payload_2` | `0x04` | |
| `pck_inf1` | `0x6a` | |
| `pck_inf2` | `0x00` | |
| `hop` | `0x0a` | |
| `command_up` | `0x20` | Override RF command bytes |
| `command_down` | `0x40` | |
| `command_stop` | `0x10` | |
| `command_check` | `0x00` | |
| `command_tilt` | `0x24` | |

### Light (`light: platform: elero`)

Required parameters:
- `blind_address` — 3-byte hex address of the light receiver (e.g., `0xc41a2b`)
- `channel` — RF channel number of the light
- `remote_address` — 3-byte hex address of the remote control paired with the light

Optional parameters (with defaults):

| Parameter | Default | Notes |
|---|---|---|
| `dim_duration` | `0s` | `0s` = on/off only; `>0s` = brightness control (ramp time 0→100%) |
| `payload_1` | `0x00` | |
| `payload_2` | `0x04` | |
| `pck_inf1` | `0x6a` | |
| `pck_inf2` | `0x00` | |
| `hop` | `0x0a` | |
| `command_on` | `0x20` | |
| `command_off` | `0x40` | |
| `command_dim_up` | `0x20` | |
| `command_dim_down` | `0x40` | |
| `command_stop` | `0x10` | |
| `command_check` | `0x00` | |

### Sensors

```yaml
sensor:
  - platform: elero
    blind_address: 0xa831e5   # Required: which blind
    name: "Blind RSSI"        # Unit: dBm

text_sensor:
  - platform: elero
    blind_address: 0xa831e5
    name: "Blind Status"      # Values: see ELERO_STATE_* constants
```

### Buttons (RF scan)

```yaml
button:
  - platform: elero
    name: "Start Scan"
    scan_start: true
  - platform: elero
    name: "Stop Scan"
    scan_start: false
```

Buttons also support a `light_id` parameter to send a direct command byte (default `0x44`) to a specific light entity instead of operating the scan.

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

### Finding blind addresses

The typical workflow for a new installation:

1. Add scan buttons and the web UI to `example.yaml`
2. Flash the device
3. Open `http://<device-ip>/elero` and press "Start Scan"
4. Operate each blind with its original remote
5. Discovered blinds appear in the web UI with addresses pre-filled
6. Download the generated YAML snippet and add it to your config

Alternatively, blinds can be **adopted at runtime** via the web UI (`/elero/api/discovered/0xADDRESS/adopt`) without reflashing — they are stored as `RuntimeBlind` entries in the hub.

---

## Testing

There are no automated tests in this repository. Validation is done manually on real hardware:

1. Flash the firmware and verify the CC1101 initialises (check `esphome logs` for `[I][elero:...]` messages)
2. Use the RF scan to confirm blind discovery
3. Test each cover entity (open, close, stop) from Home Assistant
4. Verify RSSI and status text sensors update correctly
5. For lights: verify on/off, and brightness if `dim_duration > 0s`
6. For diagnostics: use `/elero/api/dump/start` + `/elero/api/packets` to verify raw RF reception

---

## Common Pitfalls

- **Wrong frequency**: Most European Elero motors use 868.35 MHz (`freq0=0x7a`). Some use 868.95 MHz (`freq0=0xc0`). If discovery finds nothing, try the alternate frequency. Frequency can also be changed at runtime via `/elero/api/frequency/set` without reflashing.
- **SPI conflicts**: The CC1101 CS pin must not be shared with any other SPI device.
- **Using `web_server:` instead of `web_server_base:`**: Adding `web_server:` to your YAML re-enables the default ESPHome entity UI at `/`. Use `web_server_base:` (or rely on its auto-load via `elero_web`) to serve only the Elero UI at `/elero`. Navigating to `/` will redirect automatically to `/elero`.
- **Position tracking**: Leave `open_duration` and `close_duration` at `0s` if you only need open/close without position — setting incorrect durations causes wrong position estimates. Both must be set together (one without the other is a schema validation error).
- **Poll interval `never`**: Set `poll_interval: never` for blinds that reliably push state updates (avoids unnecessary RF traffic). Internally this maps to `uint32_t` max (4 294 967 295 ms).
- **Light dimming with `dim_duration: 0s`**: Results in `ON_OFF` color mode — no brightness slider is exposed to Home Assistant. Set a non-zero value only for dimmable receivers.
- **Command queue cap**: Each cover/light allows at most `ELERO_MAX_COMMAND_QUEUE` (10) queued commands to guard against OOM. Commands sent when the queue is full are silently dropped — avoid flooding the queue.
- **EleroLight feedback loop**: The `ignore_write_state_` flag prevents the light from re-sending a command when its own `set_rx_state()` updates the light's internal state. Do not remove this guard.

---

## Contributing

- Follow the existing naming conventions for C++ and Python code.
- Keep the `EleroBlindBase` and `EleroLightBase` interfaces minimal — the hub should not depend on cover or light internals.
- Test changes on real hardware before opening a pull request.
- Document new configuration parameters in both `README.md` and `docs/CONFIGURATION.md`.
- The primary development branch convention used by automation is `claude/<session-id>`.
- There is no dedicated `stable` branch — `master` is the stable reference branch.
