# Elero Remote Control Component for ESPHome

> Steuere Elero Rollläden und Lichter bidirektional via ESP32 + CC1101 direkt aus Home Assistant.
> Inklusive RF-Discovery, optionaler Web-UI mit YAML-Export, Diagnose-Sensoren,
> Positionssteuerung, Tilt, Gruppen und Licht-Unterstützung.

[![ESPHome](https://img.shields.io/badge/ESPHome-Component-blue)](https://esphome.io/)
[![License](https://img.shields.io/badge/License-GPLv3-green)](LICENSE)

---

## Features

| Feature | Status |
|---|---|
| Rollläden hoch/runter/stopp steuern | Stabil |
| Bidirektionale Kommunikation mit Statusfeedback | Stabil |
| RF-Discovery und Web-UI mit YAML-Export | Stabil |
| Positionssteuerung und Tilt/Kipp-Unterstützung | Stabil |
| RSSI-, Status- und Hub-Diagnose-Entities | Stabil |
| Gruppensteuerung mehrerer Rollläden | Stabil |
| Elero-Lichter schalten und optional dimmen | Beta |
| TempoTel 2 Kompatibilität | Getestet |

## Unterstützte Hardware

- **[Lilygo T-Embed CC1101](docs/user/boards/lilygo-t-embed-cc1101.md)** — ESP32-S3 mit integriertem CC1101, empfohlen.
- **ESP32/ESP32-S3 + externes CC1101-Modul** — freie SPI-Pinwahl, 868 MHz empfohlen.

Vollständige Hardware- und Flash-Anleitung: [Installation](docs/user/installation.md).

## Schnellstart

1. ESPHome YAML anlegen.
2. Board-spezifische Pins und ggf. Power-Setup eintragen.
3. Dummy-Cover und Scan-Buttons hinzufügen.
4. Flashen, RF-Scan starten und echte Fernbedienung betätigen.
5. Gefundene Werte in echte Cover-/Light-Konfiguration übernehmen.

Minimaler Einstieg für Lilygo T-Embed CC1101:

```yaml
esphome:
  name: elero-blinds
  friendly_name: "Elero Rollladen"

esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf

logger:
  level: DEBUG

api:

ota:
  - platform: esphome

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

external_components:
  - source: github://pfriedrich84/esphome-elero

spi:
  clk_pin: GPIO11
  mosi_pin: GPIO9
  miso_pin: GPIO10

elero:
  cs_pin: GPIO12
  gdo0_pin: GPIO3
```

> Lilygo T-Embed CC1101 benötigt auf manchen Boards zusätzlich Board-Power- und
> Antennen-Switch-Setup. Siehe [Lilygo Board Notes](docs/user/boards/lilygo-t-embed-cc1101.md).

Für externe CC1101-Module und vollständige Beispiele siehe
[Installation](docs/user/installation.md) und [example.yaml](example.yaml).

## Blind-Adressen ermitteln

Zum Auslesen von `blind_address`, `remote_address`, `channel` und optionalen
Protokollwerten gibt es drei Wege:

- Web-UI unter `/elero` mit YAML-Export
- RF-Scan per Home-Assistant-Buttons und ESPHome-Log
- manuelle Log-Analyse der Rohpakete

Details: [Discovery: Blind-Adressen und RF-Scan](docs/user/discovery.md).

## Konfiguration

Die vollständige YAML-Referenz liegt in [docs/user/configuration.md](docs/user/configuration.md):

- Hub: `elero`
- Covers: `cover: platform: elero`
- Lights: `light: platform: elero`
- Gruppen: `elero_group`
- RSSI- und Status-Entities
- Scan-Buttons
- Web-UI und REST-API
- Diagnose-Sensoren

## Home Assistant

Die ESPHome-API erzeugt Covers, Lights, Diagnose-Sensoren, Text-Sensoren und
Buttons direkt in Home Assistant. Dashboard- und Automation-Beispiele stehen in
[Home Assistant Integration](docs/user/home-assistant.md).

## Troubleshooting

Häufige Symptome und erste Checks stehen in [Common Issues](docs/user/common-issues.md),
z.B.:

- `SPI Status: FAILED — CC1101 communication broken`
- keine RF-Pakete beim Drücken der Fernbedienung
- ESP32-S3 Compile-OOM
- schwaches Signal oder unzuverlässige Steuerung

## Getestete Konfigurationen

| Board | CC1101 | Framework | Frequenz | Hinweise |
|---|---|---|---|---|
| **[Lilygo T-Embed CC1101](docs/user/boards/lilygo-t-embed-cc1101.md)** | Integriert (868 MHz) | ESP-IDF | 868 MHz | Empfohlen; Board-Power-/Antennen-Switch beachten |
| ESP32-DevKit V1 | Externes Modul | Arduino | 868 MHz | Gut; sichere SPI-Pins wählen |

## Dokumentation

- [docs/README.md](docs/README.md) — Dokumentationsindex
- [Installation](docs/user/installation.md) — Hardware, ESPHome Setup und Erstinstallation
- [Konfigurationsreferenz](docs/user/configuration.md) — vollständige YAML-Referenz
- [Discovery](docs/user/discovery.md) — RF-Scan, Log-Analyse, Web-UI und Frequenztest
- [Home Assistant Integration](docs/user/home-assistant.md) — Entities, Dashboard, Automationen
- [Common Issues](docs/user/common-issues.md) — Troubleshooting
- [Board Notes](docs/user/boards/README.md) — board-spezifische Hinweise
- [example.yaml](example.yaml) — kompaktes Beispiel
- [User Examples](docs/user/examples/README.md) — copy-paste-nahe YAML-Beispiele

## Credits

Dieses Projekt basiert auf der Arbeit von:

- [QuadCorei8085/elero_protocol](https://github.com/QuadCorei8085/elero_protocol) (MIT) — Verschlüsselungs-/Entschlüsselungsstrukturen
- [stanleypa/eleropy](https://github.com/stanleypa/eleropy) (GPLv3) — Fernbedienungs-Handling
- [andyboeh/esphome-elero](https://github.com/pfriedrich84/esphome-elero) — Grundlage für diese Steuerung, wobei dieses Repo ein nahezu vollständiger Rebuild ist
