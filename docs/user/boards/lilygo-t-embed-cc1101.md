# Lilygo T-Embed CC1101

Der Lilygo T-Embed CC1101 ist ein ESP32-S3-Board mit integriertem CC1101. Der
CC1101 hängt an der internen SPI-Verdrahtung des Boards; die Standard-Pins aus
der allgemeinen Installationsanleitung für externe Module gelten hier nicht.

Eine vollständige Startkonfiguration mit allen folgenden Punkten liegt unter
[`../examples/lilygo-minimal.yaml`](../examples/lilygo-minimal.yaml).

## ESP32-Einstellungen

```yaml
esp32:
  board: esp32-s3-devkitc-1
  variant: esp32s3
  flash_size: 16MB
  framework:
    type: esp-idf
```

## Bewährte CC1101-Pins

```yaml
spi:
  clk_pin: GPIO11
  mosi_pin: GPIO9
  miso_pin: GPIO10

elero:
  cs_pin: GPIO12
  gdo0_pin: GPIO3
```

## Erforderliches Board-Setup

Bei manchen Lilygo T-Embed CC1101 müssen Board-Stromversorgung und
Antennen-/RF-Switch initialisiert werden, bevor der Onboard-CC1101 auf SPI
antwortet.

Dieses Setup in dieselbe YAML-Konfiguration übernehmen:

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

Gibt es in der Konfiguration bereits einen `esphome:`-Block, den
`on_boot:`-Abschnitt dort einfügen statt einen zweiten Block anzulegen.

## Speicher beim Kompilieren (ESP32-S3)

Bricht die Kompilierung mit `Killed signal terminated program cc1plus` ab, die
Kompilierung serialisieren:

```yaml
esphome:
  compile_process_limit: 1
```

Das lässt sich im selben `esphome:`-Block mit dem `on_boot:`-Setup kombinieren.

## Bootlog-Hinweis `Set flash_chip: generic ...`

Auf einem T-Embed CC1101 mit ESP-IDF und ESPHome 2026.11.0 wurde beim Start
dieser Hinweis beobachtet:

```text
[W][app:...]: Set flash_chip: generic under esp32 > framework > advanced to save IRAM
```

Das ist ein optionaler Optimierungsvorschlag, kein Fehler; Funkempfang und
Senden funktionieren unverändert. Wer ihn übernehmen will, ergänzt den
bestehenden `esp32:`-Block, ohne andere Einstellungen zu ändern:

```yaml
esp32:
  board: esp32-s3-devkitc-1
  variant: esp32s3
  flash_size: 16MB
  framework:
    type: esp-idf
    advanced:
      flash_chip: generic   # nur wenn der eigene Bootlog genau "generic" nennt
```

- Den Wert immer aus dem **eigenen** Bootlog übernehmen. Andere
  Board-Revisionen können einen anderen Flash-Chip haben.
- Erfordert ESPHome 2026.10.0 oder neuer.
- Auf einem T-Embed CC1101 (ESP-IDF, ESPHome 2026.11) angewendet; das Gerät
  läuft damit stabil und sendet/empfängt Elero-Pakete. Die Speicherersparnis
  wurde nicht vermessen.

Hintergrund und Regeln:
[ESP32: optionale Speicheroptimierungen](../configuration.md#esp32-optionale-speicheroptimierungen).

## Hinweise zu GPIO12

`GPIO12` ist auf dem Lilygo T-Embed CC1101 (ESP32-S3) der erwartete
CC1101-Chip-Select-Pin. Die Warnung zu GPIO12 und VDD_SDIO beim klassischen
ESP32 gilt für den ESP32-S3 so nicht.

Bei externen CC1101-Modulen an klassischen ESP32-Boards bleibt GPIO12 für
SPI-Signale problematisch und sollte gemieden werden.

## Fehlersuche bei `SPI Status: FAILED`

Wenn im Log steht:

```text
SPI Status: FAILED — CC1101 communication broken
```

ist der CC1101 noch nicht per SPI erreichbar. Auf dem Lilygo T-Embed CC1101 in
dieser Reihenfolge prüfen:

1. Board-Power- und Antennen-/RF-Switch-Setup von oben ergänzen.
2. Prüfen, dass die ESP32-Konfiguration ein ESP32-S3-Board und `variant: esp32s3` verwendet.
3. Prüfen, dass die SPI-Pins den bewährten Lilygo-Pins oben entsprechen.
4. Den vollständigen Bootlog sichern, inklusive der CC1101-Diagnosezeilen für
   `PARTNUM`, `VERSION` und die SPI-Schreib-/Lesetests.

Das alles erledigen, bevor du Frequenz, Blind-Adressen, Kanäle oder
Befehlswerte änderst; diese Einstellungen spielen erst eine Rolle, wenn SPI
funktioniert.
