# Architect Review: Post-Implementation Findings

## Summary

After a holistic review of all implemented changes (L1-L4, R1-R4, H1-H9) and their interactions with the existing codebase, this document identifies new issues and improvement opportunities. The focus is on systemic patterns, edge cases introduced by combined changes, resource concerns on ESP32, and architectural inconsistencies that were not part of the original 10-issue improvement plan.

---

## New Issues

### Issue A1: Duplicated Command-Dispatch Logic Between EleroCover and EleroLight

**Priority:** P2 | **Effort:** M | **Labels:** `code-quality`, `maintainability`

**Problem:** `EleroCover::handle_commands()` (EleroCover.cpp:151-185) and `EleroLight::handle_commands()` (EleroLight.cpp:166-201) are nearly identical: both check `is_tx_idle()`, gate on `send_delay`, manage `send_packets_`/`send_retries_`, pop from a `std::queue<uint8_t>`, increment the counter, and auto-reset `queue_full_published_`. The only meaningful difference is the log tag string. This represents ~70 lines of duplicated logic that must be kept in sync manually.

**Betroffene Dateien:**
- `components/elero/cover/EleroCover.cpp:151-185`
- `components/elero/light/EleroLight.cpp:166-201`

**Fix:** Extract command dispatch into a shared helper, either as a non-virtual method on a common base class or as a standalone utility function that takes the command queue, parent pointer, and command struct by reference. The `queue_full_published_` reset and counter increment logic would be captured in the same helper.

---

### Issue A2: EleroLight Missing millis() Wraparound Protection in recompute_brightness()

**Priority:** P2 | **Effort:** S | **Labels:** `robustness`

**Problem:** Issue H6 added millis() wraparound protection to `EleroCover::recompute_position()` (elapsed > ELERO_TIMEOUT_MOVEMENT check), but the equivalent `EleroLight::recompute_brightness()` (EleroLight.cpp:210-218) has no such guard. The calculation `(now - this->last_recompute_time_)` can produce an implausibly large elapsed value if `last_recompute_time_` is stale or a wraparound occurs, causing an instantaneous brightness jump to 0% or 100%.

**Betroffene Dateien:**
- `components/elero/light/EleroLight.cpp:210-218`

**Fix:** Add the same elapsed-time sanity check as in `EleroCover::recompute_position()`:
```cpp
if (elapsed > some_reasonable_max) {
    this->last_recompute_time_ = now;
    return;
}
```
Use `dim_duration_ * 2` or a fixed constant (e.g., 120000 ms) as the threshold.

---

### Issue A3: RuntimeBlind Position Tracking Missing Wraparound Protection

**Priority:** P2 | **Effort:** S | **Labels:** `robustness`

**Problem:** `Elero::recompute_runtime_positions_()` (elero.cpp:464-496) computes `elapsed = now - rb.last_recompute_ms` without any sanity check on the elapsed value, unlike the configured cover's `recompute_position()` which was fixed in H6. If `last_recompute_ms` is stale (e.g., a runtime blind sits idle for 49+ days, then starts moving), the first recompute could produce a massive delta, jumping position to 0.0 or 1.0 instantaneously.

**Betroffene Dateien:**
- `components/elero/elero.cpp:464-496`

**Fix:** Add the same elapsed-time guard used in `EleroCover::recompute_position()`:
```cpp
if (elapsed > ELERO_TIMEOUT_MOVEMENT) {
    rb.last_recompute_ms = now;
    continue;
}
```

---

### Issue A4: Log Buffer Ring-Buffer Read Order is Incorrect

**Priority:** P1 | **Effort:** S | **Labels:** `bug`, `web-ui`

**Problem:** The log buffer in `Elero::append_log()` (elero.cpp:1391-1408) uses a ring buffer with `log_write_idx_` once it reaches `ELERO_LOG_BUFFER_SIZE`. However, `get_log_entries_copy()` simply returns a copy of the vector as-is, and `build_log_entries_array_json_()` iterates it linearly. After the ring buffer wraps, entries are out of chronological order: the newest entry is at `log_write_idx_`, and the oldest is at `log_write_idx_ + 1`. The web UI's log display and the `since_ms` filter will show entries in wrong order and may skip recent entries or include old ones.

**Betroffene Dateien:**
- `components/elero/elero.h:369-370` (`get_log_entries_copy`, `clear_log_entries`)
- `components/elero/elero.cpp:1391-1408` (`append_log`)
- `components/elero_web/elero_web_server.cpp:1142-1161` (`build_log_entries_array_json_`)

**Fix:** Either:
a) Return entries in chronological order from `get_log_entries_copy()` by rotating: entries from `[write_idx..end]` followed by `[0..write_idx-1]`, or
b) Change the web server's JSON builder to iterate in ring-buffer order. The `since_ms` filter already helps but doesn't fix the underlying ordering issue.

---

### Issue A5: `log_write_idx_` Type Too Small for `ELERO_LOG_BUFFER_SIZE`

**Priority:** P1 | **Effort:** S | **Labels:** `bug`

**Problem:** `log_write_idx_` is declared as `uint8_t` (elero.h:473) but `ELERO_LOG_BUFFER_SIZE` is 200 (elero.h:361). When the buffer is full, `(log_write_idx_ + 1) % 200` works correctly for values 0-199, which fits in `uint8_t` (max 255). However, if `ELERO_LOG_BUFFER_SIZE` were ever increased beyond 255, this would silently overflow. More critically, the buffer size constant is `static const uint8_t ELERO_LOG_BUFFER_SIZE = 200` which itself cannot exceed 255. These two `uint8_t` constraints are coupled but not documented, creating a maintenance trap.

**Betroffene Dateien:**
- `components/elero/elero.h:361, 473`

**Fix:** Change both `ELERO_LOG_BUFFER_SIZE` and `log_write_idx_` to `uint16_t` for safety, or add a `static_assert(ELERO_LOG_BUFFER_SIZE <= 255)` if keeping `uint8_t`.

---

### Issue A6: Web Server JSON Response Built as std::string Then Copied to Response

**Priority:** P2 | **Effort:** M | **Labels:** `performance`, `memory`

**Problem:** Every web server handler builds a complete JSON response as a `std::string` (often reserving 1-4 KB), then passes it to `request->beginResponse()` which copies the string again into the AsyncWebServer's response buffer. For the combined `/api/status` endpoint — which includes covers, lights, runtime blinds, diagnostics, and optionally logs (200 entries x ~512 bytes each) or packets (50 entries x ~320 bytes each) — this can temporarily allocate 50-100 KB of heap on an ESP32 with only ~160 KB available. The `handle_get_logs()` and `handle_get_status(tab=log)` paths copy the entire log vector (mutex-protected) into a local vector, then serialize it to a string, then copy to the response. That is three copies of the log data.

**Betroffene Dateien:**
- `components/elero_web/elero_web_server.cpp` (all handlers)

**Fix:** Use `AsyncWebServerResponse::beginChunkedResponse()` to stream JSON directly without building the entire string in memory. Alternatively, limit the log/packet endpoints to return at most N entries per request (pagination), or use `beginResponse(200, "application/json", json.c_str())` with `json.shrink_to_fit()` to release excess capacity before the copy.

---

### Issue A7: No Polling/Status-Check for Runtime-Adopted Blinds

**Priority:** P2 | **Effort:** M | **Labels:** `feature-gap`

**Problem:** Runtime-adopted blinds (`RuntimeBlind`) have a `poll_intvl_ms` field, but nothing in the codebase actually polls them. `drain_runtime_queues()` only sends commands from the queue; there is no periodic `command_check` (0x00) enqueue for runtime blinds. Configured covers get periodic polling via `EleroCover::loop()`, but runtime blinds only update their state when they happen to broadcast a status packet (e.g., in response to a remote command) or when the user explicitly sends a "check" command via the web UI. The `poll_intvl_ms` setting is accepted via the API and stored but has no effect.

**Betroffene Dateien:**
- `components/elero/elero.cpp:391-419` (`drain_runtime_queues`)
- `components/elero/elero.h:148-172` (`RuntimeBlind` struct)

**Fix:** Add periodic polling logic to `drain_runtime_queues()` or a new `poll_runtime_blinds_()` method called from `loop()`. Track `last_poll_ms` per runtime blind and enqueue a check command when the interval elapses.

---

### Issue A8: EleroLight Has No Error State Handling (Asymmetry with Cover H3)

**Priority:** P3 | **Effort:** S | **Labels:** `feature-gap`, `consistency`

**Problem:** Issue H3 added error state handling to `EleroCover::set_rx_state()` for BLOCKING, OVERHEATED, and TIMEOUT states, publishing them to the text sensor and logging warnings. However, `EleroLight::set_rx_state()` (EleroLight.cpp:221-252) only handles `ELERO_STATE_ON` and `ELERO_STATE_OFF`, silently ignoring all other states including errors. If a light receiver reports BLOCKING, OVERHEATED, or TIMEOUT, the user gets no notification.

**Betroffene Dateien:**
- `components/elero/light/EleroLight.cpp:221-252`

**Fix:** Add handling for error states in `EleroLight::set_rx_state()`, mirroring the cover implementation: log a warning and publish to the text sensor.

---

### Issue A9: EleroLight Has No Queue-Full Text Sensor Publication (Asymmetry with Cover H5)

**Priority:** P3 | **Effort:** S | **Labels:** `feature-gap`, `consistency`

**Problem:** Issue H5 added "queue_full" text sensor publication to `EleroCover::start_movement()` when the command queue overflows. While `EleroLight::write_state()` does publish "queue_full" in some code paths, the `enqueue_command()` override in EleroLight.h:30-37 only logs a warning but does NOT publish to the text sensor. This is inconsistent: the cover's `enqueue_command()` also only logs, but its `start_movement()` adds the text sensor publication. The light has no equivalent `start_movement()`, so external callers using `enqueue_command()` (e.g., the web API via `handle_cover_command`) will silently lose commands without any HA-visible feedback.

**Betroffene Dateien:**
- `components/elero/light/EleroLight.h:30-37`

**Fix:** Add `queue_full` text sensor publication in `EleroLight::enqueue_command()` when the queue is full, matching the pattern used in `write_state()`.

---

### Issue A10: `reinit_frequency` and `reinit_frequency_mhz` Race Condition with TX State Machine

**Priority:** P1 | **Effort:** S | **Labels:** `reliability`

**Problem:** Both `reinit_frequency()` (elero.cpp:560-570) and `reinit_frequency_mhz()` (elero.cpp:572-597) forcibly set `tx_state_` to IDLE and call `reset()` + `init()` without checking whether a TX is currently in progress. If called from the web API while a packet is mid-transmission, this corrupts the CC1101 state: `reset()` issues SRES while data may still be in the TX FIFO, and the subsequent `init()` reconfigures all registers while the radio may be in an indeterminate state. The web API handler calls these directly from the async web server context (Arduino loopTask), which shares the same thread as `loop()`, so there is no true race, but the handlers can execute between any two lines in `loop()` if ESPHome yields (e.g., via `delay()`). More critically, `reinit_frequency_mhz()` calls `radio_->setFrequency()` which performs SPI transactions that may conflict with the radio state machine's expectations.

**Betroffene Dateien:**
- `components/elero/elero.cpp:560-597`
- `components/elero_web/elero_web_server.cpp:1071-1138`

**Fix:** Check `is_tx_idle()` before reinitializing. If TX is in progress, either return an error (409 Conflict) from the web API, or queue the frequency change for execution in the next idle `loop()` iteration. The cleanest approach is to set a flag and handle it in `loop()` when TX is idle.

---

### Issue A11: Diagnostic Counters Never Reset and Will Wrap After Extended Uptime

**Priority:** P3 | **Effort:** S | **Labels:** `robustness`

**Problem:** The diagnostic counters `rx_count_`, `tx_count_`, and `watchdog_recovery_count_` (elero.h:464-466) are `uint32_t` values that increment monotonically and are exposed via the `/api/status` endpoint. At high packet rates (e.g., 10 RX/sec), `rx_count_` wraps after ~13.6 years, which is fine. However, the JSON serialization uses `%lu` format which is correct on ESP32 (32-bit `unsigned long`), but there is no API endpoint to reset these counters. The web UI cannot distinguish between "zero events" and "4 billion events" after a wrap. More practically, there is no way to reset counters for a diagnostic session without rebooting.

**Betroffene Dateien:**
- `components/elero/elero.h:374-377`
- `components/elero_web/elero_web_server.cpp:1273-1278`

**Fix:** Add a `/elero/api/diagnostics/reset` POST endpoint that zeroes the counters. Alternatively, expose uptime alongside counters (already available via `/api/info`) so the web UI can compute rates.

---

### Issue A12: `handle_cover_command` Used for Lights via URL Routing Creates Confusing Error Messages

**Priority:** P3 | **Effort:** S | **Labels:** `api-consistency`

**Problem:** In the web server routing (elero_web_server.cpp:166-171), requests to `/elero/api/lights/0xADDR/command` are dispatched to `handle_cover_command()`. This function first tries to find the address in configured covers, then configured lights, then runtime blinds. If no entity is found at all, the error message says "Cover not found" (line 590), which is misleading for a request sent to the lights endpoint. Additionally, the `handle_runtime_command()` and `handle_runtime_settings()` methods (lines 782-788) simply delegate to `handle_cover_command()` and `handle_cover_settings()` without any address-specific error messages.

**Betroffene Dateien:**
- `components/elero_web/elero_web_server.cpp:166-171, 573-591, 782-788`

**Fix:** Change the error message in `handle_cover_command()` to "Device not found" instead of "Cover not found". Alternatively, pass the device type context through to produce accurate error messages.

---

### Issue A13: `EleroLogListener` Leaked on Heap — Never Freed

**Priority:** P3 | **Effort:** S | **Labels:** `code-quality`

**Problem:** In `Elero::setup()` (elero.cpp:549), `new EleroLogListener(this)` is allocated on the heap and passed to `add_log_listener()`, but the pointer is never stored by the `Elero` class. If the component is ever torn down or re-setup, the listener leaks. On ESP32 in normal operation this is benign (the component lives for the entire firmware lifetime), but it makes the code asymmetric with the `radio_`/`radio_module_` cleanup in the destructor.

**Betroffene Dateien:**
- `components/elero/elero.cpp:548-550`
- `components/elero/elero.h` (missing member)

**Fix:** Store the `EleroLogListener*` as a member variable and delete it in the destructor, or use `std::unique_ptr`.

---

### Issue A14: Cover `_final_validate` Does Not Detect Cross-Platform Duplicates with Lights

**Priority:** P2 | **Effort:** S | **Labels:** `robustness`, `validation`

**Problem:** The cover's `_final_validate` (cover/__init__.py:138-164) only checks for duplicate addresses among covers. The light's `_final_validate` (light/__init__.py:98-144) correctly checks both light-light and light-cover duplicates. However, the cover validator does not check against lights. This means if a user configures a cover and a light with the same `blind_address`, the error is only raised if the light config is validated after the cover. Due to ESPHome's validation order being non-deterministic across platforms, this creates an inconsistency: sometimes the duplicate is caught, sometimes it is not.

**Betroffene Dateien:**
- `components/elero/cover/__init__.py:138-164`

**Fix:** Add cross-platform checking in the cover's `_final_validate` to also scan the light config for duplicate addresses, mirroring what the light validator already does.

---

### Issue A15: RuntimeBlind `cmd_counter` Wraps from 255 to 1 But Uses `int` Comparison

**Priority:** P3 | **Effort:** S | **Labels:** `code-quality`

**Problem:** In `drain_runtime_queues()` (elero.cpp:413), the counter increment uses `if (rb.cmd_counter > 255)` to wrap around. But `cmd_counter` is `uint8_t` (elero.h:165), so it can never exceed 255 — the comparison is always false, meaning the counter wraps from 255 to 0 (via unsigned overflow) rather than to 1 as intended. The configured cover/light `increase_counter()` methods correctly check `== 0xff` and set to 1, but the runtime blind path silently uses counter value 0.

**Betroffene Dateien:**
- `components/elero/elero.cpp:413`
- `components/elero/elero.h:165`

**Fix:** Change the runtime blind counter logic to match the configured cover/light pattern:
```cpp
if (rb.cmd_counter == 0xFF)
    rb.cmd_counter = 1;
else
    rb.cmd_counter++;
```

---

## Priority Summary

| # | Issue | Prio | Effort | Category |
|---|-------|------|--------|----------|
| A1  | Duplicated command dispatch logic | P2 | M | Code Quality |
| A2  | Light missing wraparound protection | P2 | S | Robustness |
| A3  | Runtime blind missing wraparound protection | P2 | S | Robustness |
| A4  | Log buffer ring-buffer read order | P1 | S | Bug |
| A5  | log_write_idx_ type too small | P1 | S | Bug |
| A6  | Web server JSON heap pressure | P2 | M | Performance |
| A7  | No polling for runtime blinds | P2 | M | Feature Gap |
| A8  | Light missing error state handling | P3 | S | Consistency |
| A9  | Light missing queue-full text sensor | P3 | S | Consistency |
| A10 | Frequency reinit during TX | P1 | S | Reliability |
| A11 | Diagnostic counters never reset | P3 | S | Robustness |
| A12 | Misleading error messages for lights | P3 | S | API Consistency |
| A13 | EleroLogListener heap leak | P3 | S | Code Quality |
| A14 | Cover validator missing cross-platform check | P2 | S | Validation |
| A15 | Runtime blind cmd_counter overflow bug | P3 | S | Bug |

**Recommended order:** A15 -> A4 -> A5 -> A10 -> A14 -> A2 -> A3 -> A7 -> A1 -> A6 -> A8 -> A9 -> A11 -> A12 -> A13

A15 is a real bug (counter value 0 sent over RF). A4/A5 affect log display correctness. A10 can corrupt radio state. The rest are improvements.
