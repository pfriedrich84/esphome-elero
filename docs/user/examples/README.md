# User Examples

Diese Beispiele sind Startpunkte: WLAN-Secrets, Adressen und Namen an die eigene
Installation anpassen. Licht-Beispiele fehlen bewusst, bis der Licht-Workflow
ausführlicher dokumentiert und getestet ist.

## Vollständige Startkonfigurationen

| Beispiel | Einsatzzweck |
|---|---|
| [`lilygo-minimal.yaml`](lilygo-minimal.yaml) | Lilygo T-Embed CC1101 mit Onboard-Funk, Board-Power, Antennen-Switch, Platzhalter-Cover und Scan-Buttons |
| [`external-cc1101-minimal.yaml`](external-cc1101-minimal.yaml) | Klassischer ESP32 mit externem CC1101-Modul an sicheren SPI-Pins |

Beide Dateien erwarten eine `secrets.yaml` mit `wifi_ssid`, `wifi_password` und
`ap_password`.

Alle weiteren Dateien sind Ausschnitte und werden in eine funktionierende
Startkonfiguration (mit `spi:` und `elero:`) eingefügt.

## Discovery

| Beispiel | Einsatzzweck |
|---|---|
| [`discovery-web-ui.yaml`](discovery-web-ui.yaml) | RF-Discovery über die `/elero`-Web-UI mit YAML-Export |
| [`discovery-buttons.yaml`](discovery-buttons.yaml) | RF-Discovery über Home-Assistant-Buttons und ESPHome-Log |

## Feature-Snippets

| Beispiel | Einsatzzweck |
|---|---|
| [`cover-position.yaml`](cover-position.yaml) | Ein Rollladen mit zeitbasierter Positionssteuerung |
| [`cover-tilt.yaml`](cover-tilt.yaml) | Ein Raffstore / Rollladen mit Tilt-Funktion |
| [`group-cover.yaml`](group-cover.yaml) | Mehrere Elero-Covers als ein Home-Assistant-Cover gruppieren |
| [`diagnostics-custom.yaml`](diagnostics-custom.yaml) | Auto-generierte Diagnose-Entities durch eigene Namen ersetzen |

Siehe auch:

- [Installation](../installation.md)
- [Konfigurationsreferenz](../configuration.md)
- [Discovery](../discovery.md)
- [Common Issues](../common-issues.md)
