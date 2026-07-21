# Repository Governance Assessment

Last reviewed: 2026-07-21

## Maturity summary

- Documentation maturity: operational
- Validation maturity: operational
- AI-agent readiness: operational
- Supply-chain maturity: developing
- Governance consistency: operational
- Governance drift risk: low to medium
- Platform governance maturity: developing

## Strengths

- Short canonical `AGENTS.md` entry point with modular docs under `docs/agent/`.
- Clear split between user docs, developer docs, and agent docs.
- Strong CI coverage: ESPHome and HACS Ruff checks, Python schema tests, C++ unit tests, ESPHome compile matrix, and frontend build.
- Dependency-free Markdown local-link checker exists and passes.
- Domain invariants are documented for RF safety, CC1101 task ownership, group covers, runtime adopted blinds, and web API safety.

## Governance debt addressed

- Added missing standard agent governance files for constraints, coding, review, supply chain, memory, decisions, anti-patterns, definition of done, assessment, and governance changelog.
- Added GitHub issue and PR templates.
- Added root `LICENSE` to match the GPLv3 README badge/link and make repository licensing visible to GitHub.
- Added root `SECURITY.md` with public guidance for sensitive reports, secrets, private RF captures, and security-sensitive contribution areas.
- Refreshed the active frontend lockfile, verified `npm ci`, and switched frontend CI to lockfile-based installs.
- Documented external documentation/Context7 expectations for dependency-sensitive changes.
- Documented frontend dependency workflow and noted lockfile drift risk.

## Structure audit — 2026-05-17

- Current topology is coherent: root `AGENTS.md` is the canonical agent operating contract; `docs/README.md` is the documentation index; `docs/agent/` contains modular governance; `docs/user/` contains user-facing setup/configuration; `docs/developer/` contains architecture and development guidance; `docs/developer/adr/` is reserved for significant architecture decision records.
- No documentation files should be merged, moved, or deleted as part of the current cleanup. Apparent overlap is intentional: `README.md` is a user quickstart, `docs/user/configuration.md` is the full configuration reference, and `docs/developer/development.md` is the detailed maintainer guide.
- Tool-specific files are shims or command references rather than competing governance: `CLAUDE.md` points to `AGENTS.md`, and `.claude/skills/*.md` documents Claude command workflows.
- `components/elero_web/frontend-legacy/` is not duplicate active source; it is documented reference/rollback material until a future cleanup explicitly decides its lifecycle.
- Preserve content when tightening structure: prefer links, ownership notes, and stale-status labels before moving or deleting files.

## Platform and governance observations — 2026-06-03

- Markdown local-link validation passes with `python3 scripts/check_markdown_links.py`.
- Repository is public and uses `main` as the default branch.
- Secret scanning and push protection are enabled in GitHub repository settings.
- Vulnerability alerts and Dependabot security updates are enabled.
- `.github/dependabot.yml` defines one monthly multi-ecosystem Dependabot version-update group for GitHub Actions and npm frontend dependencies with a 3-day cooldown.
- `main` and `dev` branch protection is reproducible through `.github/scripts/protect-main.sh`: `ci-ok` is required, strict status checks are enabled, force pushes are blocked, and branch deletion is blocked.
- Code scanning/CodeQL status is not established from repository files; GitHub API returned no analysis.
- GitHub Actions are enabled and currently allow all actions; workflow files pin common first-party actions by major version.

## Remaining governance debt

- One accepted formal ADR currently records command-intent delivery; significant future architecture changes should add or supersede ADRs as needed.
- Supply-chain scanning is optional and not wired into CI.
- `frontend-legacy` remains in the repository as documented reference/rollback source; decide in a future cleanup whether to retain or remove it.
- Hardware validation remains external/manual; docs correctly avoid implying automated RF hardware coverage.

## Recommended next steps

1. Keep the accepted command-intent delivery ADR current and add or supersede ADRs when other major RF/runtime decisions change.
2. Decide whether `components/elero_web/frontend-legacy/` should remain as rollback/reference material or be removed in a future cleanup.
3. Consider optional `npm audit` or OSV scanning for periodic dependency reviews.
4. Keep issue/PR templates lightweight; adjust after observing contributor friction.
