# Agent Project Brief — esphome-elero

Concise project context for coding agents. Prefer canonical docs linked below instead of duplicating them here.

## Purpose

`esphome-elero` is a custom ESPHome external component that lets Home Assistant control Elero wireless blinds, shutters, awnings, and lights through an ESP32 connected to a CC1101 sub-GHz radio. It supports RF discovery, bidirectional status feedback, diagnostic sensors, group covers, runtime adoption from the web UI, and an optional `/elero` web interface for discovery/control/YAML export.

## Core architecture

- **Python ESPHome codegen layer** (`components/**/__init__.py`) validates YAML schemas and emits C++ component wiring.
- **Elero hub C++ layer** (`components/elero/`) owns CC1101 radio orchestration, RF packet parsing/dispatch, TX/RX queues, watchdog recovery, runtime adopted blinds, discovery, and diagnostics.
- **Entity platforms** (`cover`, `light`, `button`, `sensor`, `text_sensor`, `elero_group`) expose Home Assistant entities and enqueue command intents through the hub.
- **Web UI component** (`components/elero_web/`) serves `/elero`, REST endpoints, runtime adoption/control, diagnostics, and an embedded generated frontend header.
- **Tests** combine C++ unit tests for pure logic, Python schema tests, and ESPHome compile fixtures.

## Important paths

```text
components/elero/                  Elero hub, RF protocol, CC1101, cover/light/button/sensor platforms
components/elero_group/            Group cover platform and command policy integration
components/elero_web/              Optional web UI component and REST API
components/elero_web/frontend/     Active Svelte/Vite source for generated elero_web_ui.h
components/elero_web/frontend-legacy/ Legacy Alpine.js source retained for reference/rollback
tests/unit/                        C++ unit tests
tests/python/                      Python schema/codegen tests
tests/configs/                     ESPHome compile matrix fixtures
docs/                              User/developer documentation
docs/agent/                        Tool-neutral agent instructions
```

## Canonical docs

- [`../developer/architecture.md`](../developer/architecture.md) — module seams and deepening notes.
- [`../developer/development.md`](../developer/development.md) — detailed development conventions, architecture, REST API, CI, and pitfalls.
- [`../user/installation.md`](../user/installation.md) — hardware and setup guide.
- [`../user/configuration.md`](../user/configuration.md) — complete configuration reference.
- [`../../README.md`](../../README.md) — overview, quickstart, and troubleshooting.
- [`CONTEXT.md`](CONTEXT.md) — Elero RF domain vocabulary.

## Agent docs

- [`RULES.md`](RULES.md) — non-negotiable project rules.
- [`CONSTRAINTS.md`](CONSTRAINTS.md) — hardware, RF, compatibility, and tooling constraints.
- [`CODING.md`](CODING.md) — coding conventions and external-docs guidance.
- [`REVIEW.md`](REVIEW.md) — PR/diff review priorities.
- [`CHECKS.md`](CHECKS.md) — validation commands.
- [`WORKFLOWS.md`](WORKFLOWS.md) — reusable maintenance workflows.
- [`SAFETY.md`](SAFETY.md) — safe/unsafe operations.
- [`SUPPLY_CHAIN.md`](SUPPLY_CHAIN.md) — dependency and external documentation safety guidance.
- [`MEMORY.md`](MEMORY.md), [`DECISIONS.md`](DECISIONS.md), and [`ANTI_PATTERNS.md`](ANTI_PATTERNS.md) — durable agent memory.
- [`DEFINITION_OF_DONE.md`](DEFINITION_OF_DONE.md) — completion criteria.
- [`AUTORESEARCH.md`](AUTORESEARCH.md) — optional metric-driven experiment workflow.
- [`ASSESSMENT.md`](ASSESSMENT.md) and [`CHANGELOG_AGENT.md`](CHANGELOG_AGENT.md) — governance status and change history.
