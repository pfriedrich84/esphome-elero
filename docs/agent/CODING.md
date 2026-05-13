# Agent Coding Guidance

Project-specific coding guidance for implementation agents.

## General discipline

- Keep changes small, focused, and reviewable.
- Prefer pure helper modules for RF/protocol/state logic so behavior can be unit-tested without ESPHome hardware.
- Add or update tests with behavior changes; prefer regression tests for RF reliability fixes.
- Avoid broad formatting-only churn outside the touched lines.
- Update user/developer docs when YAML parameters, REST endpoints, entity behavior, wiring guidance, or web UI behavior changes.

## C++ patterns

- Keep RF packet parsing, counter logic, queue policy, radio-state classification, and runtime blind helpers dependency-light.
- Preserve existing naming style: private C++ members use trailing underscores.
- Prefer explicit constants for timing/queue thresholds and cover them with unit tests when behavior-sensitive.
- Treat queue-full, SPI failure, and watchdog paths as reliability-critical; do not collapse distinct failure modes unless tests prove it is safe.

## Python / ESPHome codegen patterns

- Keep YAML schema validation close to the component platform in `components/**/__init__.py`.
- Validate inconsistent user configuration early with clear `cv.Invalid` messages.
- When adding YAML parameters, wire schema, codegen, C++ storage/behavior, docs, tests, and compile fixtures together.

## Web UI patterns

- Edit frontend source under `components/elero_web/frontend/`, then rebuild the generated header.
- Preserve backend REST behavior for auth, same-origin/CORS, JSON escaping, and disabled UI.
- Prefer lockfile-based frontend installs once lockfiles are in sync; until then use the documented `npm install` workflow and avoid unrelated lockfile churn.

## External documentation

When changing behavior that depends on ESPHome, RadioLib, Svelte, Vite, Flowbite, GitHub Actions, CMake, or other third-party APIs/config formats, verify current official documentation before implementing. Use Context7 when available for public library/framework docs; otherwise use official docs, release notes, repository READMEs, or source code. Summarize non-trivial external documentation used in the final response.
