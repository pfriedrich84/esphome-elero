# Agent Safety

Tool-neutral safety guidance for coding agents.

## Allowed by default

- Read, search, and make targeted edits to repository files.
- Run validation commands listed in [`CHECKS.md`](CHECKS.md).
- Use read-only Git commands for orientation: `git status`, `git diff`, `git log`, `git branch`.
- Read example YAML and CI configuration.
- Rebuild generated web UI output when frontend source changes.

## Ask first / avoid unless explicitly requested

- Destructive filesystem operations, especially recursive deletes.
- History rewriting or destructive Git operations such as force-push or hard reset.
- Broad formatting or large refactors unrelated to the task.
- Changing generated lock files or dependency versions without validating dependency state.
- Removing compile fixtures, unit tests, or safety checks to make CI pass.
- Changing RF defaults, watchdog thresholds, or authentication behavior without a clear task and regression coverage.

## Never do

- Do not print, copy, or modify secrets from private ESPHome YAML, Wi-Fi credentials, OTA/API keys, or local device logs.
- Do not weaken packet validation, auth checks, queue bounds, JSON escaping, or same-origin/CORS restrictions without an explicit security-driven task.
- Do not edit `components/elero_web/elero_web_ui.h` manually.
- Do not introduce SPI access from both ESP32 cores after setup.
- Do not claim hardware validation unless it was actually performed.

## Before finishing

Run the relevant checks from [`CHECKS.md`](CHECKS.md), or state clearly why checks were not run for documentation-only or planning-only work.
