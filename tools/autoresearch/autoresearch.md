# Autoresearch: Elero protocol reliability with RadioLib

## Objective
Improve command/packet handling reliability for the Elero protocol stack (CC1101 + RadioLib integration), with emphasis on edge-case timing behavior that can cause delayed command dispatch or stale queue handling.

## Metrics
- **Primary**: `reliability_score_v20` (unitless, higher is better) — combined dispatch + packet-validation + watchdog/radio-state reliability score from deterministic invariants in `autoresearch.sh`.
- **Secondary**:
  - `reliability_score_v2` (dispatch-only score)
  - `packet_score` (packet validation score)
  - `failed_checks` (count, lower is better)
  - `radio_state_score` (radio MARC-state classification score)
  - `unit_seconds` (s, lower is better)

## How to Run
`./autoresearch.sh`

## Files in Scope
- `components/elero/elero_dispatch_logic.h` — pure dispatch timing predicates.
- `components/elero/elero_protocol.cpp` — packet parsing and routing.
- `components/elero/elero_cc1101.cpp` — CC1101 RX/TX and watchdog behavior.
- `tests/unit/*.cpp` — regression tests for protocol/dispatch behavior.
- `autoresearch.sh` / `autoresearch.checks.sh` — benchmark + correctness checks.

## Off Limits
- Benchmark cheating (no fake metric output).
- Unrelated UI/docs changes.

## Constraints
- Keep changes focused on reliability/functionality.
- Preserve existing behavior unless there is a clear correctness/reliability benefit.
- Do not overfit to benchmark-only behavior; keep invariants generally valid.

## What's Been Tried
- Baseline reliability probe found boundary failures in dispatch predicates.
- **Kept:** `is_dispatch_ready()` and `should_clear_stale_queue()` now use inclusive `>=` boundaries to avoid 1-tick timing blind spots.
- **Kept:** retry drop now triggers at retry ceiling (`should_drop_after_retries >= max_retries`) to avoid extra failed retry cycles.
- **Kept (corrected):** packet decode bounds now match actual payload indexing (`26 + dests_len` must be within `length`) to avoid rejecting valid frames while preserving safety checks.
- **Kept:** strict destination count validation: reject `num_dests == 0` and `num_dests > max` with explicit reasons (`zero_dests`, `too_many_dests`).
- **Kept:** explicit CRC status helper in `elero_packet_validation.h` and parser integration in `interpret_msg()`; bad-CRC packets are dropped before decode with `bad_crc` reason.
- **Kept:** watchdog escalation helpers added and wired (`elero_watchdog_logic.h`) with healthy-state reset and inclusive window-expiry behavior.
- **Kept:** recovery helper (`elero_recovery_logic.h`) removes double counting in `send_command_internal_` reinit-failure escalation path.
- **Kept:** RX overflow helper (`elero_overflow_logic.h`) centralizes overflow count progression and reinit threshold behavior in `process_rx()`.
- **Kept:** TX sanitization helper (`elero_tx_logic.h`) clamps oversized `num_dests` to max destinations instead of collapsing to single destination.
- **Kept:** TX destination-address availability and effective-destination helpers prevent malformed group commands from sending unset/zero destination slots.
- **Kept:** packet bounds validation now requires the full copied/decrypted 10-byte payload (`28 + dests_len`) to fit inside declared packet length, preventing status/stale FIFO bytes from being decoded as payload.
- **Kept:** retry-drop path advances the command counter when a partially transmitted command is abandoned after retry exhaustion, matching stale-queue cleanup semantics and avoiding counter reuse.
- **Kept:** watchdog radio-state classification treats CC1101 TX/RX and RX/TX switch states as transient while keeping FIFO overflow/underflow as non-transient errors.
- Environment lacked `cmake` and `esphome` for full upstream tests, so local dependency-free checks are used in this session.
