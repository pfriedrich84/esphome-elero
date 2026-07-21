# Architecture Decision Records

Use this directory for significant architecture decisions that future maintainers and coding agents should preserve.

For small operational decisions, use [`../../agent/DECISIONS.md`](../../agent/DECISIONS.md). Add a dedicated ADR here when a decision changes module boundaries, RF protocol semantics, persistence, public YAML/API behavior, or long-term dependency/tooling strategy.

Suggested filename format:

```text
YYYY-MM-DD-short-title.md
```

## Records

- [2026-07-18 — Command intent delivery](2026-07-18-command-intent-delivery.md)
- [2026-07-21 — Remove the HACS companion integration](2026-07-21-remove-hacs-companion.md)

Suggested ADR sections:

- Status
- Context
- Decision
- Consequences
- Validation
