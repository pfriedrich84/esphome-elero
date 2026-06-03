# Security Policy

## Reporting sensitive issues

Please do not open public issues or pull requests that contain secrets, credentials, private RF captures, or private device logs.

Sensitive material includes, but is not limited to:

- Wi-Fi SSIDs/passwords, ESPHome secrets, OTA passwords, API encryption keys, Home Assistant tokens, or GitHub tokens.
- Private ESPHome YAML files containing credentials or household-specific device details.
- RF captures, logs, screenshots, or exported YAML that expose private addresses, timing, home layout, or device behavior.

For sensitive security reports, use GitHub private vulnerability reporting or GitHub Security Advisories when available. If private reporting is not available, open a minimal public issue that describes the affected area without secrets or private captures, and ask for a private contact path.

## Scope

This repository contains an ESPHome external component for Elero RF control. Security-sensitive areas include packet validation, RF command/counter handling, queue bounds, web UI authentication and same-origin/CORS behavior, JSON escaping, generated web UI assets, GitHub Actions workflows, and dependency updates.

## Contributor expectations

- Redact secrets and private device data before sharing logs, YAML, screenshots, or RF traces.
- Do not weaken authentication, packet validation, bounds checks, or same-origin/CORS protections without a focused security rationale and review.
- Follow the validation guidance in [`docs/agent/CHECKS.md`](docs/agent/CHECKS.md) for changes that affect runtime behavior, web UI behavior, dependencies, or workflows.
