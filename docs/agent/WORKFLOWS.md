# Agent Workflows

Reusable, tool-neutral workflows for common repository tasks.

## Branch naming

Use these branch names for new work so GitHub Actions CI runs predictably:

- `dev` for the shared development branch.
- `feat/<short-topic>` for feature work.
- `fix/<short-topic>` for bug fixes.
- `docs/<short-topic>` for documentation-only or governance cleanup work.

Avoid legacy prefixes such as `feature/`, `bugfix/`, ad-hoc names, or direct work on `main` unless a maintainer explicitly requests a different branch. CI is configured for pushes to `main`, `dev`, `feat/**`, `fix/**`, and `docs/**`, plus pull requests targeting `main` or `dev`. Pushes to `docs/**` run markdown validation only; open a PR to `main` or `dev` when full CI is needed before merge. Do not carry code, dependency, generated artifact, or runtime workflow changes on `docs/**` branches. Other branch names are validated when opened as PRs to `main` or `dev`.

## Standard change workflow

1. Read [`RULES.md`](RULES.md), [`PROJECT.md`](PROJECT.md), and the relevant user/developer docs.
2. Keep the change small and reviewable.
3. Update tests and docs when behavior changes.
4. Run the relevant checks from [`CHECKS.md`](CHECKS.md).
5. Summarize changed files, validation results, and follow-up work.

## Local CI simulation

Use this when a change touches multiple subsystems or CI configuration.

1. Python lint/format:
   ```bash
   ruff check components/
   ruff format --check components/
   ```
2. Python tests:
   ```bash
   pytest tests/python/ -v --tb=short
   ```
3. C++ unit tests:
   ```bash
   cmake -S tests -B tests/build -DCMAKE_BUILD_TYPE=Debug
   cmake --build tests/build --parallel
   cd tests/build && ctest --output-on-failure -V
   ```
4. ESPHome compile matrix when relevant:
   ```bash
   for cfg in tests/configs/*.yaml; do esphome compile "$cfg"; done
   ```
5. Web UI build when relevant:
   ```bash
   cd components/elero_web/frontend
   npm install
   npm run build
   ```
6. Docs validation:
   ```bash
   python3 scripts/check_markdown_links.py
   ```

If one check fails, continue with independent checks where practical so the final report is complete.

## Web UI change workflow

1. Edit source under `components/elero_web/frontend/` or REST API code under `components/elero_web/`.
2. Preserve auth checks, same-origin/CORS behavior, JSON escaping, and disabled-UI HTTP 503 behavior.
3. Run `npm install`, then `npm run build` from the frontend directory. Prefer `npm ci` only after the frontend lockfile is confirmed in sync with `package.json`.
4. Verify `components/elero_web/elero_web_ui.h` changed only through the build.
5. Run relevant C++/ESPHome checks if backend handlers changed.

## Release governance

Release automation is defined in `.github/workflows/release.yml`.

- Releases run weekly on Saturday at 06:00 UTC and can also be triggered manually with `workflow_dispatch`.
- The workflow creates date-based tags in `YYYY-MM-NN` format only when non-merge commits exist since the previous date-based release tag.
- Release notes are generated from conventional-commit-style prefixes (`feat`, `fix`, `perf`, `ci`/`devops`, `docs`, `refactor`/`test`).
- The workflow needs `contents: write` and uses the repository `GITHUB_TOKEN` through the `gh` CLI to create tags and GitHub releases.
- Do not change release permissions, tag format, or published artifact behavior without focused review and validation.

## Dependency documentation workflow

Use this before dependency-sensitive changes to ESPHome, RadioLib, Svelte, Vite, Flowbite, GitHub Actions, CMake, or generated-client/config formats.

1. Check current official documentation. Use Context7 when available for public libraries/frameworks/CLIs.
2. Verify version-specific behavior against the versions used by CI, lockfiles, or ESPHome.
3. Keep dependency changes minimal and prefer existing packages.
4. Summarize documentation sources and validation results in the final report.

## YAML parameter workflow

1. Update ESPHome schema/codegen in the relevant `components/**/__init__.py`.
2. Update C++ storage/behavior as needed.
3. Add or update Python schema tests and compile fixtures.
4. Document the parameter in [`../user/configuration.md`](../user/configuration.md) and update `README.md` when it affects common usage.
5. Run Python checks and at least one ESPHome compile fixture.

## RF reliability workflow

1. Prefer extracting protocol/radio/queue rules into pure helper modules when possible.
2. Add C++ unit tests for edge cases before or alongside behavior changes.
3. Preserve queue bounds, packet length/bounds checks, watchdog escalation, and counter-advance semantics.
4. Run C++ unit tests and relevant compile fixtures.
5. If the change is experimental, record failed ideas or follow-ups in the autoresearch notes.
