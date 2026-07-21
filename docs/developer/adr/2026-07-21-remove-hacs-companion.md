# Remove the HACS companion integration

## Status

Accepted

## Context

The repository contained an experimental Home Assistant custom integration under `custom_components/elero_companion/`. It polled the optional ESP-hosted `/elero/api/*` REST endpoints and was packaged for installation through HACS.

Later planning documents proposed replacing that REST client with a managed-device workflow over ESPHome Native API helpers. That direction was not completed, leaving the repository with an unsupported companion implementation and conflicting descriptions of its intended architecture.

The ESPHome external component already exposes configured covers, lights, sensors, buttons, and groups to Home Assistant through the standard ESPHome Native API. The optional embedded `/elero` web UI remains available for RF discovery, diagnostics, runtime adoption, control, and YAML export.

## Decision

Remove the HACS companion integration, its HACS metadata, its dedicated CI job, and its user installation instructions.

The supported Home Assistant integration path is the standard ESPHome integration backed by entities created by this external component. The optional `/elero` web UI remains part of the ESPHome firmware and is not replaced or removed by this decision.

Remove the superseded companion planning documents rather than retaining them as active guidance:

- `docs/developer/home-assistant-rf-platform.md`
- `docs/developer/native-api-companion-decision.md`
- `docs/user/companion-native-api-process.md`

A future Home Assistant companion or managed-device architecture requires a new ADR. It must define a current use case, ownership boundaries, migration behavior, validation, and maintenance commitment before implementation begins.

## Consequences

- This repository is no longer installable as an Elero Companion custom integration through HACS.
- Existing ESPHome-created Home Assistant entities and the ESPHome YAML workflow are unaffected.
- Users who installed the experimental custom integration should remove its Home Assistant config entry, uninstall its downloaded integration through HACS, and remove the HACS custom-repository entry. Their ESPHome entities remain available through the standard ESPHome integration.
- Companion-specific diagnostic sensors, its web-UI connectivity binary sensor, Repairs issues, and downloadable Home Assistant diagnostics disappear. Users must remove or replace automations that reference those entities.
- The `/elero` REST endpoints remain an implementation surface of the optional embedded web UI, not a promised API for a separate Home Assistant integration.
- Companion-specific Python code, packaging, documentation, and validation no longer need maintenance.

## Validation

- Verify that no active repository file references the removed companion, HACS package, or deleted planning documents.
- Run the Markdown link checker.
- Run the remaining CI jobs to confirm that removing the companion-specific job does not weaken the aggregate `ci-ok` gate.
