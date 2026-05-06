# Agent Autoresearch

Optional workflow for autonomous, metric-driven experiment loops. Use it only when the task has a measurable target and repeated iterations are useful.

This repository includes a ready-to-run reliability harness:

```bash
./autoresearch.sh
./autoresearch.checks.sh
```

Harness details and session history live in [`../../tools/autoresearch/autoresearch.md`](../../tools/autoresearch/autoresearch.md). Future ideas live in [`../../tools/autoresearch/autoresearch.ideas.md`](../../tools/autoresearch/autoresearch.ideas.md).

## Good fits

- RF packet parser reliability and bounds handling.
- CC1101 radio-state, watchdog, FIFO, and TX/RX edge-case behavior.
- Command queue, group cover, runtime adopted blind, and counter semantics.
- Web UI/API hardening when there is a deterministic metric or probe.
- Performance or memory improvements with a stable benchmark.

## Poor fits

- Documentation-only cleanup.
- Broad subjective refactors without a primary metric.
- Security-sensitive changes to auth/CORS without explicit review.
- Changes requiring real user RF captures, Wi-Fi secrets, OTA/API keys, or private Home Assistant data in logs.

## Required setup

Before starting an experiment loop, define:

1. **Primary metric** — one number to optimize, with direction.
2. **Benchmark command** — deterministic enough to compare iterations.
3. **Safety checks** — tests/lints/compile checks that must still pass.
4. **Rollback rule** — discard experiments that do not improve the primary metric or fail checks.
5. **Data boundary** — never log secrets, private YAML, or private RF captures unless explicitly sanitized.

## Experiment rules

- Keep each iteration small and reviewable.
- Preserve invariants from [`RULES.md`](RULES.md).
- Run relevant checks from [`CHECKS.md`](CHECKS.md) after each kept result.
- Prefer synthetic fixtures and pure helper tests over hardware-only validation.
- Record failed ideas and why they failed so they are not repeated.
- Do not keep a change solely because a secondary metric improved.
