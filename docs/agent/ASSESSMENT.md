# Repository Governance Assessment

Last reviewed: 2026-05-13

## Maturity summary

- Documentation maturity: operational
- Validation maturity: operational
- AI-agent readiness: operational
- Supply-chain maturity: developing
- Governance consistency: operational
- Governance drift risk: low to medium

## Strengths

- Short canonical `AGENTS.md` entry point with modular docs under `docs/agent/`.
- Clear split between user docs, developer docs, and agent docs.
- Strong CI coverage: Ruff, Python schema tests, C++ unit tests, ESPHome compile matrix, and frontend build.
- Dependency-free Markdown local-link checker exists and passes.
- Domain invariants are documented for RF safety, CC1101 task ownership, group covers, runtime adopted blinds, and web API safety.

## Governance debt addressed

- Added missing standard agent governance files for constraints, coding, review, supply chain, memory, decisions, anti-patterns, definition of done, assessment, and governance changelog.
- Added GitHub issue and PR templates.
- Documented external documentation/Context7 expectations for dependency-sensitive changes.
- Documented frontend dependency workflow and noted lockfile drift risk.

## Remaining governance debt

- No formal ADRs exist yet under `docs/developer/adr/`; significant future architecture changes should add one.
- Supply-chain scanning is optional and not wired into CI.
- `frontend-legacy` remains in the repository; its lifecycle/status should be documented if it is still intentionally retained.
- `components/elero_web/frontend/package-lock.json` is not currently compatible with `npm ci`; keep CI on `npm install` or deliberately refresh the lockfile in a focused dependency-maintenance change.
- Hardware validation remains external/manual; docs correctly avoid implying automated RF hardware coverage.

## Recommended next steps

1. Add ADRs for major RF/runtime design decisions when they next change.
2. Decide whether `components/elero_web/frontend-legacy/` is retained for rollback/reference or should be removed in a future cleanup.
3. Refresh the active frontend lockfile in a focused dependency-maintenance change, then consider switching CI to `npm ci`.
4. Consider optional `npm audit` or OSV scanning for periodic dependency reviews.
5. Keep issue/PR templates lightweight; adjust after observing contributor friction.
