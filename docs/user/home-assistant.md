# Home Assistant Integration

Nach dem Flashen erscheint das ESPHome-Gerät automatisch in Home Assistant,
wenn `api:` konfiguriert ist. Dieser ESPHome-Weg bleibt der empfohlene
Standardpfad: RF-Steuerung, Statusfeedback, Discovery und Diagnose laufen
firmware-nah auf dem ESP32 + CC1101.

## Typische Entities

| Entity-Typ | Beispiel | Beschreibung |
|---|---|---|
| Cover | `cover.schlafzimmer` | Hoch, runter, stopp, optional Position und Tilt |
| Cover-Gruppe | `cover.alle_rolllaeden` | Steuert mehrere Covers gleichzeitig |
| Light | `light.wohnzimmerlicht` | Ein/Aus, optional Helligkeit |
| Sensor | `sensor.schlafzimmer_rssi` | Signalstärke in dBm, auto-generiert |
| Text Sensor | `text_sensor.schlafzimmer_status` | Aktueller Status, auto-generiert |
| Button | `button.schlafzimmer_refresh` | Sofortige Statusabfrage, auto-generiert |
| Button | `button.elero_start_scan` | RF-Scan starten |
| Button | `button.elero_stop_scan` | RF-Scan stoppen |

## Dashboard-Karte

Ein einfaches Entities-Dashboard:

```yaml
type: entities
title: Elero Rollläden
entities:
  - entity: cover.schlafzimmer
  - entity: sensor.schlafzimmer_rssi
  - entity: text_sensor.schlafzimmer_status
  - type: divider
  - entity: button.elero_start_scan
  - entity: button.elero_stop_scan
```

## Diagnose-Entities

Bei `auto_sensors: true` erzeugt der Hub automatisch Diagnose-Sensoren für
Frequenz, RX/TX-Zähler, Watchdog-Recoveries, Drop-Zähler und Latenzen.

Covers und Lights erzeugen standardmäßig:

- RSSI-Sensor
- Status-Text-Sensor
- Refresh-Button

Details und Override-Möglichkeiten stehen in der
[Konfigurationsreferenz](configuration.md#hub-diagnose-sensoren).

## Automation-Beispiel

Status-Text-Sensoren können für Automationen genutzt werden:

```yaml
automation:
  - alias: "Warnung bei Rollladen-Blockade"
    trigger:
      - platform: state
        entity_id: text_sensor.schlafzimmer_status
        to: "blocking"
    action:
      - service: notify.notify
        data:
          message: "Rollladen Schlafzimmer ist blockiert!"
```

## Gruppen in Home Assistant

`elero_group:` erzeugt ein eigenes Cover-Entity für mehrere Elero-Covers. Die
Mitglieder bleiben standardmäßig sichtbar. Mit `hide_members: true` können sie
als einzelne Home-Assistant-Entities ausgeblendet werden.

Details: [Konfigurationsreferenz: `elero_group`](configuration.md#plattform-elero_group-gruppensteuerung).

## Web-UI neben Home Assistant

Die optionale Elero-Web-UI ist kein Ersatz für die ESPHome-API. Sie ergänzt Home
Assistant vor allem für Discovery, Diagnose und YAML-Export.

Details: [Discovery und Web-UI](discovery.md#methode-1-web-ui-empfohlen).
