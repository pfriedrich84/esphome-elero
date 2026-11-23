# Common Issues

Diese Seite sammelt typische Symptome beim Einrichten und die ersten Prüfungen,
bevor du Elero-Protokollwerte änderst.

## `SPI Status: FAILED — CC1101 communication broken`

Der ESP32 kann nicht per SPI mit dem CC1101 kommunizieren. Frequenz,
Blind-Adressen, Kanäle und Elero-Befehlswerte spielen an dieser Stelle noch
keine Rolle.

### Lilygo T-Embed CC1101

Für den Lilygo T-Embed CC1101 gibt es eigene Hinweise:
[`boards/lilygo-t-embed-cc1101.md`](boards/lilygo-t-embed-cc1101.md).

Typische Ursachen auf diesem Board:

- die Stromversorgung des Onboard-CC1101 ist nicht eingeschaltet
- die Antennen-/RF-Switch-Pins sind nicht initialisiert
- falsche ESP32-S3-Board-Variante oder falsches Pin-Mapping

Prüfe das Board-Power- und Antennen-Switch-Setup, bevor du SPI-Pins änderst.

### Externes CC1101-Modul

Bei einem separaten CC1101-Modul prüfen:

- VCC hängt an 3.3V, nicht an 5V
- GND ist verbunden
- CLK, MOSI, MISO und CS entsprechen der YAML-Konfiguration
- CS wird nicht mit einem anderen SPI-Gerät geteilt
- keine kritischen ESP32-Strapping-Pins für SPI-Signale

Auf klassischen ESP32-Boards GPIO0, GPIO2, GPIO5, GPIO12 und GPIO15 für
SPI-Signale meiden. GPIO12 ist besonders kritisch, weil es beim Boot VDD_SDIO
festlegt. Der ESP32-S3 hat andere Strapping-Pins; siehe die Board Notes zum
Lilygo T-Embed CC1101.

## Keine RF-Pakete beim Drücken der Fernbedienung

Funktioniert SPI, erscheinen beim Drücken der echten Elero-Fernbedienung aber
keine Pakete im Log, zuerst die Frequenz prüfen. `logger:` muss dafür auf
`DEBUG` stehen.

Gängige 868-MHz-Varianten:

| Variante | `freq2` | `freq1` | `freq0` |
|---|---:|---:|---:|
| Standard 868,35 MHz (Default) | `0x21` | `0x71` | `0x7a` |
| Alternative 868,95 MHz | `0x21` | `0x71` | `0xc0` |

Zuerst den Standard testen, dann die Alternative. Mit aktiviertem `elero_web`
lässt sich die Frequenz auch zur Laufzeit über die Web-API testen, siehe
[Discovery: Frequenz testen](discovery.md#frequenz-testen).

## ESP32-S3-Kompilierung bricht mit `Killed signal terminated program cc1plus` ab

ESP32-S3-Builds können bei paralleler Kompilierung zu wenig Arbeitsspeicher auf
dem Build-Rechner haben. Kompilierung serialisieren:

```yaml
esphome:
  compile_process_limit: 1
```

## `Duplicate sensor entity with name '... RSSI' found`

Covers und Lights erzeugen mit `auto_sensors: true` (Standard) bereits einen
RSSI-Sensor, einen Status-Text-Sensor und einen Refresh-Button. Zusätzliche
`sensor:`/`text_sensor:`-Einträge mit `platform: elero` und demselben Namen
kollidieren damit. Entweder die zusätzlichen Einträge entfernen oder am Cover
bzw. Light `auto_sensors: false` setzen. Details:
[Konfigurationsreferenz: `sensor`](configuration.md#plattform-sensor-rssi).

## `[minimum_chip_revision] is an invalid option for [esp32]`

`minimum_chip_revision` gehört unter `esp32: framework: advanced:`, nicht direkt
unter `esp32:`. Die Option ist optional; siehe
[ESP32: optionale Speicheroptimierungen](configuration.md#minimum_chip_revision-nur-klassischer-esp32).

## Bootlog: `Set flash_chip: ... to save IRAM`

Beispiel:

```text
[W][app:...]: Set flash_chip: generic under esp32 > framework > advanced to save IRAM
```

Das ist ein **optionaler Optimierungshinweis** von ESPHome 2026.10 und neuer,
kein Laufzeitfehler. Funkempfang, Reichweite und Elero-Protokoll sind davon
nicht betroffen; das Gerät kann ohne Änderung weiterlaufen. Wer die
Optimierung übernehmen will, setzt genau den im eigenen Bootlog genannten Wert,
siehe [ESP32: optionale Speicheroptimierungen](configuration.md#flash_chip).
`generic` nicht ungeprüft für andere Boards übernehmen.

## Schwaches Signal oder unzuverlässige Steuerung

- Den RSSI-Diagnosesensor beobachten.
- Werte über etwa `-70 dBm` sind in der Regel gut.
- Antenne neu ausrichten und von störenden Netzteilen fernhalten.
- Für europäische Elero-Installationen ein 868-MHz-CC1101-Modul verwenden.
- Auf eine stabile 3.3V-Versorgung von ESP32 und CC1101 achten.
