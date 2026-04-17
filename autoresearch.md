# Autoresearch: Elero protocol reliability with RadioLib

## Objective
Improve command/packet handling reliability for the Elero protocol stack (CC1101 + RadioLib integration), with emphasis on edge-case timing behavior that can cause delayed command dispatch or stale queue handling.

## Metrics
- **Primary**: `reliability_score` (unitless, higher is better) — score from deterministic protocol/dispatch invariants in `autoresearch.sh`.
- **Secondary**:
  - `failed_checks` (count, lower is better)
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
- Baseline reliability probe found 2 boundary failures in dispatch predicates.
- **Kept:** `is_dispatch_ready()` and `should_clear_stale_queue()` now use inclusive `>=` boundaries to avoid 1-tick timing blind spots.
- Tried earlier retry-drop (`send_retries >= max_retries`), but no reliability_score gain; discarded.
- Environment lacked `cmake` and `esphome` for full upstream tests, so local dependency-free checks are used in this session.
