---
name: review
description: "Code quality review with project-specific quality gates. Use before committing or creating PRs."
user-invocable: true
---

# Code Quality Review

Review pending changes against project-specific quality gates.

## Quality Gates

Check each gate and report pass/fail:

### Gate 1: Compilation
Run `esphome compile compile_test.yaml` — must succeed.

### Gate 2: Linting
Run `ruff check components/` — no errors.

### Gate 3: Thread Safety
Check any new or modified code for correct multi-core patterns:
- `std::atomic` loads use `std::memory_order_acquire`
- `std::atomic` stores use `std::memory_order_release`
- Shared data (runtime_blinds_, discovered_blinds_) accessed under `state_mutex_`
- Log buffer accessed under `log_mutex_`
- No SPI access outside Core 0 after setup()

### Gate 4: Buffer Safety
Check protocol/RF code for bounds validation:
- Array accesses preceded by length checks
- `msg_rx_` indices validated against `ELERO_MAX_PACKET_SIZE`
- FreeRTOS queue operations check return values

### Gate 5: Web API Consistency
New endpoints must have:
- CORS headers via `add_cors_headers()`
- Auth check when `username_`/`password_` are set
- OPTIONS handler for POST/DELETE endpoints
- JSON-escaped user strings via `json_escape()`

### Gate 6: Memory Safety
- No unbounded `std::vector` growth (use max size caps)
- No heap allocation in ISR context
- `std::string` reserves capacity for known-size JSON

### Gate 7: Documentation
- New YAML parameters documented in `docs/CONFIGURATION.md`
- New constants added to `CLAUDE.md` key constants table
- New API endpoints added to the REST API table in `CLAUDE.md`

## Output Format

For each gate: PASS / FAIL / SKIP (not applicable)
For failures: specific file:line references and concrete suggestions.

## Arguments

`$ARGUMENTS` — optional: specific files or directories to review (defaults to `git diff` output)
