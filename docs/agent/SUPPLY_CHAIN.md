# Agent Supply-Chain Guidance

Supply-chain rules for repository changes.

## Dependency policy

- Prefer existing dependencies and standard-library functionality over adding new packages.
- Do not add or upgrade dependencies casually; explain the need, risk, and validation path.
- Prefer packages/releases that have existed for at least 3 days before adoption unless the maintainer explicitly approves a newer release.
- Dependabot version updates are configured in `.github/dependabot.yml` as one monthly multi-ecosystem group with a 3-day cooldown for GitHub Actions and npm ecosystems.
- Preserve lockfiles and prefer lockfile-based installs when lockfiles are in sync. Do not introduce unrelated lockfile churn.
- Do not commit secrets, tokens, private logs, Wi-Fi credentials, OTA/API keys, or private RF captures.

## Current dependency surfaces

- Python tooling: Ruff, pytest, ESPHome.
- Firmware/component ecosystem: ESPHome, Arduino/ESP-IDF through ESPHome, RadioLib, CMake/GoogleTest for unit tests.
- Web frontend: Node 22, Svelte, Vite, Tailwind, Flowbite/Flowbite-Svelte, generated embedded header.
- CI/release: GitHub Actions, `gh` CLI in release workflow.

## External documentation requirement

Before dependency-sensitive code/config changes, verify current official documentation. Use Context7 when available for public libraries/frameworks/CLIs; otherwise use official docs, release notes, repository READMEs, or source code.

Use this especially for:

- ESPHome schema/codegen APIs and YAML formats.
- RadioLib/CC1101 behavior.
- Svelte/Vite/Tailwind/Flowbite build behavior.
- GitHub Actions syntax, permissions, artifact behavior, and release automation.

## Dependabot update surfaces

Dependabot version updates are intentionally limited to dependency manifests that exist in this repository and are grouped into one monthly multi-ecosystem pull request:

- GitHub Actions in `.github/workflows/`.
- Active web frontend npm dependencies in `components/elero_web/frontend/`.
- Legacy web frontend npm dependencies in `components/elero_web/frontend-legacy/`.

The repository does not currently have Python dependency lockfiles or package manifests for Dependabot to maintain. CI pins Python tools directly in workflow files, so those pins are covered by the GitHub Actions ecosystem review.

Keep Dependabot version-update noise low: do not split these surfaces into separate schedules or ungrouped update entries unless the maintainer explicitly accepts multiple Dependabot PRs per run.

## Optional security checks

Run only when tools are available or explicitly approved:

```bash
npm audit --prefix components/elero_web/frontend
npm audit --prefix components/elero_web/frontend-legacy
```

For broader supply-chain reviews, optional tools include `osv-scanner`, `pip-audit`, `trivy`, `grype`, or `syft`. Do not add these tools to CI without maintainer approval.
