# ESPHome Elero Context

This context describes the Elero RF domain used by the ESPHome integration.

## Language

**Elero hub**:
The ESPHome module that owns the CC1101 radio and coordinates Elero RF devices.
_Avoid_: central component, service

**CC1101 radio**:
The sub-GHz transceiver used to send and receive Elero RF packets.
_Avoid_: radio module, RF chip

**RF packet**:
A raw Elero protocol message received from or transmitted through the CC1101 radio.
_Avoid_: message, frame when referring to the Elero wire format

**Packet parser**:
The module that interprets raw CC1101 FIFO bytes as Elero RF packet fields.
_Avoid_: protocol helper, decode utility

**Blind**:
An Elero-controlled shutter/cover device identified by a 24-bit blind address.
_Avoid_: roller shutter when naming code concepts

**Runtime adopted blind**:
A discovered Blind made controllable from the web UI without reflashing ESPHome YAML.
_Avoid_: dynamic blind, temporary blind

**Group cover**:
A Home Assistant cover entity that sends one command intent to multiple Blinds.
_Avoid_: cover group, group service

**Command intent**:
A requested Blind action such as up, down, stop, tilt, or check before it becomes one or more RF packets.
_Avoid_: action, command when ambiguity with RF packet bytes matters

**Command queue**:
The per-Blind retry path that holds command intents until the Elero hub can transmit them.
_Avoid_: TX queue when referring to per-Blind retries

**Command intent delivery**:
The per-device module that owns bounded semantic queueing and coalescing for one Blind or compatible Group cover delivery lane.
_Avoid_: dispatcher, raw command queue

**Delivery profile coordinator**:
The module that serializes Command intents and owns retries, stale aging, RF packet construction, and rolling counters for delivery lanes sharing one RF remote profile.
_Avoid_: global queue, hub scheduler

**Command profile**:
The RF identity and command-shaping data needed to turn a Blind command intent into an RF packet.
_Avoid_: RF params, config blob

**Radio state**:
The CC1101 MARCSTATE value interpreted as receive, transmit, calibration, FIFO failure, or idle behaviour.
_Avoid_: status when referring to CC1101 MARCSTATE

## Relationships

- An **Elero hub** owns exactly one **CC1101 radio**.
- A **CC1101 radio** receives and transmits **RF packets**.
- A **Packet parser** turns one raw **RF packet** into protocol fields.
- A **Group cover** contains two or more **Blinds**.
- A **Group cover** turns one **Command intent** into one native group RF packet or multiple Blind command intents.
- A **Group cover** can use a native group RF packet only when member **Command profiles** are compatible.
- A **Command queue** belongs to one **Command intent delivery** instance.
- Configured Blinds, lights, runtime adopted Blinds, and compatible Group covers each own a **Command intent delivery** instance.
- A **Delivery profile coordinator** orders all **Command intent delivery** instances sharing one RF remote profile.
- A **Delivery profile coordinator** is the sole rolling-counter and RF submission owner for its RF remote profile.
- Incompatible Group covers atomically fan out one semantic **Command intent** through member delivery instances.
- A **Command profile** belongs to one **Blind**.
- The **Elero hub** interprets **Radio state** before deciding whether to drain RX, advance TX, or recover the **CC1101 radio**.
- A **Runtime adopted blind** starts from RF discovery data and shares command, polling, and movement concepts with a configured **Blind**.

## Example dialogue

> **Dev:** "Should the **Elero hub** decide whether this raw FIFO read is a status packet?"
> **Domain expert:** "No — the **Packet parser** should classify the **RF packet**; the **Elero hub** decides what side effects follow."

## Flagged ambiguities

- "message" can mean a FreeRTOS queue item or an Elero **RF packet** — use **RF packet** for the Elero wire format.
