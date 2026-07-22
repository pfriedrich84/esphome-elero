# Lilygo T-Embed CC1101

The Lilygo T-Embed CC1101 is an ESP32-S3 board with an onboard CC1101 radio. The
CC1101 uses the board's internal SPI wiring; do not use the default external
ESP32 wiring from the generic installation guide.

## Known-good CC1101 pins

```yaml
spi:
  clk_pin: GPIO11
  mosi_pin: GPIO9
  miso_pin: GPIO10

elero:
  cs_pin: GPIO12
  gdo0_pin: GPIO3
```

## Required board setup

Some Lilygo T-Embed CC1101 boards require the board power rail and antenna/RF
switch to be initialized before the onboard CC1101 responds on SPI.

Add this setup to the same YAML configuration:

```yaml
esphome:
  on_boot:
    priority: 800
    then:
      - switch.turn_on: power_rail
      - switch.turn_off: ant_sw1
      - switch.turn_on: ant_sw0
      - delay: 150ms

switch:
  - platform: gpio
    pin: GPIO15
    id: power_rail
    restore_mode: ALWAYS_ON

  - platform: gpio
    pin: GPIO47
    id: ant_sw1
    restore_mode: ALWAYS_OFF
    internal: true

  - platform: gpio
    pin: GPIO48
    id: ant_sw0
    restore_mode: ALWAYS_ON
    internal: true
```

If your configuration already has an `esphome:` block, merge the `on_boot:`
section into the existing block instead of creating a second one.

## ESP32-S3 compile memory

If compilation fails with `Killed signal terminated program cc1plus`, serialize
the build:

```yaml
esphome:
  compile_process_limit: 1
```

This can be combined with the `on_boot:` setup in the same `esphome:` block.

## Notes about GPIO12

`GPIO12` is the expected CC1101 chip-select pin on the Lilygo T-Embed CC1101
ESP32-S3. The classic ESP32 warning about GPIO12 and VDD_SDIO does not apply in
the same way to ESP32-S3.

For external CC1101 modules on classic ESP32 boards, GPIO12 can still be unsafe
and should be avoided for SPI signals.

## Troubleshooting `SPI Status: FAILED`

If the log shows:

```text
SPI Status: FAILED — CC1101 communication broken
```

then the CC1101 is not reachable over SPI yet. On Lilygo T-Embed CC1101, check
in this order:

1. Add the board power and antenna/RF switch setup above.
2. Confirm the ESP32 configuration uses an ESP32-S3 board and variant.
3. Confirm the SPI pins match the known-good Lilygo pins above.
4. Capture the full boot log, including the CC1101 diagnostic lines for
   `PARTNUM`, `VERSION`, and SPI write/read tests.

Do this before changing Elero frequency, blind addresses, channels, or command
values; those settings only matter after SPI communication works.
