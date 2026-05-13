# AGENTS.md — esphome-elero Agent Instructions

Tool-neutral entry point for coding agents working in this repository. Keep this file short; put durable details in `docs/agent/`.

## Read first

1. [`docs/agent/RULES.md`](docs/agent/RULES.md) — non-negotiable project rules and domain invariants.
2. [`docs/agent/CONSTRAINTS.md`](docs/agent/CONSTRAINTS.md) — hardware, RF, compatibility, and tooling constraints.
3. [`docs/agent/PROJECT.md`](docs/agent/PROJECT.md) — project context, architecture notes, and important implementation details.
4. [`docs/agent/CODING.md`](docs/agent/CODING.md) — coding conventions, implementation patterns, and external-docs guidance.
5. [`docs/agent/CHECKS.md`](docs/agent/CHECKS.md) — validation commands to run before finishing code changes.
6. [`docs/agent/WORKFLOWS.md`](docs/agent/WORKFLOWS.md) — reusable maintenance workflows.
7. [`docs/agent/SAFETY.md`](docs/agent/SAFETY.md) — safe/unsafe operations for agents.
8. [`docs/agent/DEFINITION_OF_DONE.md`](docs/agent/DEFINITION_OF_DONE.md) — completion criteria for changes.
9. [`docs/agent/AUTORESEARCH.md`](docs/agent/AUTORESEARCH.md) — optional metric-driven experiment workflow.

## Project docs

- [`docs/developer/architecture.md`](docs/developer/architecture.md) — module seams and architecture notes.
- [`docs/developer/development.md`](docs/developer/development.md) — detailed development notes and conventions.
- [`docs/user/installation.md`](docs/user/installation.md) — hardware and ESPHome setup.
- [`docs/user/configuration.md`](docs/user/configuration.md) — complete YAML configuration reference.
- [`README.md`](README.md) — user-facing overview and quickstart.

## Quick summary

`esphome-elero` is an ESPHome external component for controlling Elero blinds and lights through an ESP32 with a CC1101 radio. The Python layer validates ESPHome YAML and generates C++; the C++ layer owns RF packet parsing, CC1101 state handling, cover/light entities, group covers, runtime adopted blinds, and the optional `/elero` web UI.

Before finishing code changes, run the relevant checks from [`docs/agent/CHECKS.md`](docs/agent/CHECKS.md). Use branch names `feat/<short-topic>` or `fix/<short-topic>` for new work; `dev` is the shared development branch.
