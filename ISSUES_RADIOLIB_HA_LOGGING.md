# Proposed Issues: RadioLib, HA Integration & Logging

Extends `IMPROVEMENT_PLAN.md` (Issues 1–10). Cross-references noted where applicable.

**Direktiven:** Keine Breaking Changes, keine Instabilitaeten.

---

## Bereich A: RadioLib-Integration vertiefen

### Issue R1: `SPIwriteRegisterBurst()` fuer PATABLE- und TX-FIFO-Schreibvorgaenge nutzen

**Priority:** P2 | **Effort:** S (1h) | **Labels:** `radiolib`, `reliability`

**Problem:** `write_burst()` (elero.cpp:558–567) nutzt direkte SPI-Transfers ohne Fehlerpruefung. RadioLib bietet `SPIwriteRegisterBurst()` mit konsistenter Fehlerbehandlung.

**Betroffene Dateien:**
- `components/elero/elero.cpp:558–567` (write_burst)
- `components/elero/elero.cpp:537` (PATABLE-Schreibvorgang in init())

**Fix:**
- `write_burst()` intern auf `radio_module_->SPIwriteRegisterBurst(addr, data, len)` umstellen
- Bei Fehler loggen: `if (rc != RADIOLIB_ERR_NONE) ESP_LOGW(TAG, "Burst write 0x%02x failed: %d", addr, rc);`
- Wrapper-Signatur `write_burst()` fuer Aufrufer beibehalten

**Ueberschneidung:** Teilweise mit IMPROVEMENT_PLAN Issue 2 (SPI-Fehlerbehandlung). Kann zusammen umgesetzt werden.

---

### Issue R2: RadioLib `SPIsendCommand()` fuer Command-Strobes mit Fehlerrueckmeldung

**Priority:** P2 | **Effort:** S (1h) | **Labels:** `radiolib`, `reliability`

**Problem:** `write_cmd()` (elero.cpp:569–576) nutzt direkte SPI-Transfers fuer Command-Strobes (SRES, SIDLE, SRX, STX, SFRX, SFTX). Der Kommentar in Zeile 571 vermerkt, dass RadioLib's `SPIsendCommand()` auf der CC1101-Klasse liegt — `radio_` (die CC1101-Instanz) ist aber verfuegbar.

**Betroffene Dateien:**
- `components/elero/elero.cpp:569–576`

**Fix:**
- Pruefen ob `radio_->SPIsendCommand(cmd)` oeffentlich zugaenglich ist (CC1101 public method)
- Falls ja: raw SPI durch RadioLib-Aufruf ersetzen fuer bessere Fehlerdiagnostik
- Falls nicht direkt verfuegbar: raw SPI beibehalten, aber Status-Byte nach Strobe validieren (CC1101 liefert Status bei jedem Strobe)

---

### Issue R3: MHz-Anzeige in `reinit_frequency()` und Web-API

**Priority:** P3 | **Effort:** S (30min) | **Labels:** `radiolib`, `usability`

**Problem:** `reinit_frequency()` (elero.cpp:460–469) loggt nur rohe Registerwerte (Hex). Nutzer koennen die tatsaechliche Frequenz in MHz nicht direkt ablesen. Gleiches gilt fuer den Web-API GET-Endpunkt `/elero/api/frequency`.

**Betroffene Dateien:**
- `components/elero/elero.cpp:460–469`
- `components/elero_web/elero_web_server.cpp` (frequency GET handler)

**Fix:**
- Helper-Funktion: `float registers_to_mhz(uint8_t freq2, uint8_t freq1, uint8_t freq0)`
  - Formel: `f_mhz = (26.0f / 65536.0f) * ((freq2 << 16) | (freq1 << 8) | freq0)`
- Log: `"CC1101 re-initialised: %.2f MHz (0x%02x 0x%02x 0x%02x)"`
- Web-API JSON-Response um `"mhz": 868.35` erweitern

---

### Issue R4: `radio_->setFrequency()` als MHz-basierte Alternative evaluieren

**Priority:** P3 | **Effort:** S (1h, Investigation) | **Labels:** `radiolib`, `investigation`

**Problem:** RadioLib's `CC1101::setFrequency(float freq)` berechnet Register intern und validiert den Frequenzbereich. Aktuell ungenutzt, da die Web-API rohe Register sendet.

**Betroffene Dateien:**
- `components/elero/elero.cpp:498–505`
- `components/elero_web/elero_web_server.cpp` (frequency-set Endpoint)

**Fix:**
- Alternativen Web-API-Endpoint: `POST /elero/api/frequency/set_mhz` mit `{"mhz": 868.35}`
- Intern `radio_->setFrequency(mhz)` + vollstaendiges Reinit aufrufen
- Bestehenden Register-Endpoint fuer Power-User beibehalten
- Web-UI nutzerfreundlicher: MHz-Eingabe statt Hex-Register

---

## Bereich B: Home-Assistant-Integration haerten

### Issue H1: Light — RSSI- und Status-Sensoren automatisch generieren (Paritaet mit Covers)

**Priority:** P1 (Important) | **Effort:** S (2h) | **Labels:** `ha-integration`, `usability`

**Problem:** Covers generieren via `auto_sensors: true` (cover/__init__.py) automatisch RSSI- und Status-Text-Sensoren. Lights haben kein Aequivalent — Nutzer koennen RSSI und Status von Lichtempfaengern nicht in HA ueberwachen, ohne manuell separate Sensor-Entities anzulegen.

**Betroffene Dateien:**
- `components/elero/light/__init__.py` — `auto_sensors`-Option + Sensor-Injektion hinzufuegen
- `components/elero/light/EleroLight.h` — Sensor-Registrierungsmethoden
- `components/elero/elero.cpp` — Hub muss RSSI/Status an Light-Sensoren weiterleiten

**Fix:**
- `_auto_sensor_validator()`-Pattern aus `cover/__init__.py` spiegeln
- `AUTO_LOAD = ["sensor", "text_sensor"]` zur Light-Platform hinzufuegen
- RSSI- und Text-Sensoren pro Light-Adresse im Hub registrieren

---

### Issue H2: Light — Duplikat-Adress-Validierung (Paritaet mit Covers)

**Priority:** P2 | **Effort:** S (1h) | **Labels:** `ha-integration`, `robustness`

**Problem:** Cover-Platform hat `_final_validate()` (cover/__init__.py:138–161) zur Erkennung doppelter Blind-Adressen. Light-Platform hat keine solche Pruefung. Zwei Lights mit derselben Adresse verursachen konfligierende RF-Befehle.

**Betroffene Dateien:**
- `components/elero/light/__init__.py`

**Fix:**
- `FINAL_VALIDATE_SCHEMA` mit Duplikat-Erkennung hinzufuegen, analog zum Cover-Pattern
- Cross-Platform-Validierung erwaegen (gleiche Adresse als Cover UND Light)

**Ueberschneidung:** Erweitert IMPROVEMENT_PLAN Issue 9.

---

### Issue H3: Cover — Fehlerzustaende (BLOCKING, OVERHEATED, TIMEOUT) in HA sichtbar machen

**Priority:** P2 | **Effort:** M (3h) | **Labels:** `ha-integration`, `usability`

**Problem:** Bei BLOCKING, OVERHEATED oder TIMEOUT (EleroCover.cpp:241–252) loggt der Code Warnungen, setzt aber `COVER_OPERATION_IDLE` in HA. Nutzer koennen keine HA-Automationen erstellen, die auf diese Fehlerzustaende reagieren (z.B. Alarm bei ueberhitztem Motor).

**Betroffene Dateien:**
- `components/elero/cover/EleroCover.cpp:241–252`
- `components/elero/cover/EleroCover.h`

**Fix (empfohlen: Option C):**
- Text-Sensor publiziert bereits den Roh-Status — sicherstellen, dass menschenlesbare Strings veroeffentlicht werden ("blocking", "overheated", "timeout" statt Hex-Codes)
- Dokumentation/Beispiele fuer HA-Automationen ergaenzen, die auf Text-Sensor-Aenderungen triggern
- Optional: Zusaetzlichen `binary_sensor` pro Cover fuer "error"-Zustand mit Attributen fuer spezifischen Fehlertyp

---

### Issue H4: Cover — Stop-Verify-Erschoepfung an HA melden

**Priority:** P2 | **Effort:** S (1h) | **Labels:** `ha-integration`, `robustness`

**Problem:** Nach `ELERO_STOP_VERIFY_MAX_RETRIES` (3) fehlgeschlagenen Stop-Verifizierungen (EleroCover.cpp:81–85) loggt der Code eine Warnung, meldet das Cover aber als idle an HA. Ein Motor der nicht gestoppt hat, bewegt sich weiter, waehrend HA ihn als stationaer anzeigt.

**Betroffene Dateien:**
- `components/elero/cover/EleroCover.cpp:70–86`

**Fix:**
- Nach Retry-Erschoepfung Text-Sensor-Status auf "stop_failed" oder "unresponsive" setzen
- Optional Position auf NAN/unknown setzen um Unsicherheit anzuzeigen
- WARN-Logging beibehalten (bereits vorhanden)

---

### Issue H5: Cover/Light — Feedback bei Command-Queue-Ueberlauf

**Priority:** P2 | **Effort:** S (1h) | **Labels:** `ha-integration`, `robustness`

**Problem:** Bei voller Command-Queue (ELERO_MAX_COMMAND_QUEUE=10) werden neue Befehle stillschweigend verworfen (nur Log-Warnung). HA-Automationen haben kein Feedback, dass der Befehl abgelehnt wurde.

**Betroffene Dateien:**
- `components/elero/cover/EleroCover.cpp`
- `components/elero/light/EleroLight.cpp`

**Fix:**
- Text-Sensor-Status auf "queue_full" setzen bei verworfenem Befehl
- Automatisch zuruecksetzen nach erfolgreichem Queue-Drain
- Optional: Queue-Tiefe als diagnostischen Sensor exponieren (siehe H7)

---

### Issue H6: Cover — millis()-Wraparound-Schutz in Positionstracking

**Priority:** P2 | **Effort:** S (1h) | **Labels:** `ha-integration`, `robustness`

**Problem:** `recompute_position()` (EleroCover.cpp:416–444) berechnet `now - last_recompute_time_`. Bei millis()-Wraparound (~49 Tage) kann die Float-Konvertierung von sehr grossen uint32_t-Werten unerwartete Ergebnisse liefern.

**Betroffene Dateien:**
- `components/elero/cover/EleroCover.cpp:416–444`

**Fix:**
- Subtraktion explizit als `uint32_t` casten: `uint32_t elapsed = now - last_recompute_time_;`
- Sanity-Check: wenn `elapsed > ELERO_TIMEOUT_MOVEMENT`, Position-Recompute ueberspringen oder clampen

**Ueberschneidung:** Verwandt mit IMPROVEMENT_PLAN Issue 8.

---

### Issue H7: Diagnostische Sensoren fuer Radio-Health und TX-Queue

**Priority:** P3 | **Effort:** M (3h) | **Labels:** `ha-integration`, `usability`

**Problem:** Nutzer haben keine HA-Sichtbarkeit auf Radio-Gesundheit. Nuetzliche diagnostische Sensoren:
- **TX-Queue-Tiefe** (pro Blind) — hilft Konnektivitaetsprobleme zu erkennen
- **Letzte erfolgreiche Kommunikation** (pro Blind) — Zeit seit letztem RX
- **Radio-Status** (Hub-Ebene) — aktueller MARCSTATE, RX/TX-Zaehler, Watchdog-Recovery-Zaehler

**Betroffene Dateien:**
- `components/elero/elero.h` — Zaehler hinzufuegen (rx_count_, tx_count_, watchdog_recovery_count_)
- Neue Sensor-Registrierung im Hub fuer globale Diagnostik
- `cover/__init__.py`, `light/__init__.py` — optionale Diagnostik-Sensor-Konfiguration

**Fix:**
- Interne Zaehler in der Elero-Hub-Klasse ergaenzen
- Ueber bestehenden Web-API-Endpoint `/elero/api/status` exponieren
- Optional als ESPHome-Sensoren fuer HA exponieren

---

### Issue H8: Web-API — Eingabevalidierung fuer Settings-Endpoints

**Priority:** P2 | **Effort:** S (2h) | **Labels:** `ha-integration`, `security`

**Problem:** `handle_cover_settings()` (elero_web_server.cpp:579) akzeptiert Timing-Werte ohne Bereichsvalidierung. Fehlerhafte Requests koennten unrealistische Werte setzen.

**Betroffene Dateien:**
- `components/elero_web/elero_web_server.cpp:579+`

**Fix:**
- Dauer-Werte validieren: `0 <= value <= 300000` ms (5 Minuten max fuer open/close)
- Poll-Intervall validieren: `>= 1000` ms oder 0 (deaktiviert)
- HTTP 400 mit Fehlermeldung bei ungueltigen Werten zurueckgeben
- Beide-oder-keine-Validierung fuer open_duration/close_duration (Python-Validator spiegeln)

**Ueberschneidung:** Teilweise mit IMPROVEMENT_PLAN Issue 4 (Web-Server-Robustheit).

---

### Issue H9: Web-UI — Positionstracking fuer runtime-adoptierte Blinds

**Priority:** P3 | **Effort:** M (4h) | **Labels:** `ha-integration`, `usability`

**Problem:** Runtime-adoptierte Blinds melden immer `"position": null` in der Web-API. Nach Adoption mit gesetzter open/close-Duration sollte Positionstracking funktionieren, aber es gibt kein `recompute_position()`-Aequivalent fuer Runtime-Blinds.

**Betroffene Dateien:**
- `components/elero/elero.h:148–168` (RuntimeBlind struct)
- `components/elero/elero.cpp` (Runtime-Blind Command Handling)

**Fix:**
- `position`, `last_recompute_time_`, `moving_direction` Felder zu RuntimeBlind hinzufuegen
- Einfaches Dead-Reckoning implementieren wenn `open_duration_ms > 0` und `close_duration_ms > 0`
- Web-API-Response um Position erweitern wenn verfuegbar

---

## Bereich C: Logging-Optimierung & obsoleter Code

### Issue L1: `interpret_msg()` Paketdump von LOGD auf LOGV herabstufen

**Priority:** P1 (Important) | **Effort:** S (15min) | **Labels:** `logging`, `performance`

**Problem:** Zeile 879 in elero.cpp feuert ein `ESP_LOGD` mit 20+ Format-Argumenten bei **jedem validen empfangenen Paket** (alle 2–5 Sekunden pro Blind). Extrem verbose auf DEBUG-Level mit signifikantem String-Formatting-Overhead. Bei mehreren Blinds dominiert dies die Log-Ausgabe.

**Betroffene Datei:** `components/elero/elero.cpp:879`

**Fix:**
- `ESP_LOGD` zu `ESP_LOGV` aendern (Verbose-Level, standardmaessig deaktiviert)
- Kurze `ESP_LOGD`-Zusammenfassung hinzufuegen: `"rcv'd from 0x%06x: state=0x%02x rssi=%.1f"`
- Fuer volle Paketdumps soll das Packet-Dump-System genutzt werden

---

### Issue L2: `format_hex_pretty()` im RX-Hot-Path absichern

**Priority:** P1 (Important) | **Effort:** S (30min) | **Labels:** `logging`, `performance`

**Problem:** Zeile 233 in elero.cpp ruft `format_hex_pretty()` auf LOGV-Level in `process_rx()` auf. Auch wenn LOGV normalerweise deaktiviert ist, alloziert `format_hex_pretty()` einen `std::string` und formatiert Hex-Daten **bevor** die Log-Level-Pruefung stattfindet (ESPHome's LOG-Makros evaluieren Argumente vor der Level-Pruefung).

**Betroffene Dateien:** `components/elero/elero.cpp:233, 815–816, 837–838, 849–850`

**Fix:**
- Teure Log-Aufrufe mit expliziter Level-Pruefung umschliessen:
  ```cpp
  #if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    ESP_LOGV(TAG, "RAW RX %d bytes: %s", fifo_count,
             format_hex_pretty(this->msg_rx_, fifo_count).c_str());
  #endif
  ```
- Fuer Fehler-Pfad Hex-Dumps (Zeilen 815, 837, 849): beibehalten (feuern nur bei Fehlern)

---

### Issue L3: `send_command()` Log-Statement kuerzen

**Priority:** P2 | **Effort:** S (15min) | **Labels:** `logging`, `code-quality`

**Problem:** Zeile 1158 in elero.cpp hat ein 409 Zeichen langes einzeiliges `ESP_LOGV` mit 30 Format-Argumenten bei jedem TX. Selbst auf LOGV-Level uebertrieben und schwer lesbar.

**Betroffene Datei:** `components/elero/elero.cpp:1158`

**Fix:**
- Durch knappe Zusammenfassung auf LOGV ersetzen: `"send to 0x%06x: cmd=0x%02x ch=%02d cnt=%02d"`
- Vollen 30-Byte-Dump ins Packet-Dump-System verlagern oder hinter `#ifdef ELERO_VERBOSE_TX` gaten

**Ueberschneidung:** IMPROVEMENT_PLAN Issue 6e.

---

### Issue L4: Log-Levels im gesamten Code standardisieren

**Priority:** P2 | **Effort:** S (1h) | **Labels:** `logging`, `code-quality`

**Problem:** Log-Levels sind inkonsistent:
- TX-Completion-Timing ist LOGD (elero.cpp:275) — sollte LOGV sein (passiert bei jedem TX)
- TX-Timeout ist LOGD (elero.cpp:288) — sollte LOGW sein (abnormaler Zustand)
- Ungueltige Paket-Payloads loggen auf LOGD mit Hex-Dumps (elero.cpp:815) — sollte LOGW sein

**Betroffene Dateien:** `components/elero/elero.cpp` (mehrere Stellen)

**Vorgeschlagene Hierarchie:**

| Level | Verwendung |
|-------|------------|
| `ESP_LOGE` | Unrecoverable: TX Underflow, SPI-Ausfall, Init-Fehler |
| `ESP_LOGW` | Recoverable: FIFO Overflow, TX Timeout, ungueltige Pakete, Stop-Verify-Fehler |
| `ESP_LOGI` | Lifecycle: Setup abgeschlossen, Blind adoptiert/entfernt, Scan Start/Stop |
| `ESP_LOGD` | Operativ: Zustandsaenderungen, Befehl gesendet (nur Zusammenfassung) |
| `ESP_LOGV` | Diagnostik: Volle Paket-Dumps, rohe Register-Reads, Timing-Details |

**Ueberschneidung:** IMPROVEMENT_PLAN Issue 10.

---

## Analyse: Kein obsoleter Code gefunden

Die RadioLib-Migration war sauber durchgefuehrt. Folgendes wurde geprueft und als **korrekt und weiterhin notwendig** befunden:

1. **`write_reg()` / `read_reg()` Wrapper** — duenne Wrapper um RadioLib, aber nuetzlich als Abstraktionsschicht. Beibehalten.
2. **`write_cmd()` fuer Command-Strobes** — direkte SPI weil RadioLib's API auf CC1101-Klasse statt Module liegt. R2 evaluiert Verbesserung.
3. **`write_burst()` / `read_buf()`** — fuer TX-FIFO/RX-FIFO im Hot-Path noetig. R1 evaluiert Verbesserung.
4. **`reset()` / `init()`** — weiterhin notwendig, RadioLib's `begin()` setzt andere Defaults als Elero braucht
5. **`cc1101.h` Register-Map** — alle 94 Defines werden aktiv genutzt, kein Aufraeum-Bedarf
6. **ISR** (elero.cpp:404–409) — korrekt: nur atomare Flags, kein Logging, kein SPI, keine Allokation
7. **Log-Capture-System** — Mutex-geschuetzt, Early-Return-Guard auf `log_capture_` verhindert Overhead wenn deaktiviert
8. **Packet-Dump-System** — gut designter Ring-Buffer (50 Eintraege, ~4.9KB), keine Probleme

---

## Zusammenfassung nach Prioritaet

| # | Bereich | Issue | Prio | Aufwand |
|---|---------|-------|------|---------|
| L1 | Logging | `interpret_msg()` Paketdump LOGD -> LOGV | P1 | S |
| L2 | Logging | `format_hex_pretty()` im RX-Hot-Path absichern | P1 | S |
| H1 | HA | Light Auto-Sensoren (RSSI + Status) | P1 | S |
| R1 | RadioLib | `SPIwriteRegisterBurst()` fuer PATABLE/FIFO | P2 | S |
| R2 | RadioLib | `SPIsendCommand()` fuer Command-Strobes | P2 | S |
| H2 | HA | Light Duplikat-Adress-Validierung | P2 | S |
| H3 | HA | Fehlerzustaende in HA sichtbar machen | P2 | M |
| H4 | HA | Stop-Verify-Erschoepfung melden | P2 | S |
| H5 | HA | Command-Queue-Ueberlauf Feedback | P2 | S |
| H6 | HA | millis()-Wraparound im Positionstracking | P2 | S |
| H8 | HA | Web-API Eingabevalidierung | P2 | S |
| L3 | Logging | `send_command()` Log kuerzen | P2 | S |
| L4 | Logging | Log-Levels standardisieren | P2 | S |
| R3 | RadioLib | MHz-Anzeige in `reinit_frequency()` | P3 | S |
| R4 | RadioLib | `setFrequency()` als MHz-API evaluieren | P3 | S |
| H7 | HA | Diagnostische Sensoren (Radio-Health, TX-Queue) | P3 | M |
| H9 | HA | Positionstracking fuer Runtime-Blinds | P3 | M |

**Empfohlene Reihenfolge:**
1. L1 + L2 (Quick Wins, sofortige Performance-Verbesserung)
2. H1 (Lights bekommen Sensor-Paritaet mit Covers)
3. R1 + R2 (RadioLib-Fehlerbehandlung verbessern)
4. H2 + H3 + H4 + H5 (HA-Haertungs-Batch)
5. L3 + L4 (Logging-Cleanup)
6. H6 + H8 (Robustheit)
7. R3 + R4 + H7 + H9 (niedrigere Prioritaet)

---

## Verifizierung

Fuer jedes implementierte Issue:
1. `esphome compile example.yaml` — muss fehlerfrei durchlaufen
2. Manuelle Hardware-Tests: Blind-/Light-Steuerung, Scan, Web-UI
3. `esphome logs` Ausgabe auf Log-Level-Aenderungen pruefen
4. CLAUDE.md bei Bedarf aktualisieren (neue Konstanten, geaenderte Architektur-Doku)
