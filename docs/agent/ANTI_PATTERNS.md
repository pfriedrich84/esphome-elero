# Agent Anti-Patterns

Repo-specific patterns agents should avoid.

## Avoid

- Broad, unrelated refactors mixed into RF reliability or bug-fix changes.
- Mass formatting churn unrelated to the task.
- Weakening RF packet validation, destination bounds, CRC/LQI checks, stale-counter handling, queue bounds, or watchdog recovery to make tests pass.
- Advancing command counters before a command has been accepted by the transmission path.
- Reusing `dedup_window` as a long counter-resync threshold; these concepts are intentionally separate.
- Manually editing `components/elero_web/elero_web_ui.h` instead of rebuilding it from frontend source.
- Changing generated lockfiles or dependency versions without a dependency-specific reason and validation.
- Adding cloud services, external telemetry, or network dependencies to solve local ESPHome/RF problems.
- Committing real ESPHome secrets, Wi-Fi credentials, OTA/API keys, Home Assistant tokens, private logs, or private RF captures.
- Claiming hardware validation when only unit tests or compile checks were run.

## Preferred alternatives

- Isolate behavior changes in helper modules with focused regression tests.
- Keep docs and examples sanitized with placeholder secrets.
- Use compile fixtures for integration confidence after YAML/codegen changes.
- Keep generated artifacts tied to their source and build command.
