# Elero Architecture Notes

This document tracks the intended deep Modules and seams for the ESPHome Elero integration. Domain vocabulary lives in [`../agent/CONTEXT.md`](../agent/CONTEXT.md). The delivery decision is recorded in [ADR 2026-07-18](adr/2026-07-18-command-intent-delivery.md).

## Deep Modules

### Command intent delivery

- **Files**: `components/elero/elero_command_delivery.h`, `components/elero/elero_profile_delivery_coordinator.h`, configured cover/light implementations, runtime management, Group cover, and `tests/unit/test_command_delivery.cpp`
- **Interfaces**: device lanes expose `submit(CommandIntent, submitted_at_ms)`; firmware callers pass their monotonic admission time, the hub registers each lane, and it advances one `ProfileDeliveryCoordinator` per shared remote RF profile.
- **Implementation owns**: lanes own bounded semantic queues, admission timestamps, and safe coalescing without rewriting an active sequence; the profile coordinator exclusively owns cross-device ordering, repeat/retry state, 30-second stale aging from admission or latest completed transmission, RF packet construction, urgent STOP preemption, and the rolling command counter. A radio-wide admission gate permits only one queued or in-flight command packet across all profiles, and the hub selects eligible STOP work before normal work when that gate becomes free. A rotating profile cursor provides round-robin fairness within the urgent or normal priority class. Queue admission creates a pending transaction; only the matching Core 0 RF completion advances repeat progress or the rolling counter.
- **Callers own**: outcome callbacks, logging, diagnostics, entity state, and registration lifetime. Compatible Group fallback is configured on its native lane. Timed cover/light movement starts from the monotonic RF completion timestamp carried by the first successful delivery outcome.
- **Concurrency**: submission and atomic multi-member admission are mutex-protected. Submitters can run in the ESPHome loop or AsyncWebServer context. Outcome callbacks run after the coordinator lock is released.
- **Depth**: high — every configured Blind/light, runtime adopted Blind, and compatible native Group lane joins the coordinator for its RF profile. Incompatible Group commands use all-or-none atomic admission into member lanes.

### Packet parser

- **Files**: `components/elero/elero_packet_parser.h`, `components/elero/elero_protocol.cpp`, `tests/unit/test_packet_parser.cpp`
- **Interface**: `packet_parser::parse_fifo_packet(raw_fifo)`
- **Implementation owns**: packet length and destination bounds, CRC/LQI/RSSI extraction, payload decode, and status/command classification.
- **Callers own**: logging, packet dumps, counters, deduplication, scan state, and entity dispatch.

### Command profile and native groups

- **Files**: `components/elero/elero_command_profile.h`, `components/elero_group/EleroGroupCover.cpp`
- **Interface**: `BlindCommandProfile`, `CommandDeliveryConfig`, and `command_profile::can_share_native_group()`.
- **Rule**: native multi-destination delivery requires compatible RF profiles and semantic command mappings. Final native failure falls back only after zero accepted repeats; partial delivery never fans out automatically.

### Radio state logic

- **Files**: `components/elero/elero_radio_state_logic.h`, `components/elero/elero_cc1101.cpp`, `tests/unit/test_radio_state_logic.cpp`
- Pure predicates classify RX/TX/watchdog states. Hardware I/O remains in the Elero hub, and all post-setup CC1101 SPI access stays on the radio task/Core 0.

### Runtime adopted Blind behaviour

- **Files**: `components/elero/elero_runtime.cpp`, `components/elero/elero_runtime_blind_logic.h`, `tests/unit/test_runtime_blind_logic.cpp`
- Runtime adopted Blinds own delivery lanes created from discovery profiles and registered with the matching profile coordinator. Runtime storage remains protected by the hub state mutex; the shared delivery modules own command delivery state.

### Discovery and packet dump management

- **Files**: `components/elero/elero_discovery.cpp`, `components/elero/elero_protocol.cpp`, `components/elero/elero_cc1101.cpp`
- Discovery snapshots and packet dump capture are separate from packet parsing and command delivery.

## Review notes

- Keep ESPHome/FreeRTOS dependencies out of pure logic Modules whenever possible.
- Never reintroduce raw-byte delivery callers; custom bytes must be explicit `CUSTOM` semantic intents.
- Preserve queue bounds, partial-counter retirement, STOP urgency, native Group cover compatibility, and no-fan-out-after-partial-delivery.
- Keep the RF submission callback short and non-reentrant.
