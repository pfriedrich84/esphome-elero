# Agent Decision Log

Lightweight decision memory for future agents. For major architecture decisions, add an ADR under `docs/developer/adr/`.

## Decisions

### Use tool-neutral agent instructions

- `AGENTS.md` is the canonical root entry point for coding agents.
- Tool-specific files such as `CLAUDE.md` should stay as shims unless a tool genuinely needs extra syntax.

### Keep RF logic testable without hardware

- Protocol, counter, deduplication, queue, cover/light, radio-state, and group-command rules should be extracted into dependency-light helpers when practical.
- Rationale: C++ unit tests provide fast regression coverage without ESPHome firmware compilation or physical CC1101 hardware.

### Use `dev` as the shared integration branch

- `main` is the release/default branch and `dev` is the protected shared development branch.
- Normal feature, fix, and documentation pull requests target `dev`.
- Promote `dev` to `main` through a release pull request. Direct pull requests to `main` are reserved for urgent fixes and require the `hotfix` label.
- CI enforces the `main` target policy, and release workflow runs are restricted to `main`.
- CI is configured to run on `dev`, `main`, `feat/**`, `fix/**`, and `docs/**`.
- Direct pushes to `docs/**` run markdown validation only; full CI still runs when the branch is opened as a pull request to `dev`.
- `docs/**` branches are documentation/governance-only and must not carry code, dependency, generated artifact, or runtime workflow changes.

### Prefer native group RF packets only when command profiles match

- Group covers may send a native multi-destination RF packet only when member command profiles are compatible.
- Otherwise they must fall back to individual member command queues.

### Use a separate counter resync threshold

- Short `dedup_window` remains for immediate duplicate suppression.
- Stale status counter resync uses a separate longer quiet-gap threshold so lossy links/restarts can recover without accepting repeated stale traffic.
