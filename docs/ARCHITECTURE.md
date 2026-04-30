# Elero Architecture Notes

This document tracks the intended deep Modules and seams for the ESPHome Elero integration. Domain vocabulary lives in [`../CONTEXT.md`](../CONTEXT.md).

## Current deepening sequence

1. **Packet parser** — raw CC1101 FIFO bytes become Elero RF packet fields without hub side effects.
2. **Group command policy** — Group cover command intent handling has explicit fallback semantics.
3. **Command profile** — Group cover reads one Blind command profile instead of many unrelated RF getters.
4. **CC1101 radio orchestration** — in progress: pure state predicates now cover RX drain limits, FIFO completeness, TX progress states, and watchdog state classification.
5. **Runtime adopted blind behaviour** — in progress: pure polling, counter, direction, and position rules are extracted.
6. **Elero hub split** — in progress: runtime adopted blind management moved out of protocol dispatch into its own translation unit.

## Module status

### Packet parser

- **Files**: `components/elero/elero_packet_parser.h`, `components/elero/elero_protocol.cpp`, `tests/unit/test_packet_parser.cpp`
- **Interface**: `packet_parser::parse_fifo_packet(raw_fifo)`
- **Implementation owns**: length checks, destination parsing, CRC/LQI/RSSI extraction, payload decode, status/command classification.
- **Callers own**: logging, packet dump updates, counters, deduplication, scan state, RX queue dispatch.
- **Depth**: high — the Elero hub gets a parsed RF packet or a stable reject reason from one call.

### Group command policy

- **Files**: `components/elero/elero_group_command_logic.h`, `components/elero_group/EleroGroupCover.cpp`, `tests/unit/test_group_command_logic.cpp`
- **Interface**: small pure predicates for accepted/fallback behaviour and counter wrapping.
- **Implementation owns**: deciding when hub submission consumes the group counter and when member queues should become the retry path.
- **Depth**: medium — the policy is small, but it gives tests leverage over previously implicit failure behaviour.

### Command profile

- **Files**: `components/elero/elero_command_profile.h`, `components/elero/elero.h`, `components/elero_group/EleroGroupCover.cpp`
- **Interface**: `BlindCommandProfile` plus `command_profile::can_share_native_group()`.
- **Implementation owns**: the compatibility rule for native multi-destination RF packets.
- **Depth**: medium — Group cover no longer needs to know each individual RF getter for native group eligibility/building.

### Radio state logic

- **Files**: `components/elero/elero_radio_state_logic.h`, `components/elero/elero_cc1101.cpp`, `tests/unit/test_radio_state_logic.cpp`
- **Interface**: pure predicates for CC1101 MARCSTATE classification, RX drain priority, FIFO bounds, and complete-packet checks.
- **Implementation owns**: deciding whether a CC1101 state is TX-progress, watchdog-healthy, watchdog-transient, or idle-restartable.
- **Depth**: medium — hardware I/O stays in the Elero hub, while state interpretation becomes testable without a CC1101 radio.

### Runtime blind logic

- **Files**: `components/elero/elero_runtime_blind_logic.h`, `components/elero/elero_runtime.cpp`, `tests/unit/test_runtime_blind_logic.cpp`
- **Interface**: pure predicates and calculations for runtime polling, command counter wrapping, direction from Elero state, and dead-reckoned position updates.
- **Implementation owns**: behaviour rules for runtime adopted blind timing and movement.
- **Depth**: medium — the Elero hub still owns storage and locking, but runtime blind behaviour is no longer embedded directly in protocol dispatch.

### Runtime blind management

- **Files**: `components/elero/elero_runtime.cpp`, `components/elero/elero_protocol.cpp`
- **Interface**: existing Elero hub methods for adopt/remove/update/send runtime blinds.
- **Implementation owns**: runtime adopted blind storage loops, queue draining, polling, and position recompute orchestration.
- **Depth**: low-to-medium — the public hub Interface is unchanged, but locality improves because RF packet dispatch and runtime adopted blind management no longer share one file.

## Review notes

- Keep ESPHome/FreeRTOS dependencies out of pure logic Modules whenever possible so unit tests stay cheap.
- A new seam is only worth keeping when it has at least test leverage or multiple real callers.
- Prefer adding stable domain terms to `CONTEXT.md` as soon as a refactor names them.
