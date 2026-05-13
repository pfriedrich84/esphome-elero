## Summary

- 

## Related issues

Refs #

## Type of change

- [ ] Bug fix
- [ ] Feature
- [ ] Documentation
- [ ] Refactor / maintenance
- [ ] CI / tooling

## Validation

Run the smallest relevant checks from `docs/agent/CHECKS.md` and mark what applies.

- [ ] Markdown links: `python3 scripts/check_markdown_links.py`
- [ ] Python lint/format: `ruff check components/` and `ruff format --check components/`
- [ ] Python tests: `pytest tests/python/ -v --tb=short`
- [ ] C++ unit tests: CMake build + CTest
- [ ] ESPHome compile fixture(s)
- [ ] Frontend build from `components/elero_web/frontend/`
- [ ] Not run / not applicable (explain below)

## Notes for reviewers

- RF/protocol safety impact:
- YAML/API/entity behavior impact:
- Generated files or dependency changes:
