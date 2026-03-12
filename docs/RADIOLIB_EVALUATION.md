# RadioLib vs. Custom CC1101: Technische Bewertung

## 1. Zusammenfassung

Dieses Dokument bewertet RadioLib (github.com/jgromes/RadioLib) als Alternative zur aktuellen Custom-SPI-Implementierung für die CC1101-Kommunikation im esphome-elero Projekt.

**Entscheidung:** Variante B — Hybrid-Integration. RadioLib wird als SPI-Abstraktionsschicht genutzt, die gesamte Elero-Protokolllogik (TX State Machine, Encryption, Position Tracking) bleibt custom.

---

## 2. Protokollanalyse

Das Elero-Protokoll nutzt **2-FSK** (MDMCFG2=0x13), nicht ASK/OOK:
- Datenrate: ~38.4 kbps
- Deviation: ±47.6 kHz
- RX-Bandbreite: 101.6 kHz
- Sync Word: 0xD391
- Paketlänge: 29 Bytes fix (ELERO_MSG_LENGTH)
- Frequenz: 868.35 MHz (Europa Standard)

Die OOK/ASK-Fähigkeit von RadioLib ist für Elero **nicht relevant**.

---

## 3. Technischer Vergleich

### 3.1 SPI-Kommunikation

| Aspekt | Custom (aktuell) | RadioLib |
|--------|-------------------|----------|
| Register-Write | `enable/write_byte/disable` + 15µs | `SPIsetRegValue()` mit verify-readback |
| Register-Read | `enable/write_byte/read_byte/disable` + 15µs | `SPIreadRegister()` mit Validierung |
| Burst-Write | Byte-Loop, kein Fehlercheck | `SPIwriteRegisterBurst()` |
| Strobes | `write_byte(cmd)`, kein Status-Check | `SPIsendCommand()` |
| **Fehlerbehandlung** | **4 TODOs: keine Prüfung** | **Verify-Readback, Status-Checks** |

### 3.2 TX/RX-Architektur

| Aspekt | Custom | RadioLib |
|--------|--------|----------|
| TX-Pipeline | 7-State Non-Blocking State Machine | `startTransmit()` + ISR + `finishTransmit()` |
| RX-Handling | ISR + `rx_ready_` Flag + `process_rx()` | `startReceive()` + ISR + `readData()` |
| Interrupt-Pins | 1 Pin (GDO0) dual-use | Empfiehlt 2 Pins (GDO0+GDO2) |
| FIFO-Kontrolle | Direkte SFRX/SFTX-Strobes | Intern abstrahiert |
| MARCSTATE-Recovery | Watchdog alle 5s | Automatische Timeouts |

### 3.3 Auswirkung auf Position-Tracking

| Timing-Faktor | Custom | Mit RadioLib (Hybrid) |
|----------------|---------|----------------------|
| TX-Pipeline-Latenz | ~10-50ms | **Identisch** (State Machine bleibt) |
| Post-TX Cooldown | 10ms | **Identisch** |
| 150ms Kompensation | Funktioniert | **Unverändert** |
| Stop-Verify-Loop | 3 Retries, 500ms | **Unverändert** |

**Fazit:** Hybrid-Ansatz hat **kein Risiko** für Position-Tracking, da TX State Machine und alle Timing-Parameter identisch bleiben.

---

## 4. Risikobewertung RadioLib

### Hohe Risiken (nur bei Vollmigration relevant)
1. **Paketverlust:** 22% Korruptionsrate bei 50 kbps auf mancher Hardware (GitHub-Reports)
2. **FIFO-Kontrolle:** Kein direkter SFRX/SFTX bei Nutzung der High-Level TX/RX API
3. **Variable-Length Bug:** RadioLib Issue #1482

### Mittlere Risiken
4. **SPI-Ownership:** RadioLib + ESPHome SPIDevice auf demselben Bus → gelöst durch HAL-Adapter
5. **Flash-Overhead:** ~50-100KB zusätzlich
6. **Dual-ISR:** RadioLib empfiehlt GDO0+GDO2 → nur bei Vollmigration nötig

### Niedrige Risiken (Hybrid)
7. **Register-Zugriff:** RadioLib exponiert `SPIsetRegValue/SPIgetRegValue/SPIsendCommand` → volle Kontrolle bleibt
8. **API-Stabilität:** 3400+ Commits, aktiv gepflegt

---

## 5. Entscheidungsvarianten

### Variante A: Status Quo + SPI-Fehlerbehandlung

- Die 4 TODOs in `elero.cpp` manuell nachrüsten
- **Aufwand:** ~2-4h | **Risiko:** Keines | **Nutzen:** Minimal

### Variante B: Hybrid — RadioLib als SPI-Layer (GEWÄHLT)

- RadioLib nur für Register-Zugriff mit verify-readback
- TX State Machine, FIFO-Flushing, Watchdog, Encryption bleiben custom
- **Aufwand:** ~2-3 Tage | **Risiko:** Gering | **Nutzen:** SPI-Fehlerbehandlung, Zukunftssicherheit

### Variante C: Vollmigration auf RadioLib TX/RX API

- `startTransmit/startReceive/readData` statt eigener FIFO-Operationen
- **Aufwand:** ~1-2 Wochen | **Risiko:** Hoch | **Nutzen:** Weniger eigener Code

### Variante D: Parallelbetrieb

- Nicht möglich mit einem CC1101-Modul
- **Aufwand:** N/A | **Risiko:** N/A

---

## 6. Begründung für Variante B

1. **SPI-Fehlerbehandlung gelöst:** RadioLib `SPIsetRegValue()` macht verify-readback automatisch — alle 4 TODOs sind behoben
2. **Kein Regressionsrisiko:** TX State Machine, FIFO-Flushing, Watchdog, Position-Tracking bleiben identisch
3. **Zukunftssicher:** Wenn andere Funkprotokolle (OOK/ASK, 433 MHz) dazukommen, ist die Abstraktion da
4. **Inkrementell:** Migration kann Schritt für Schritt getestet werden
5. **Raw-Zugriff bleibt:** `SPIsendCommand()` für SFRX/SFTX-Strobes, `SPIgetRegValue()` für MARCSTATE

---

## 7. Implementierungsdetails

### HAL-Adapter (EspHomeHal)

RadioLib benötigt eine Hardware-Abstraktionsschicht. `EspHomeHal` implementiert `RadioLibHal` und delegiert SPI-Operationen an ESPHome's SPIDevice:

```
ESPHome YAML → spi: → SPIDevice → EspHomeHal → RadioLib::Module → RadioLib::CC1101
```

Die `Elero`-Klasse behält ihre SPIDevice-Vererbung für ESPHome-Kompatibilität. RadioLib greift über den HAL-Adapter auf denselben SPI-Bus zu.

### SPI-Methoden-Migration

| Custom-Methode | RadioLib-Äquivalent |
|----------------|---------------------|
| `write_reg(addr, data)` | `radio_module_->SPIsetRegValue(addr, data)` |
| `write_burst(addr, data, len)` | `radio_module_->SPIwriteRegisterBurst(addr, data, len)` |
| `write_cmd(cmd)` | `radio_module_->SPIsendCommand(cmd)` |
| `read_reg(addr)` | `radio_module_->SPIreadRegister(addr)` |
| `read_status(addr)` | `radio_module_->SPIreadRegister(addr \| 0xC0)` |
| `read_buf(addr, buf, len)` | `radio_module_->SPIreadRegisterBurst(addr, len, buf)` |

### Was NICHT migriert wird

- `reset()` — bleibt custom (RadioLibs `begin()` setzt falsche Register-Defaults)
- `init()` — 29 Elero-spezifische Register-Writes bleiben
- TX State Machine (`advance_tx()`) — keine Änderung
- RX Processing (`process_rx()`) — keine Änderung
- Encryption/Decryption — keine Änderung
- Position Tracking — keine Änderung

---

## 8. Quellen

- [RadioLib GitHub](https://github.com/jgromes/RadioLib)
- [RadioLib CC1101 Class Reference](https://jgromes.github.io/RadioLib/class_c_c1101.html)
- [RadioLib Issue #357: RX Interrupt during TX](https://github.com/jgromes/RadioLib/issues/357)
- [RadioLib Issue #1482: Variable-length packet bug](https://github.com/jgromes/RadioLib/issues/1482)
- [esphome-radiolib-cc1101](https://github.com/juanboro/esphome-radiolib-cc1101)
- [RadioLib-esphome Fork](https://github.com/smartoctopus/RadioLib-esphome)
