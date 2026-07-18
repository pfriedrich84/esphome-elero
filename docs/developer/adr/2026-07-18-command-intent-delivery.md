# Command intent delivery

## Status

Accepted

## Context

Configured Blinds, lights, runtime adopted Blinds, Group covers, buttons, polling, and the web API previously submitted raw command bytes through several queues and direct-send paths. Counter advancement, retry, stale aging, and STOP preemption therefore differed by caller. A partial native Group cover transmission also made unconditional member fallback unsafe.

## Decision

Use one internally thread-safe `CommandIntentDelivery` instance per configured device and runtime adopted Blind. The queue contains semantic intents and the module owns coalescing, repeat progress, failure budget/backoff, stale aging, packet construction, and the rolling counter. Callers provide monotonic time and a short, non-reentrant RF submission callback returning `OK`, `QUEUE_FULL`, or `FAILED`.

`STOP` is urgent and replaces pending work. If it interrupts a partially accepted intent, that intent's counter is retired first. `QUEUE_FULL` preserves state and consumes no failure budget. Final failure and stale clearing retire a counter only after partial acceptance.

A compatible Group cover owns a dedicated multi-destination delivery instance. Compatibility includes the shared RF profile and semantic cover command mapping. An incompatible group fans the semantic intent into member delivery instances. A native final failure fans out only if zero repeats were accepted; partial native delivery never automatically fans out.

The web API, refresh/custom buttons, configured entities, and runtime entities submit semantic intents. Raw-byte delivery interfaces and their shallow policy modules are removed.

`elero_group.hide_members` defaults to `false`. When true, code generation marks every referenced member internal, globally hiding its individual Home Assistant entity even if another group also references it.

## Consequences

- One dependency-light module defines RF-safe delivery semantics and is unit-testable without ESPHome hardware.
- Submission is safe from both ESPHome loop and AsyncWebServer contexts.
- The Elero hub still owns shared FreeRTOS TX queues and all post-setup CC1101 SPI work remains on the radio task/Core 0.
- Native group fallback may deliberately report/drop a partially delivered command rather than risk duplicate member commands.
- `hide_members` is a global entity visibility choice, not visibility scoped to one group.

## Validation

The acceptance suite covers semantic mapping, coalescing, capacity, repeats, queue congestion, bounded failures, stale clearing, counter retirement, STOP preemption, multi-destination packets, and concurrent submission. Python tests cover Group cover bounds and `hide_members` code generation. The ESPHome fixture matrix includes a hidden-member Group cover.
