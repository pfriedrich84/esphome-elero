# Test Coverage Analysis

**Date:** 2026-03-12
**Status:** No automated tests exist in this repository.

---

## Current State

The codebase contains ~6,000 lines of production code across Python (config validation/codegen) and C++ (firmware logic). **There are zero automated tests.** Validation is performed manually on real hardware per the workflow described in `CLAUDE.md`.

There is no CI/CD pipeline, no test framework configured, and no test infrastructure of any kind.

---

## Recommended Test Areas (by priority)

### 1. CRITICAL — Encryption / Decryption Round-Trip (C++)

**Files:** `components/elero/elero.cpp` (lines 627–766)

The RF protocol relies on a multi-stage encryption pipeline:

```
Encode: calc_parity → add_r20_to_nibbles → xor_2byte_encode → encode_nibbles
Decode: decode_nibbles → xor_2byte_decode → sub_r20_from_nibbles
```

Each function is pure (no I/O, no hardware dependency) and can be tested in isolation:

| Function | What to test |
|---|---|
| `encode_nibbles()` / `decode_nibbles()` | Round-trip: encode then decode returns original data |
| `calc_parity()` | Known parity values from captured RF packets |
| `add_r20_to_nibbles()` / `sub_r20_from_nibbles()` | Round-trip with various `r20` seeds |
| `xor_2byte_in_array_encode()` / `..._decode()` | Round-trip with various XOR keys |
| `msg_encode()` / `msg_decode()` | Full pipeline round-trip; known-plaintext vectors from real Elero traffic |

**Why critical:** A subtle encryption bug would silently break all RF communication. These are pure functions that can be tested without any hardware mocking — they are the single highest-value test target.

**Suggested approach:** Extract encryption functions into a testable module (or use `#include` directly in a Google Test / Catch2 harness). Use captured RF packets as known-good test vectors.

---

### 2. CRITICAL — Packet Parsing & Bounds Checking (C++)

**File:** `components/elero/elero.cpp`, `interpret_msg()` (lines 768–970)

The packet parser performs 50+ lines of bounds validation before processing any data:

- Rejects packets longer than `ELERO_MAX_PACKET_SIZE` (57 bytes)
- Validates destination count against `MAX_SAFE_DESTS` (10)
- Prevents out-of-bounds decryption access: `(26 + dests_len) > length`
- Prevents buffer overflow: `(26 + dests_len) >= CC1101_FIFO_LENGTH`
- Handles two's complement RSSI conversion

**What to test:**

| Scenario | Expected behavior |
|---|---|
| Valid packet with known structure | Successfully parsed, state dispatched |
| Packet with `length > 57` | Rejected, logged |
| Packet with `length < 17` (minimum) | Rejected, logged |
| `num_dests` causing index overflow | Rejected before out-of-bounds access |
| Destination length exceeding packet length | Rejected |
| RSSI raw value `0x00`, `0x7F`, `0x80`, `0xFF` | Correct dBm conversion |
| All-zero packet | Graceful rejection |
| Maximum-length valid packet (57 bytes) | Parsed correctly |

**Why critical:** Malformed RF packets from interference or attackers could trigger buffer overflows or undefined behavior. This is an external input boundary.

**Suggested approach:** Mock the SPI read to inject crafted byte arrays, or refactor `interpret_msg()` to accept a buffer parameter for direct testing.

---

### 3. HIGH — TX State Machine (C++)

**File:** `components/elero/elero.cpp`, `advance_tx()` (lines 255–328)

The non-blocking TX state machine progresses one step per `loop()`:

```
IDLE → GOING_IDLE → LOADING → FIRING → TRANSMITTING → VERIFYING → COOLDOWN → IDLE
```

**What to test:**

| Scenario | Expected behavior |
|---|---|
| Normal TX cycle (happy path) | Each state transitions correctly |
| TX state timeout (stuck >50ms in any state) | Recovery to IDLE with error log |
| FIFO underflow during TRANSMITTING | Detected and recovered |
| Post-TX RX FIFO overflow | Flushed before resuming RX |
| `is_tx_idle()` returns correct value per state | `true` only in IDLE |
| Concurrent `send_command()` while TX active | Returns `false`, does not corrupt state |

**Why high priority:** A stuck TX state machine silently disables all RF communication. The timeout and recovery paths are the most likely to contain untested edge cases.

---

### 4. HIGH — Cover Position Tracking / Dead-Reckoning (C++)

**File:** `components/elero/cover/EleroCover.cpp` (lines 416–444)

Position is tracked by linear interpolation over time:

```cpp
position += direction * (now - last_recompute_time) / action_duration;
position = clamp(position, 0.0f, 1.0f);
```

**What to test:**

| Scenario | Expected behavior |
|---|---|
| Full open (position 0.0 → 1.0) over `open_duration` | Position reaches 1.0 |
| Full close (position 1.0 → 0.0) over `close_duration` | Position reaches 0.0 |
| Partial move (stop at 50%) | Position approximately 0.5 |
| Direction reversal mid-move | Position tracks correctly |
| Position clamping at boundaries | Never exceeds [0.0, 1.0] |
| `is_at_target()` with TX latency compensation | Stops 150ms early (0.75% of 20s) |
| Very short duration (<1s) | Latency margin doesn't overshoot |
| Very long duration (>60s) | Position increments remain accurate |

**Why high priority:** Incorrect position tracking directly affects user experience — blinds stopping at the wrong position, or Home Assistant showing wrong state.

---

### 5. HIGH — Radio Health Watchdog (C++)

**File:** `components/elero/elero.cpp`, `check_radio_state_()` (lines 333–369)

**What to test:**

| CC1101 MARCSTATE | Expected recovery |
|---|---|
| `RXFIFO_OVERFLOW` (0x11) | Flush RX FIFO, restart RX |
| Stuck in `IDLE` (0x01) | Restart RX |
| Unexpected state (e.g., `CALIBRATE`) | Full radio reinit |
| Normal `RX` state | No action taken |
| Called during active TX | Skipped entirely |

---

### 6. MEDIUM — Python Config Validation

**Files:** `components/elero/cover/__init__.py`, `components/elero/light/__init__.py`

These validators are pure Python functions that can be tested with pytest:

| Function | What to test |
|---|---|
| `poll_interval("never")` | Returns `4294967295` |
| `poll_interval("5min")` | Returns `300000` |
| `_validate_duration_consistency()` | Both zero = OK, both nonzero = OK, mixed = `cv.Invalid` |
| `_auto_sensor_validator()` | Injects sensors when `auto_sensors: true`, skips when `false` |
| `_final_validate()` | Detects duplicate `blind_address` across covers |
| Light schema | Validates hex ranges for addresses (0–0xFFFFFF) and command bytes (0–0xFF) |

**Suggested approach:** Create `tests/test_validation.py` with pytest. Mock ESPHome's `config_validation` module where needed.

---

### 7. MEDIUM — Command Queue Management (C++)

**File:** `components/elero/cover/EleroCover.cpp` (lines 145–173)

| Scenario | Expected behavior |
|---|---|
| Queue empty, send command | Command sent immediately |
| Queue at capacity (10) | New command rejected, warning logged |
| Retry exhausted (3 attempts) | Command dropped, counter still incremented |
| Counter wraparound (`0xFF` → `0x01`, skipping `0x00`) | Correct behavior |
| Multiple commands queued | FIFO ordering preserved |

---

### 8. MEDIUM — Light Brightness Dead-Reckoning (C++)

**File:** `components/elero/light/EleroLight.cpp`

Similar to cover position tracking but for brightness:

| Scenario | Expected behavior |
|---|---|
| Dim up from 0% to 100% over `dim_duration` | Brightness reaches 1.0 |
| Dim down from 100% to 0% | Brightness reaches 0.0 |
| `dim_duration = 0` (on/off only) | No brightness interpolation |
| `ignore_write_state_` flag during `set_rx_state()` | No feedback loop |

---

### 9. LOW — Web API Endpoints

**File:** `components/elero_web/elero_web_server.cpp`

| What to test | How |
|---|---|
| `parse_addr_url()` extracts hex address correctly | Unit test with various URL patterns |
| JSON response structure for `/api/configured` | Validate schema |
| 503 response when web switch is OFF | Integration test |
| CORS headers present on all responses | Validate `Access-Control-*` headers |

---

### 10. LOW — Frontend (JavaScript)

**File:** `components/elero_web/frontend/src/main.js`

Currently no frontend test framework. If added, priorities would be:
- API call error handling
- Polling state management
- YAML generation from discovered blinds

---

## Recommended Testing Infrastructure

### Phase 1: Python Tests (Lowest effort, immediate value)

```
tests/
├── conftest.py           # ESPHome mock fixtures
├── test_cover_validation.py
├── test_light_validation.py
└── test_hub_validation.py
```

- Framework: **pytest**
- Requires: Mocking `esphome.config_validation` and `esphome.codegen`
- CI: GitHub Actions workflow running `pytest tests/`

### Phase 2: C++ Unit Tests (Medium effort, highest value)

```
tests/
├── cpp/
│   ├── CMakeLists.txt       # or Makefile
│   ├── test_encryption.cpp
│   ├── test_packet_parsing.cpp
│   ├── test_position_tracking.cpp
│   └── mocks/
│       ├── esphome_mocks.h  # Stub ESPHome Component, SPIDevice, etc.
│       └── cc1101_mocks.h   # Stub SPI register reads/writes
```

- Framework: **Catch2** (header-only, no build system dependency) or **Google Test**
- Key challenge: Mocking ESPHome's `Component` lifecycle and SPI bus
- Approach: Extract pure logic into standalone functions/headers that don't depend on ESPHome runtime

### Phase 3: CI Pipeline

```yaml
# .github/workflows/test.yml
name: Tests
on: [push, pull_request]
jobs:
  python-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: pip install pytest esphome
      - run: pytest tests/

  cpp-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: apt-get install -y cmake
      - run: cd tests/cpp && cmake . && make && ctest

  esphome-compile:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: pip install esphome
      - run: esphome config example.yaml
      - run: esphome compile example.yaml
```

---

## Summary

| Priority | Area | Type | Effort | Value |
|---|---|---|---|---|
| CRITICAL | Encryption round-trip | C++ unit | Medium | Very High |
| CRITICAL | Packet bounds checking | C++ unit | Medium | Very High |
| HIGH | TX state machine | C++ unit | High | High |
| HIGH | Position tracking | C++ unit | Low | High |
| HIGH | Radio watchdog | C++ unit | Medium | High |
| MEDIUM | Python validators | Python unit | Low | Medium |
| MEDIUM | Command queue | C++ unit | Low | Medium |
| MEDIUM | Light brightness | C++ unit | Low | Medium |
| LOW | Web API | Integration | Medium | Low |
| LOW | Frontend JS | JS unit | Medium | Low |

**Recommended starting point:** Encryption round-trip tests using Catch2, followed by Python validation tests using pytest. These two targets provide the highest coverage-per-effort ratio and catch the most dangerous bug categories (silent data corruption and invalid config acceptance).
