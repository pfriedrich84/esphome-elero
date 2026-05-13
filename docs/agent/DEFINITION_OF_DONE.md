# Definition of Done

Completion criteria for agent work in `esphome-elero`.

## For every change

- The change is focused and reviewable.
- Relevant docs are updated when behavior, YAML, REST endpoints, wiring, or user workflows change.
- Secrets and private logs/captures are not added.
- The final summary lists changed files and validation results.

## Code changes

- RF/protocol/state behavior changes include focused C++ unit tests where practical.
- Python schema/codegen changes include Python tests and/or ESPHome compile fixtures.
- Web UI changes are made in frontend source and the generated header is rebuilt.
- Safety invariants from `RULES.md`, `CONSTRAINTS.md`, and `SAFETY.md` are preserved.

## Validation

Run the smallest relevant checks from `CHECKS.md`:

- Markdown/docs: `python3 scripts/check_markdown_links.py`
- Python/schema: Ruff + Python tests
- C++ logic: CMake build + CTest
- ESPHome integration: compile fixture(s)
- Frontend: `npm install` + `npm run build` from `components/elero_web/frontend/`

If a check cannot run locally because tooling is missing, state that clearly and rely on CI where appropriate.

## GitHub readiness

Before opening or updating a PR:

- PR description explains what changed and why.
- Validation checklist is filled in.
- Related issues are referenced with `Refs #...` or `Fixes #...` as appropriate.
- Generated files and lockfiles are included only when intentionally changed.
