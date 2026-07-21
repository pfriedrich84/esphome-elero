# Command intent delivery

## Status

Accepted

## Context

Configured Blinds, lights, runtime adopted Blinds, Group covers, buttons, polling, and the web API previously submitted raw command bytes through several queues and direct-send paths. Counter advancement, retry, stale aging, and STOP preemption therefore differed by caller. A partial native Group cover transmission also made unconditional member fallback unsafe.

## Decision

Use one internally thread-safe `CommandIntentDelivery` lane per configured device and runtime adopted Blind. The lane owns its bounded semantic queue and coalescing, and firmware callers timestamp each admitted intent with monotonic time. Coalescing never rewrites the coordinator's active sequence. All lanes with the same remote RF profile register with one `ProfileDeliveryCoordinator`, which exclusively owns cross-device ordering, repeat progress, failure budget/backoff, stale aging from admission or the latest completed transmission, packet construction, and the rolling counter. The hub advances coordinators with monotonic time and a short RF submission callback returning queue admission plus a transaction ID. Core 0 returns the matching actual RF completion or failure; queue admission alone never advances repeat progress, counters, or timed entity state.

`STOP` is urgent and replaces pending work. If it interrupts a partially accepted intent, that intent's counter is retired first. The hub admits at most one command packet radio-wide across the normal and priority FreeRTOS queues; after its actual completion, eligible STOP work is selected before normal work from any profile. Thus priority changes latency without allowing a newer packet to overtake an older physically queued command. `QUEUE_FULL` preserves state and consumes no failure budget. Final failure and stale clearing retire a counter only after partial acceptance.

A compatible Group cover owns a dedicated multi-destination lane on the same profile coordinator as its members. Compatibility includes the shared RF profile and semantic cover command mapping. An incompatible group admits the semantic intent atomically into all member lanes or changes none. A native final failure falls back through the coordinator only if zero repeats were accepted; partial native delivery never automatically fans out.

The web API, refresh/custom buttons, configured entities, and runtime entities submit semantic intents. Raw-byte delivery interfaces and their shallow policy modules are removed.

`elero_group.hide_members` defaults to `false`. When true, code generation marks every referenced member internal, globally hiding its individual Home Assistant entity even if another group also references it.

## Consequences

- Dependency-light lane and profile-coordinator modules define RF-safe delivery semantics and are unit-testable without ESPHome hardware.
- Submission is safe from both ESPHome loop and AsyncWebServer contexts.
- The Elero hub still owns shared FreeRTOS TX queues and all post-setup CC1101 SPI work remains on the radio task/Core 0.
- Native group fallback may deliberately report/drop a partially delivered command rather than risk duplicate member commands.
- `hide_members` is a global entity visibility choice, not visibility scoped to one group.

## Validation

The acceptance suite covers semantic mapping, coalescing, capacity, repeats, queue congestion, bounded failures, stale clearing, counter retirement, STOP preemption, multi-destination packets, and concurrent submission. Python tests cover Group cover bounds and `hide_members` code generation. The ESPHome fixture matrix includes a hidden-member Group cover.
