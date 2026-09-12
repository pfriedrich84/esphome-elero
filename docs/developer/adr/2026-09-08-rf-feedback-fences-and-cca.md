# RF feedback fences, packet ownership and real CCA

- Date: 2026-09-08
- Status: accepted for implementation; hardware qualification outstanding
- Extends: [command-intent delivery](2026-07-18-command-intent-delivery.md)

## Context

Queued/pre-buffered status could falsely confirm STOP, RX bulk reads could consume following packets, and SIDLE→STX bypassed the configured CC1101 CCA. Neither local TX completion nor software position estimates establish physical motor delivery.

## Decision

Keep existing ownership: Core 0 owns the radio, profile coordinators own ordering/counters/repeats, and covers own STOP verification. Carry radio-allocated RX sequence/epoch and a local-TX cutoff through the existing queues. Unify configured-cover STOP entry and block native movement while any member verifies STOP.

Keep CRC_AUTOFLUSH and all RF tuning unchanged. Do not consume even a length byte in live RX: wait for packet end with a bounded deadline, freeze in verified IDLE, read packetwise and count any interrupted trailing fragment explicitly. Use RX→STX after listening, with bounded CCA backoff that retains RX ownership. Persist completion-based normal spacing across intents/profiles, exempting priority STOP but not CCA.

Ordinary commands remain explicitly unconfirmed rather than inventing a command ACK or adding blind repeats. Counter resync uses accepted-frontier age plus multiple advancing candidates. Dynamic status reads stabilize boundedly; TX underflow and unknown completion states fail closed.

## Alternatives and consequences

- Reading a prefix into software during active CRC_AUTOFLUSH was rejected: a later CRC failure can rewrite hardware FIFO pointers and splice unrelated frames.
- Changing RXOFF_MODE, disabling CRC_AUTOFLUSH, RF power/bandwidth tuning and broad retry increases are outside this change. The chosen freeze method has a measurable receive blind interval and cannot reconstruct a FIFO already erased/overflowed in hardware.
- Exact GDO packet-end timestamps and authenticated response correlation are not claimed. Conservative fences/reads can exclude an early response or miss an unobservably short successful TX; bounded verification/retries remain necessary.
- A coherent paced RF replay is not defeated by an unauthenticated rolling counter.

See [behavior and validation boundaries](../rf-reliability.md) and the [implementation report](../reviews/2026-09-08-rf-reliability-implementation.md).
