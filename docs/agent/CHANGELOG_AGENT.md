# Agent Governance Changelog

This changelog tracks changes to repository governance and agent instructions. It is not the user-facing release changelog.

## 2026-07-23

- Removed the inactive Alpine.js legacy frontend and updated frontend and supply-chain guidance to describe only the maintained Svelte UI.

## 2026-05-17

- Refreshed stale developer documentation references for user docs, active Svelte frontend source, CI triggers/jobs, and branch conventions.
- Added `docs/**` as an officially supported CI branch pattern for documentation/governance work, with direct pushes limited to markdown validation and documented as documentation-only.
- Documented release automation governance in `WORKFLOWS.md`.
- Clarified that `frontend-legacy` is retained as reference/rollback source until a future cleanup decides its lifecycle.

## 2026-05-13

- Added standard modular agent governance files:
  - `CONSTRAINTS.md`
  - `CODING.md`
  - `REVIEW.md`
  - `SUPPLY_CHAIN.md`
  - `MEMORY.md`
  - `DECISIONS.md`
  - `ANTI_PATTERNS.md`
  - `DEFINITION_OF_DONE.md`
  - `ASSESSMENT.md`
  - `CHANGELOG_AGENT.md`
- Added GitHub issue and pull request templates.
- Updated agent/doc indexes to expose the expanded governance structure.
- Documented dependency-sensitive external documentation checks and Context7 usage.
- Documented frontend dependency workflow and recorded lockfile-sync follow-up instead of changing CI install behavior.
