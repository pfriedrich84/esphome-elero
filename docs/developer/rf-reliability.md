# RF delivery and STOP reliability

This implementation follows the [read-only review at its immutable revision](https://github.com/pfriedrich84/esphome-elero/blob/1ca6cf755cea05f619c1d10efbcc2d1db280d955/docs/developer/reviews/2026-09-08-rf-reliability-astra-gpt-6.md).

## Ownership and causal STOP confirmation

- Core 0 owns SPI, packet capture, receive sequence/epoch allocation and local TX completions.
- `ProfileDeliveryCoordinator` owns ordering, counters and bounded packet repetitions/retries.
- `EleroCover` owns motor-state interpretation and STOP verification. A UI IDLE estimate is not proof of physical standstill.

`RxMetadata` accompanies a status before it crosses the result queue: monotonic 64-bit capture sequence, radio receive epoch/capture time, source, forwarding addresses, destinations, channel, counter, type and hop. Capture time is the Core-0 FIFO observation (or the earlier observation of its prefix), not an exact over-air packet-end timestamp. No 64-bit atomic operations run in the GPIO ISR. Before reporting successful local TX, Core 0 captures already buffered packets and fences the receive timeline. A buffered prefix must retain its original epoch when completed later. This intentionally conservative fence can exclude a very early response; the bounded CHECK path can observe a later state. Entity-dispatch time never creates freshness.

The first successful STOP transmission carries that cutoff through the transaction completion and coordinator outcome. Only a later receive sequence/epoch for the configured motor may affect verification. Packet counters are not assumed to echo the transmitted counter, and a spontaneous fresh state observation is not called a protocol ACK.

## Common STOP entry and outcomes

`EleroCover::request_stop()` is the common entry for ESPHome calls, position-triggered STOP, semantically mapped custom buttons, web intents and already-admitted group STOPs. It records the position/trigger, blocks movement, requests priority delivery and handles rejection. Group admission remains in the existing group/coordinator path; its member preparation invokes the same entry without duplicating an already admitted packet.

- `stop_queued`: admitted, not yet locally transmitted.
- `stop_verifying`: local TX succeeded; standstill is not established.
- `stop_confirmed`: fresh STOPPED, TOP, BOTTOM, INTERMEDIATE, TILT, TOP_TILT or BOTTOM_TILT observation for the correct motor.
- `stop_motor_blocking` / `stop_motor_overheated`: explicit terminal motor fault, not successful user STOP.
- `stop_failed`: bounded local delivery/verification failed.

UNKNOWN, TIMEOUT and unknown raw values do not confirm STOP. Fresh movement may request the existing one additional burst, only after the initial burst finishes. Missing feedback uses one CHECK per burst, at the existing two-second interval. Repeated user requests during the same verification do not replenish its budget. Physical overshoot is not inferred from RX/dispatch delay.

Only STOP bypasses a lane's three-second failure cooldown; the cooldown is not erased for OPEN/CLOSE/CHECK. A selected STOP burst is never truncated by another lane's STOP. Native group movement is ineligible while any referenced member verifies STOP, including movement admitted before the block was established.

## FIFO, CCA and RX opportunities

With unchanged `PKTCTRL1=0x8c` (CRC_AUTOFLUSH), consuming even a length byte during active reception is unsafe. `RxFifoReader` waits for packet end, verifies IDLE and reads each packet separately from a frozen <=64-byte snapshot. Complete packet + in-flight prefix is left entirely in hardware until the following packet ends; nothing is concatenated in software across a CRC autoflush. A stuck GDO has a 20 ms budget. If a reception starts in the GPIO→SIDLE race, complete predecessors are preserved and the interrupted tail is explicitly counted/discarded. Overflow is not reconstructable. This bounded freeze strategy has a receive blind interval; its duration and overflow behavior under back-to-back long frames require hardware measurement.

Buffered data is processed before new TX and on completion/cooldown, rather than sacrificing it to TX-first scheduling. TX preparation verifies IDLE, clears/loads TX only, issues SRX, listens for at least 1 ms and checks RX/CCA before STX. The hardware therefore actually applies unchanged `MCSM1=0x3f` CCA mode. A busy/rejected channel retains RX ownership and loaded TX bytes during backoff, without SFRX. Backoff is 2–19 ms with deterministic per-attempt jitter, bounded by five busy decisions or 50 ms; it does not add unbounded coordinator retries. STOP priority never bypasses CCA.

`send_delay` is measured from successful local completion, survives intent changes, and applies across profiles through the hub. Normal traffic also leaves a minimum 2 ms RX opportunity when `send_delay=0ms`. STOP bypasses spacing, not receive safety, CCA or bounded failure backoff. A zero timestamp and millis wrap are valid.

## Ordinary delivery is explicitly unconfirmed

`DeliveryEvent::COMPLETED` means that the local packet plan completed. `MotorDeliveryEvidence::LOCAL_TX_UNCONFIRMED` deliberately has no ACK/success counterpart. OPEN/CLOSE/TILT and ordinary light commands publish/log `delivery_unconfirmed` on their first local TX; a local plan failure is `delivery_failed`. Runtime-adopted ordinary commands log the same distinction. Timed movement/dimming remains an estimate anchored to actual local completion, not proof that the motor received anything. A later raw state is an independent observation, not an invented echoed command-counter ACK. No extra ordinary repeats or response-triggered resend loop is introduced.

## Counter resynchronization

`CounterState` separately records the last **accepted** counter/time and all observed activity. Rejections never postpone the accepted-frontier age. Normal modulo-256 forward progress remains valid; equal counters never resync. After at least 30 seconds without accepted progress, resync additionally requires at least three distinct advancing candidates spanning >=1 second, with each step <=16, inter-candidate gap <=10 seconds and chain age <=30 seconds. Out-of-order/expired candidates restart the chain; duplicates do not extend it. Thus reset 100→1…6 at five-second intervals recovers at 30 seconds instead of being starved indefinitely. This is conservative protocol filtering, **not cryptographic replay protection**: a deliberately paced, coherent replay stream cannot be distinguished without protocol authentication.

## Dynamic status and local completion

MARCSTATE, RXBYTES, TXBYTES and PKTSTATUS use up to five SPI reads to obtain two consecutive identical full bytes. FIFO error bits and any observed TX-underflow state are retained, not masked into an empty FIFO. Inconclusive reads fail closed. Normal completion requires observed TX/TX_END, stable empty TXBYTES without error flags, and the expected RX state; GDO falling, arbitrary non-TX state, IDLE or a counter increment alone is not success. An unobservable short TX may conservatively fail/retry. Recovery strobes require verified IDLE; failed preparation returns to RX rather than leaving it stranded in IDLE.

The CC1101 sources for CRC_AUTOFLUSH, CCA, FIFO state restrictions and dynamic status reads are the [TI datasheet SWRS061I](https://www.ti.com/lit/ds/swrs061i/swrs061i.pdf) and [TI errata SWRZ020E](https://www.ti.com/lit/er/swrz020e/swrz020e.pdf). RadioLib 7.1.2 remains the existing dependency. No PATABLE, frequency, modem, bandwidth, deviation, AGC or calibration register values change.

## Validation boundary

The host entity tests compile the complete production cover/group method bodies and production declarations, substituting only framework include paths and ESPHome/queue boundaries. Causal timelines, lane/coordinator logic and state transitions are production code. These tests do not validate RF propagation, CC1101 electrical timing or motor mechanics. Register access and FIFO/CCA behavior require separate radio tests and hardware validation.
