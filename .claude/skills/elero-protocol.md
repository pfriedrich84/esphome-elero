---
name: elero-protocol
description: "Quick reference for Elero RF protocol: packet structure, encryption, state bytes, command bytes. Use when working on RF code or debugging packets."
user-invocable: true
---

# Elero RF Protocol Reference

## Packet Structure (27-30 bytes typical)

```
Offset  Field           Size    Notes
0       length          1       Total packet bytes (excluding length byte itself)
1       counter         1       Incrementing command counter (1-255, wraps to 1)
2       typ (pck_inf1)  1       0x6a/0x69 = command, 0xca/0xc9 = status response
3       typ2 (pck_inf2) 1       Usually 0x00
4       hop             1       Hop counter (default 0x0a)
5       syst            1       System address (always 0x01)
6       chl             1       RF channel number
7-9     src             3       Source address (sender, 3 bytes big-endian)
10-12   bwd             3       Backward address
13-15   fwd             3       Forward address
16      num_dests       1       Number of destination addresses
17-19+  dests           N*3     Destination addresses (3 bytes each)
N+0     payload[0]      1       Usually 0x00
N+1     payload[1]      1       Usually 0x04
N+2..9  encrypted       8       Encrypted payload (see encryption below)
last-1  rssi_raw        1       Appended by CC1101 (not part of RF packet)
last    lqi_crc         1       Appended by CC1101 (LQI + CRC status)
```

## Encryption/Decryption Chain

**Encode** (before TX): `calc_parity → add_r20(0xFE) → xor_2byte → encode_nibbles`
**Decode** (after RX):  `decode_nibbles → sub_r20(0xFE) → xor_2byte → sub_r20(0xBA)`

Lookup tables (nibble substitution):
```
encode: [0x08, 0x02, 0x0d, 0x01, 0x0f, 0x0e, 0x07, 0x05, 0x09, 0x0c, 0x00, 0x0a, 0x03, 0x04, 0x0b, 0x06]
decode: [0x0a, 0x03, 0x01, 0x0c, 0x0d, 0x07, 0x0f, 0x06, 0x00, 0x08, 0x0b, 0x0e, 0x09, 0x02, 0x05, 0x04]
```

Crypto constants: `MULT = 0x708f`, `MASK = 0xffff`

## Command Bytes (payload[4] in encrypted block)

| Byte | Command | Cover | Light |
|------|---------|-------|-------|
| 0x00 | CHECK   | Status query | Status query |
| 0x10 | STOP    | Stop motor | Stop dimming |
| 0x20 | UP / ON | Open cover | Turn on |
| 0x24 | TILT    | Tilt/ventilate | — |
| 0x40 | DOWN / OFF | Close cover | Turn off |
| 0x44 | INT     | Intermediate | — |

## State Bytes (in status response)

| Byte | State | Description |
|------|-------|-------------|
| 0x00 | UNKNOWN | No state received yet |
| 0x01 | TOP | Fully open (upper end position) |
| 0x02 | BOTTOM | Fully closed (lower end position) |
| 0x03 | INTERMEDIATE | Between endpoints |
| 0x04 | TILT | Tilt/ventilate position |
| 0x05 | BLOCKING | Motor blocked (error) |
| 0x06 | OVERHEATED | Motor overheated (error) |
| 0x07 | TIMEOUT | Communication timeout (error) |
| 0x08 | START_MOVING_UP | Beginning upward movement |
| 0x09 | START_MOVING_DOWN | Beginning downward movement |
| 0x0a | MOVING_UP | Moving upward |
| 0x0b | MOVING_DOWN | Moving downward |
| 0x0d | STOPPED | Stopped at intermediate |
| 0x0e | TOP_TILT | Top position, tilted |
| 0x0f | BOTTOM_TILT / OFF | Bottom tilted (covers) / Off (lights) |
| 0x10 | ON | Light on |

## RSSI Calculation

```
if (rssi_raw > 127):  rssi_dBm = (int8_t)(rssi_raw) / 2.0 + (-74)
else:                 rssi_dBm = rssi_raw / 2.0 + (-74)
```

## Frequency Registers → MHz

```
MHz = (26.0 / 65536.0) * ((freq2 << 16) | (freq1 << 8) | freq0)
```

| Variant | freq2 | freq1 | freq0 | MHz |
|---------|-------|-------|-------|-----|
| Standard EU | 0x21 | 0x71 | 0x7a | 868.35 |
| Alternative | 0x21 | 0x71 | 0xc0 | 868.95 |

## Key Files

- Encryption: `components/elero/elero_protocol.cpp:24-163`
- Packet parsing: `components/elero/elero_protocol.cpp:168-321`
- TX state machine: `components/elero/elero_cc1101.cpp:125-181`
- Register access: `components/elero/elero_cc1101.cpp`
- Constants: `components/elero/elero.h:70-134`
