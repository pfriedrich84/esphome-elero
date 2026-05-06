---
name: test
description: "Run project tests (C++ unit tests and/or Python tests). Use to verify code correctness after changes."
user-invocable: true
---

# Run Project Tests

Canonical repository instructions live in [`../../AGENTS.md`](../../AGENTS.md); this Claude skill only defines the slash-command workflow.

Execute the test suites and report results.

## Task

Determine which tests to run based on `$ARGUMENTS`:
- No argument or `all`: run both C++ and Python tests
- `cpp`: C++ unit tests only
- `python`: Python tests only

### C++ Unit Tests (GoogleTest)

```bash
cmake -S tests -B tests/build -DCMAKE_BUILD_TYPE=Debug
cmake --build tests/build --parallel
cd tests/build && ctest --output-on-failure -V
```

If CMake is not available or tests directory has no CMakeLists.txt yet, report that C++ tests are not yet set up (Phase 2 of the quality plan).

### Python Tests (pytest)

```bash
pytest tests/python/ -v --tb=short
```

If pytest is not available: `pip install pytest`
If tests/python/ doesn't exist yet, report that Python tests are not yet set up (Phase 3 of the quality plan).

## Reporting

- Count passed / failed / skipped for each suite
- On failure: show the specific test output with file:line references
- Suggest likely root cause for failures

## Arguments

`$ARGUMENTS` — `cpp`, `python`, `all`, or empty (defaults to `all`)
