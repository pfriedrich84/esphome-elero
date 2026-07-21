# Agent Checks

Validation commands for agents. Run the smallest relevant set before finishing code changes, and report what passed or failed.

## Documentation-only changes

From the repository root:

```bash
python3 scripts/check_markdown_links.py
```

Use when changing Markdown files, docs indexes, or agent instruction shims. Also verify that command examples and referenced paths are correct.

## Python / ESPHome schema layer

From the repository root:

```bash
ruff check components/
ruff format --check components/
pytest tests/python/ -v --tb=short
```

Use when changing `components/**/__init__.py`, Python validation/codegen, `pyproject.toml`, or Python tests.

## C++ unit tests

From the repository root:

```bash
cmake -S tests -B tests/build -DCMAKE_BUILD_TYPE=Debug
cmake --build tests/build --parallel
cd tests/build && ctest --output-on-failure -V
```

Use when changing pure C++ logic, RF packet parsing, command policy, radio-state helpers, runtime blind helpers, or unit tests.

## ESPHome compile checks

From the repository root:

```bash
esphome compile tests/configs/compile_test.yaml
```

For broad component changes, run the full fixture matrix:

```bash
for cfg in tests/configs/*.yaml; do esphome compile "$cfg"; done
```

Use when changing ESPHome schemas, generated C++ wiring, component registration, YAML parameters, or code that may affect firmware compilation.

## Web UI frontend

From `components/elero_web/frontend/`:

```bash
npm ci
npm run build
```

Use when changing frontend source, REST API assumptions visible in the UI, or generated `components/elero_web/elero_web_ui.h`. Commit the regenerated header with the source change.

## Dependency-sensitive changes

When changing code or configuration that depends on ESPHome, RadioLib, Svelte, Vite, Flowbite, GitHub Actions, or other third-party APIs, verify current external documentation first. Use Context7 when available for public library/framework docs; otherwise use official docs, release notes, repository READMEs, or source code. Summarize non-trivial documentation sources in the final report.

## Full local CI simulation

From the repository root, run the independent checks represented in CI:

```bash
python3 scripts/check_markdown_links.py
ruff check components/
ruff format --check components/
pytest tests/python/ -v --tb=short
cmake -S tests -B tests/build -DCMAKE_BUILD_TYPE=Debug
cmake --build tests/build --parallel
(cd tests/build && ctest --output-on-failure -V)
for cfg in tests/configs/*.yaml; do esphome compile "$cfg"; done
(cd components/elero_web/frontend && npm ci && npm run build)
```

For targeted work, run only the smallest relevant sections above. The full set mirrors the independent CI jobs; hardware-dependent RF behavior still requires explicit manual validation.
