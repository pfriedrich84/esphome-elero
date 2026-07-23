# Discovery: Blind-Adressen und RF-Scan

Diese Seite beschreibt, wie du die Elero-Protokollwerte deiner echten
Fernbedienung ermittelst. Diese Werte brauchst du später in `cover:`, `light:`
oder `elero_group:` Konfigurationen.

## Voraussetzungen

- Die CC1101-SPI-Kommunikation funktioniert ohne `SPI Status: FAILED`.
- `logger:` steht mindestens auf `DEBUG`.
- Ein Dummy-Cover ist vorhanden, damit die Elero-Komponente kompiliert.
- Scan-Buttons oder die Web-UI sind aktiviert.

Wenn SPI noch nicht funktioniert, zuerst [Common Issues](common-issues.md) prüfen.

## Methode 1: Web-UI (empfohlen)

Aktiviere die optionale Elero-Web-UI:

```yaml
# Der HTTP-Server auf Port 80 wird automatisch geladen.
# Nicht web_server: verwenden – das aktiviert die Standard-UI unter / wieder.
elero_web:
```

Danach ist die Oberfläche unter `http://<device-ip>/elero` erreichbar.

Funktionen:

- RF-Scan starten und stoppen
- gefundene Geräte mit Adresse, Kanal, Remote, RSSI, Status und Hop anzeigen
- konfigurierte Covers mit Position und Status anzeigen
- Copy-Paste-fertige YAML-Konfiguration exportieren

### Web-UI zur Laufzeit deaktivieren

Optional kann die Web-UI über einen Switch aktiviert/deaktiviert werden:

```yaml
switch:
  - platform: elero_web
    name: "Elero Web UI"
    restore_mode: RESTORE_DEFAULT_ON
```

Wenn der Switch ausgeschaltet ist, antworten alle `/elero`-Endpoints mit HTTP
503. Das blockiert unerwünschten Zugriff, ohne die Firmware neu zu flashen.

## Methode 2: RF-Scan per Button und Log

Füge zunächst ein Dummy-Cover und Scan-Buttons hinzu:

```yaml
cover:
  - platform: elero
    blind_address: 0x000001
    channel: 1
    remote_address: 0x000001
    name: "Dummy"

button:
  - platform: elero
    name: "Elero Start Scan"
    scan_start: true
  - platform: elero
    name: "Elero Stop Scan"
    scan_start: false
```

Ablauf:

1. Flashe die Konfiguration.
2. Öffne den ESPHome-Log: `esphome logs elero-blinds.yaml`.
3. Drücke den Start-Scan-Button in Home Assistant.
4. Betätige die echte Elero-Fernbedienung: hoch, runter und stopp.
5. Drücke den Stop-Scan-Button.
6. Lies die entdeckten Geräte aus dem Log ab.

Beispielausgabe:

```text
[I][elero:xxx]: Discovered new device: addr=0xa831e5, remote=0xf0d008, ch=4, rssi=-52.0
[I][elero.button:xxx]: Stopped Elero RF scan. Discovered 1 device(s).
[I][elero.button:xxx]:   addr=0xa831e5 remote=0xf0d008 ch=4 rssi=-52.0 state=top seen=3
```

Die wichtigsten Werte sind:

| Log-Wert | YAML-Wert |
|---|---|
| `addr` | `blind_address` |
| `remote` | `remote_address` |
| `ch` | `channel` |

## Methode 3: Manuelle Log-Analyse

Wenn du die Rohpakete direkt auswerten willst, aktiviere `DEBUG`-Logs und drücke
eine Taste auf der echten Fernbedienung:

```text
rcv'd: len=29, cnt=45, typ=0x6a, typ2=0x00, hop=0x0a, syst=0x01, chl=09,
       src=0x908bef, bwd=0x908bef, fwd=0x908bef, #dst=01, dst=0xe039c9,
       rssi=-84.0, lqi=47, crc= 1,
       payload=[0x00 0x04 0x00 0x00 0x20 0x00 0x00 0x00 0x00 0x00]
```

Suche Pakete, in denen `src`, `bwd` und `fwd` identisch sind.

| Log-Feld | Konfiguration | Wert im Beispiel |
|---|---|---|
| `src` / `bwd` / `fwd` | `remote_address` | `0x908bef` |
| `dst` | `blind_address` | `0xe039c9` |
| `chl` | `channel` | `9` |
| `typ` | `pck_inf1` | `0x6a` |
| `typ2` | `pck_inf2` | `0x00` |
| `hop` | `hop` | `0x0a` |
| `payload[0]` | `payload_1` | `0x00` |
| `payload[1]` | `payload_2` | `0x04` |

Das fünfte Payload-Byte (`payload[4]`) enthält meist den Befehl:

- Hoch: `0x20`
- Runter: `0x40`
- Stopp: `0x10`

> Wichtig: Für die Standard-Auswertung sind nur Pakete mit `len=29` relevant.
> Pakete mit `len=27` sind interne Nachrichten.

## Frequenz testen

Wenn SPI funktioniert, aber keine Pakete empfangen werden, teste die gängigen
868-MHz-Frequenzvarianten:

```yaml
elero:
  freq0: 0x7a  # Standard 868.35 MHz
  freq1: 0x71
  freq2: 0x21
```

oder:

```yaml
elero:
  freq0: 0xc0  # Alternative 868.95 MHz
  freq1: 0x71
  freq2: 0x21
```

Weitere Hinweise stehen in [Common Issues](common-issues.md#no-rf-packets-when-pressing-the-remote).
