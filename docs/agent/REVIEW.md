# Agent Review Guidance

Use this checklist when reviewing pull requests, diffs, or implementation plans.

## Review priorities

1. RF/protocol safety: packet bounds, CRC/LQI, destination count, counters, deduplication, TX/RX queues, watchdog recovery.
2. ESPHome/Home Assistant semantics: entities, IDs, generated code, YAML validation, compile fixtures.
3. Web/API safety: auth, same-origin/CORS, JSON escaping, disabled-UI behavior, generated frontend header workflow.
4. Regression coverage: pure C++ tests for helper logic, Python tests for schema validation, ESPHome compile fixtures for integration.
5. Documentation: configuration reference, README quickstart, developer notes, and agent docs updated when behavior changes.

## Red flags

- Manual edits to `components/elero_web/elero_web_ui.h` without frontend source changes/build.
- Weakened validation or queue bounds without a documented reliability/security reason.
- Command counters advanced before a command is accepted for transmission.
- Group cover native packet use without compatible member command profiles.
- New dependencies, version changes, or generated lockfile churn without explanation.
- Broad refactors mixed with bug fixes.

## Suggested review output

- Start with blocking correctness/safety issues.
- Separate must-fix items from optional cleanup.
- Include exact file/function references when possible.
- Note checks that were run or should be run before merge.
