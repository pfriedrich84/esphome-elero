# Agent Memory

Durable repository-local memory for future agents. Store only non-secret, evidence-backed project knowledge here.

## Durable project notes

- `dev` is the shared development branch. CI also runs for `main`, `feat/**`, and `fix/**` branches.
- The project consistently separates pure RF/protocol/state helper logic from ESPHome-dependent runtime code so C++ unit tests can run without hardware.
- Web UI source lives under `components/elero_web/frontend/`; `components/elero_web/elero_web_ui.h` is generated output and should not be edited manually.
- ESPHome compile fixtures under `tests/configs/` are the integration safety net for YAML/codegen changes.
- Local environments may not have ESPHome installed; CI pins and installs ESPHome for Python tests and compile fixtures.
- CC1101 write/readback self-tests must use a register whose bits are all R/W (e.g. `ADDR`). `FSCTRL1` bits 7:6 always read 0, so 0xAA/0x55 patterns on it raised a false "stuck MOSI" warning on healthy hardware (#250).

## Maintenance notes

- Keep this file concise. Prefer canonical user/developer docs for detailed behavior.
- Do not store chat transcripts, secrets, private logs, private RF captures, or speculative assumptions.
