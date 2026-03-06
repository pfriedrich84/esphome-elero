# Verbesserungsplan: esphome-elero (seit stable Tag)

## Kontext

Das `stable` Tag (Commit `3f971bc`) wurde als Basis-Release markiert. Seitdem wurden 32 Commits gemergt, primär RF-Reliability-Fixes (TX State Machine, ISR-Handling, Radio-Watchdog). Diese Analyse identifiziert verbleibende Verbesserungen, die als GitHub Issues erfasst werden können.

**Direktiven:** Keine Breaking Changes, keine Instabilitäten.

---

## Issue 1: Korrekte Memory-Ordering für Atomic-Variablen im ISR

**Priority:** P1 (Important) | **Effort:** S (1-2h) | **Labels:** `reliability`, `robustness`

**Problem:** Der ESP32 ist dual-core. Alle atomaren Variablen (`tx_state_`, `rx_ready_`, `gdo0_fired_`) verwenden `memory_order_relaxed` — sowohl im ISR (Store) als auch im Main-Loop (Load). Bei relaxed Ordering auf einem Multi-Core-System kann der ISR einen veralteten `tx_state_`-Wert lesen und `rx_ready_` zum falschen Zeitpunkt setzen.

**Betroffene Dateien:**
- `components/elero/elero.h:333-338` (Deklarationen)
- `components/elero/elero.cpp:203, 206, 295, 305, 397-404` (ISR + Stores/Loads)

**Fix:**
- ISR-Stores (`gdo0_fired_`, `rx_ready_`): `memory_order_release`
- ISR-Load (`tx_state_`): `memory_order_acquire`
- Main-Loop Stores (`tx_state_`, `rx_ready_`, `gdo0_fired_`): `memory_order_release`
- Main-Loop Loads (`rx_ready_`, `gdo0_fired_`): `memory_order_acquire`

**Risiko:** Minimal — rein semantische Verschärfung ohne Verhaltensänderung.

---

## Issue 2: SPI-Fehlerbehandlung und Rückgabewert-Prüfung

**Priority:** P1 (Important) | **Effort:** M (3-6h) | **Labels:** `reliability`, `robustness`

**Problem:** Alle 6 SPI-Funktionen (`write_reg`, `write_burst`, `write_cmd`, `read_reg`, `read_status`, `read_buf`) haben keine Fehlerbehandlung (4x TODO-Kommentare im Code). Zusätzlich ignorieren `reset()`, `flush_and_rx()`, `flush_rx()` die Rückgabewerte von `wait_idle()` und `wait_rx()`.

**Betroffene Dateien:**
- `components/elero/elero.cpp:520-607` (SPI-Funktionen)
- `components/elero/elero.cpp:460-516` (reset, flush_and_rx, flush_rx — ignorierte Returns)
- `components/elero/elero.cpp:547-571` (wait_idle, wait_rx)

**Fix:**
- SPI-Funktionen: Status-Byte des CC1101 nach jedem Transfer prüfen (Chip-Ready-Bit)
- `wait_idle()`/`wait_rx()` Return-Werte prüfen und bei Timeout loggen (WARN)
- Bei wiederholtem SPI-Failure: Radio-Reinit triggern
- Kein neues Error-Handling-Framework nötig — einfach `ESP_LOGW` bei Problemen

**Risiko:** Niedrig — fügt nur Diagnostik hinzu, ändert kein Verhalten.

---

## Issue 3: Log-Buffer Thread-Safety

**Priority:** P1 (Important) | **Effort:** S (1-2h) | **Labels:** `reliability`, `robustness`

**Problem:** Der Logger-Callback (`append_log()`, Zeile 1206-1218) schreibt in einen Ring-Buffer. Der Web-Server (`get_log_entries()`) liest diesen. Es gibt keine Synchronisation zwischen diesen Kontexten, was zu korrumpierten Log-Einträgen führen kann.

**Betroffene Dateien:**
- `components/elero/elero.cpp:1200-1218` (append_log)
- `components/elero/elero.h:364-368` (log_entries_, log_write_idx_)

**Fix:**
- `std::mutex` um Schreib- und Lese-Zugriffe auf `log_entries_`
- Alternativ: Lock-free Ring-Buffer mit atomaren Indices (komplexer)
- Empfohlen: Einfacher Mutex, da Web-API-Zugriffe selten sind

**Risiko:** Minimal.

---

## Issue 4: Web-Server JSON-Sicherheit und Robustheit

**Priority:** P1 (Important) | **Effort:** M (3-6h) | **Labels:** `security`, `robustness`

**Problem:** Mehrere Schwachstellen im Web-Server:

a) **json_escape() unvollständig** (Zeile 16-28): Kontrollzeichen (0x00-0x1F außer \n, \r, \t) werden nicht escaped → ungültiges JSON möglich
b) **strtoul() Truncation** (Zeile 75-78, 804-807): `unsigned long` → `uint32_t` Cast kann auf 64-bit Plattformen abschneiden
c) **strncat O(n²)** (Zeile 686, 756): Hex-Dump-Erzeugung mit quadratischer Laufzeit durch wiederholtes strlen()
d) **Keine string::reserve()** bei JSON-Aufbau: Wiederholte Reallokationen bei vielen Blinds

**Betroffene Dateien:**
- `components/elero_web/elero_web_server.cpp:16-28, 75-78, 249-290, 686-756, 804-807`

**Fix:**
a) `json_escape()`: `\uXXXX`-Encoding für alle 0x00-0x1F Kontrollzeichen
b) Range-Check nach `strtoul()`: `if (v > 0xFFFFFF) return false;` (Blind-Adressen sind 3 Byte)
c) strncat → Pointer-Offset-Ansatz: `pos += snprintf(hex_buf + pos, remaining, ...)`
d) `json.reserve(blinds.size() * 400)` vor Schleifen

**Risiko:** Niedrig — reine Verbesserungen ohne API-Änderungen.

---

## Issue 5: Web-API Command-Bytes aus Cover-Konfiguration lesen

**Priority:** P2 (Nice-to-have) | **Effort:** S (1-2h) | **Labels:** `robustness`

**Problem:** Die Web-API hardcoded Command-Bytes (0x20=up, 0x40=down, 0x10=stop) in `handle_cover_command()` (Zeile 396-404). Wenn ein Benutzer benutzerdefinierte Command-Bytes in der YAML-Konfiguration überschreibt, sendet die Web-API die falschen Befehle.

**Betroffene Dateien:**
- `components/elero_web/elero_web_server.cpp:396-428`
- `components/elero/elero.h` (EleroBlindBase Interface)

**Fix:**
- `EleroBlindBase` um Getter für Command-Bytes erweitern: `get_command_up()`, `get_command_down()`, etc.
- `handle_cover_command()`: Aus dem Cover-Objekt lesen statt hardcoded Werte
- Für Runtime-Blinds: Default-Werte beibehalten (0x20/0x40/0x10)

**Risiko:** Niedrig — erweitert nur das Interface, ändert kein Default-Verhalten.

---

## Issue 6: Code-Cleanup und Konstanten-Extraktion

**Priority:** P2 (Nice-to-have) | **Effort:** S (1-2h) | **Labels:** `code-quality`

**Problem:** Mehrere Magic Numbers und tote Code-Pfade:

a) **Toter TxState::LOADING Case** (elero.cpp:217-219): Unerreichbarer Switch-Case
b) **GDO0-Miss-Threshold** (elero.cpp:246): Hardcoded `10` ohne Konstante
c) **Poll-Stagger-Offset** (elero.cpp:972): Hardcoded `5000`ms ohne Konstante
d) **process_rx Max-Packets** (elero.cpp:138): Hardcoded `4` ohne Konstante
e) **Langer Log-String** (elero.cpp:1127): 409 Zeichen in einer Zeile

**Betroffene Dateien:**
- `components/elero/elero.h` (neue Konstanten)
- `components/elero/elero.cpp:138, 217-219, 246, 972, 1127`

**Fix:**
a) `TxState::LOADING` Case entfernen
b) `ELERO_GDO0_MISS_WARN_THRESHOLD 10` definieren
c) `ELERO_POLL_STAGGER_MS 5000` definieren
d) `ELERO_MAX_RX_PER_LOOP 4` definieren
e) Log-Formatierung in Helper oder Multi-Line aufteilen

**Risiko:** Keines — reine Refactoring-Änderungen.

---

## Issue 7: Defensive Integer-Typen in RX-Paket-Parsing

**Priority:** P2 (Nice-to-have) | **Effort:** S (1h) | **Labels:** `robustness`

**Problem:** Bounds-Checks in `interpret_msg()` verwenden uint8_t-Arithmetik (elero.cpp:804, 816). Aktuell sicher durch vorgelagerte Prüfungen, aber fragil bei zukünftigen Änderungen.

**Betroffene Dateien:**
- `components/elero/elero.cpp:780-820` (interpret_msg Bounds-Checks)

**Fix:**
- Zwischenberechnungen auf `uint16_t` casten: `if ((uint16_t)(26 + dests_len) >= CC1101_FIFO_LENGTH)`
- Oder separate Variable: `uint16_t end_offset = 26u + dests_len;`

**Risiko:** Keines.

---

## Issue 8: Cover millis()-Wraparound in Poll-Offset-Berechnung

**Priority:** P2 (Nice-to-have) | **Effort:** S (1h) | **Labels:** `robustness`

**Problem:** EleroCover.cpp:51 berechnet `(now - this->poll_offset_ - this->last_poll_)`. Bei millis()-Wraparound (nach ~49 Tagen) kann die doppelte Subtraktion mit Offset fehlerhaft sein.

**Betroffene Dateien:**
- `components/elero/cover/EleroCover.cpp:51`

**Fix:**
- Poll-Offset nur einmal bei Setup auf `last_poll_` anwenden statt bei jeder Prüfung
- Oder: `(now - this->last_poll_) > intvl` und Offset nur bei Initialisierung einrechnen

**Risiko:** Minimal.

---

## Issue 9: Python-Schema: Duplikat-Adress-Validierung

**Priority:** P2 (Nice-to-have) | **Effort:** S (1-2h) | **Labels:** `robustness`

**Problem:** Es gibt keine Validierung, die verhindert, dass mehrere Covers mit derselben `blind_address` konfiguriert werden. Das führt zu konfligierenden Befehlen an denselben Motor.

**Betroffene Dateien:**
- `components/elero/cover/__init__.py`
- `components/elero/light/__init__.py`

**Fix:**
- Final-Validator in `cover/__init__.py` der alle konfigurierten `blind_address`-Werte sammelt und bei Duplikaten einen `cv.Invalid` wirft
- Gleiche Prüfung für Light-Adressen

**Risiko:** Keines — nur Compile-Zeit-Validierung.

---

## Issue 10: Inkonsistente Log-Levels standardisieren

**Priority:** P3 (Cosmetic) | **Effort:** S (1h) | **Labels:** `code-quality`

**Problem:** Log-Levels sind inkonsistent: FIFO-Overflow = WARN, TX-Timeout = ERROR. Keine klare Severity-Hierarchie.

**Betroffene Dateien:**
- `components/elero/elero.cpp` (diverse Stellen)

**Fix:** Klare Hierarchie definieren:
- `ESP_LOGE`: Unrecoverable Fehler (TX Underflow, SPI-Ausfall)
- `ESP_LOGW`: Recoverable Probleme (FIFO Overflow, TX Timeout mit Retry, Radio-Watchdog-Recovery)
- `ESP_LOGI`: Wichtige Status-Änderungen (Blind-Adoption, Scan Start/Stop)
- `ESP_LOGD`: Operative Details (TX complete, RX packet processed)

**Risiko:** Keines.

---

## Zusammenfassung und Reihenfolge

| # | Issue | Prio | Effort | Kategorie |
|---|-------|------|--------|-----------|
| 1 | Memory-Ordering Atomics | P1 | S | Reliability |
| 2 | SPI-Fehlerbehandlung | P1 | M | Reliability |
| 3 | Log-Buffer Thread-Safety | P1 | S | Reliability |
| 4 | Web-Server JSON-Sicherheit | P1 | M | Security |
| 5 | Web-API Command-Bytes | P2 | S | Robustness |
| 6 | Code-Cleanup & Konstanten | P2 | S | Code Quality |
| 7 | Defensive Integer-Typen | P2 | S | Robustness |
| 8 | millis()-Wraparound | P2 | S | Robustness |
| 9 | Duplikat-Adress-Validierung | P2 | S | Robustness |
| 10 | Log-Levels standardisieren | P3 | S | Code Quality |

**Empfohlene Reihenfolge:** Issues 1 → 3 → 4 → 2 → 6 → 5 → 7 → 8 → 9 → 10

Issues 1 und 3 sind schnell umsetzbar und beheben echte Concurrency-Risiken. Issue 4 schließt Sicherheitslücken im Web-Server. Issue 2 ist aufwändiger, aber adressiert die meisten TODO-Kommentare im Code.

---

## Verifizierung

Für jedes Issue nach Implementierung:
1. `esphome compile example.yaml` — muss fehlerfrei durchlaufen
2. Manuelle Tests auf ESP32-Hardware: Blind-Steuerung, Scan, Web-UI
3. Prüfen, dass keine bestehende Funktionalität beeinflusst wird
4. CLAUDE.md bei Bedarf aktualisieren (neue Konstanten, geänderte Architektur-Doku)
