# RF-Zuverlässigkeit: Implementierungsbericht

**Datum: 08.09.2026.** Umsetzung der P1–P3-Befunde aus dem [Review von Astra GPT 6 am unveränderlichen Review-Commit](https://github.com/pfriedrich84/esphome-elero/blob/1ca6cf755cea05f619c1d10efbcc2d1db280d955/docs/developer/reviews/2026-09-08-rf-reliability-astra-gpt-6.md).

## Basis, Umfang und Aussagegrenze

- Ausgangspunkt: aktuelles `origin/main` bei Arbeitsbeginn, `f15974bb8d6f40cbb728b823bc2e98d96f6a360b`.
- Arbeitsbranch: `fix/rf-reliability-2026-09-08`; drei thematische Commits, kein Rollback.
- Teil 1: `a83344dcacea7e757549ad2be1484ff193638024` — kausale, gemeinsame STOP-Verifikation und sichere Lanes/Gruppen.
- Teil 2: `380399f109261420103ecfa7c3eb4f1907b64b94` — FIFO, echte CCA und completion-basierte Abstände.
- Teil 3 enthält Zustellungssemantik, Counter-Resync, Register-/Fehlerabsicherung, zusätzliche Randfalltests und diesen Bericht. Der genaue finale Commit und seine CI-Läufe werden im PR festgehalten; ein Dokument kann seinen eigenen Commit-Hash nicht vorwegnehmen.

**Keine Hardwarevalidierung und kein Nachweis verbesserter physischer Reichweite.** Die Änderungen beseitigen bzw. begrenzen nachgewiesene Software-Verlust-/Fehlklassifikationspfade. Frequenz, PATABLE, Modem, RX-Bandbreite, Deviation, AGC und Kalibrierungswerte bleiben unverändert. Normale Wiederholungen werden nicht pauschal erhöht; es entstehen weder Singleton noch zusätzlicher SPI-Owner oder direkter Gruppen-Sendebypass.

Der direkte PR nach `main` entspricht dem ausdrücklich angeforderten STOP-Sicherheits-Hotfix; dafür ist gemäß Repository-Regel das Label `hotfix` erforderlich. Keine automatische Zusammenführung.

## Befund → Änderung → Test → Hardwaregrenze

Alle zehn Zeilen der ursprünglichen Befundtabelle sind hier abgeglichen. Die Testdateien liegen unter `tests/unit/`.

| Review-Befund | Umsetzung / wesentliche Dateien | Regressionsevidenz | Verbleibende Grenze |
|---|---|---|---|
| **P1: ungeeignete/alte Statusdaten bestätigen STOP** | `elero_rx_metadata.h`, `elero_rx_fifo.h`, `elero_protocol.cpp`, `EleroCover.cpp`: Core-0-Sequenz/Epoche/Capture-Zeit und TX-Cutoff; nur frischer Status des richtigen Motors; explizite Stillstands-, Bewegungs- und Fehlerklassen | `test_cover_delivery.cpp`: alte STOPPED-Antwort, alter Präfix, falscher Motor, alle sieben Stillstandswerte, UNKNOWN/TIMEOUT/0xFF, BLOCKING/OVERHEATED; `test_radio_delivery.cpp`: wirklicher Completion-Fence-Pfad | Spontaner frischer Status ist kein authentifizierter ACK. Ein sehr früher Status wird konservativ ausgeschlossen; mechanischer Stillstand ist auf Hardware zu prüfen. |
| **P1: FIFO-Bulkread verschluckt Folgepakete** | `elero_rx_fifo.h`, `elero_cc1101.cpp`: keine Teilreads bei aktivem RX/CRC_AUTOFLUSH; begrenztes Warten, verifiziertes IDLE, je Paket Länge + Body + beide Statusbytes; explizite Tail-/Overflow-Recovery | `test_rx_fifo.cpp`: zwei 32-Byte-Pakete, vollständiges Paket + Präfix, Autoflush zwischen Aufrufen, ungültige Länge, Overflow, 20-ms-Tail-Recovery; Radio-Tests mit echtem `process_rx()` | FIFO-Kapazität, bereits erfolgter Hardware-Autoflush/Overflow und GPIO→SIDLE-Rennen bleiben physische Grenzen; keine Rekonstruktion verlorener Bytes behauptet. |
| **P1: erneuter manueller STOP wartet im Fehler-Cooldown** | Coordinator-Auswahl umgeht Cooldown nur für STOP; Cooldown wird nicht für Normalverkehr gelöscht | `ManualStopBypassesFailureCooldownButNormalCommandsDoNot` | Funkbelegung, laufende physisch zugelassene Pakete und Radiofehler können trotzdem verzögern. |
| **P2: Web-/Button-STOP ohne Cover-Verifikation** | `EleroCover::request_stop()` ist gemeinsamer Einstieg von CoverCall/Automation, direkten Web-/Button-Intents, automatischem Positions-STOP und Gruppen-Vorbereitung | `CoverWebButtonAndAutomationShareStopEntry`; echte Cover-Methoden, produktiver Web-Parser und Button-Mapper | Web-/Button-Adapter werden am semantischen Aufruf geprüft, kein Browser-/Home-Assistant-E2E und kein Tastendruck an Hardware. |
| **P2: native Gruppenbewegung / verkürzter fremder STOP-Burst** | Native Lane hält Referenzen auf Mitglieds-Lanes und respektiert ihre Verifikation; selektierter STOP wird nicht durch anderen STOP abgebrochen; Coalescing schützt den Restburst | `GroupStopVerifiesAllMembersAndBlocksNativeMovement`, `ConcurrentSameProfileStopPreservesBothTwoPacketBursts`, `GroupPartialStopThenTerminalFailureCannotLeaveVerificationStuck`; bestehende Ordering-/Counter-/Admission-/Fallback-Tests | Native Mehrzielzustellung ist weiterhin kein ACK aller Motoren. Reale Gruppenantworten/Last und Fairness sind zu messen. |
| **P2: SIDLE→STX umgeht CCA; RX wird verdrängt** | IDLE nur zum sicheren FIFO-Zugriff/Laden; danach SRX, Listen-Phase, stabiles RX/CCA und STX aus RX; begrenzter Backoff im RX; Drain vor TX und nach Completion/Cooldown | SPI-Spies: `StxIsActuallyIssuedFromRxAfterRssiListenNotFromIdle`, Busy-/Hardware-CCA-Rejection, Cooldown-Feedback, verlorene RX-End-IRQ | Spies beweisen Strobe-Reihenfolge/Softwareentscheidungen, nicht RSSI-/AGC-Einschwingzeit, belegten realen Kanal oder Funkvorschriften. |
| **P2: send_delay fehlt zwischen verschiedenen Intents** | `CompletionSpacing` überlebt Intent-Reset; Hub wendet Abstand auch über Profilwechsel an; Normalverkehr mindestens 2 ms RX-Gelegenheit, STOP davon ausgenommen | `DistinctNormalIntentsWaitFromCompletionButStopBypasses`, Timing-Tests einschließlich millis=0/Wrap; Konfigurationsdoku aktualisiert | Abstand ist RX-Gelegenheit, keine garantierte Motor-Antwortphase. Scheduler-/SPI-Latenzen kommen hinzu. |
| **P2: verworfene Counter verhindern Resync** | `CounterState` trennt akzeptierte Frontier, beobachtete Aktivität und Kandidaten; mindestens drei voranschreitende Kandidaten plus akzeptierte Altersschwelle | `test_packet_replay.cpp`: Reset 100→1…6 alle 5 s, einzelner alter Frame, Dauer-Replay, Reihenfolgefehler, Burst/Timeout, Counter-/Millis-Wrap | Keine kryptografische Replay-Abwehr. Eine passend getaktete kohärente Replay-Folge ist ohne Protokollauthentifizierung nicht sicher unterscheidbar. |
| **P2: normale Zustellung ohne Motorbestätigung** | Bewusst die erlaubte **explizit unbestätigte** Semantik gewählt: `MotorDeliveryEvidence::LOCAL_TX_UNCONFIRMED`; `COMPLETED` heißt lokaler Plan fertig; `delivery_unconfirmed` / `delivery_failed` in Cover-/Light-Diagnostik und Runtime-Logs | `OrdinaryOpenWithoutMotorResponseRemainsDeliveryUnconfirmed`; bestehende Delivery-/Timed-Action-Tests | Einzelner RF-Verlust normaler Befehle bleibt möglich. Es wird kein Echo-Counter-ACK erfunden und keine unbelegte automatische Wiederholung eingeführt. |
| **P3: unstabile Statusregister / Underflow als Erfolg** | Bounded Stable-Reads; Fehlerbits bleiben erhalten; Erfolg nur nach beobachtetem TX/TX_END, stabilem fehlerfrei leerem TXFIFO und erwarteter RX-Rückkehr; Recovery nur aus geprüftem IDLE | Radio-Tests: Underflow + fallendes GDO + leere Low-Bits, unerwartetes IDLE, unstabile MARCSTATE/RXBYTES/TXBYTES/PKTSTATUS, fehlgeschlagene Vorbereitung zurück in RX | Injizierte Fehler beweisen Zweigkorrektheit, nicht Häufigkeit/Erreichbarkeit auf realem CC1101. Konservative Falsch-Negative bei unbeobachtet kurzem TX sind möglich. |

### Vierzehn geforderte Szenariogruppen

1. Vor lokaler STOP-Completion entstandenes/gepuffertes STOPPED bestätigt nicht — Cover- und Radio-Fence-Tests.
2. UNKNOWN, TIMEOUT und unbekannte Werte bestätigen nicht — Cover-Klassifikation.
3. Frischer gültiger Stillstand bestätigt; falsche Quelle nicht — sieben Stillstandscodes und Wrong-Motor-Test.
4. BLOCKING/OVERHEATED sind eigene Fehler, keine erfolgreiche STOP-Verifikation — Motorfehler-Test; frische Bewegung besitzt genau einen zusätzlichen Burst.
5. Neuer manueller STOP nach `stop_failed` umgeht nur seinen Cooldown — Normalbefehle bleiben gesperrt.
6. Cover, Web, Custom-Button, Automation/automatischer Einstieg aktivieren dieselbe Verifikation — produktive Cover-Methoden mit Adapter-Inputs.
7. Native Gruppenbewegung wartet auf Mitglieds-Verifikation — echte Gruppen-/Cover-Methoden.
8. Zwei konkurrierende STOPs behalten ihre zwei Pakete und Counter-Zuordnung — gleicher RF-Profil-Coordinator.
9. Zwei vollständige FIFO-Pakete werden separat verarbeitet — exakte FIFO-Read-Längen und Statusbytes geprüft.
10. Vollständiges Paket + Präfix, Autoflush, Overflow und falsche Länge — Präfixschutz ohne Software-Splicing, begrenzte Recovery.
11. RX während TX-Cooldown/fehlendes Endereignis geht nicht einfach vor nächstem TX verloren — produktiver Radio-Pfad.
12. CCA beschäftigt/frei/Hardware-Ablehnung und unterschiedliche Intents mit `send_delay=1000ms` — begrenzte Versuche, RX→STX, completion-basierter Abstand; STOP bleibt bevorzugt.
13. Normales OPEN ohne Antwort bleibt unbestätigt; Counter-Neustart erholt sich trotz kontinuierlicher Statuspakete — keine erfundenen ACKs, kein Quiet-gap-Verhungern.
14. Dynamische Reads und Underflow trotz scheinbar leerem FIFO/GDO-Ende — keine erfolgreiche Completion aus Fehler/Ungewissheit.

Die abschließende Diff-Prüfung fand zusätzlich einen nach dem ersten nativen STOP-Paket hängenden Fehlerabschluss: `pending_stop_transition_` war bereits gelöscht, `stop_burst_pending_` blieb bei endgültigem Gruppenfehler jedoch stehen. Ein neuer Integrationstest reproduzierte dies vor dem Fix; der Fehlerabschluss prüft nun die aktive Verifikation und räumt den Burst-Zustand auf. Zusätzlich sind Verifikations-Deadline über millis-Wrap, unverbrauchte Präfixe bei instabilem Read sowie die bestehende Admission-/Ordering-/Counter-/Repeat-/Gruppen-/Parser-Suite abgedeckt.

## Ablauf, Budgets und Semantik

### STOP

`request_stop()` verwaltet Trigger/Position, Annahmefehler, pending Transition, Verifikation und Prioritäts-Lane. Gruppen rufen nach erfolgreicher atomarer Admission denselben Einstieg mit `already_admitted=true` auf, ohne ein zusätzliches Paket zu erzeugen. Erste erfolgreiche lokale STOP-Completion liefert den RX-Cutoff. Vorher gepufferte vollständige Pakete werden vor dem Fence verarbeitet; ein in-flight Präfix behält seine alte Epoche, auch wenn die Interpretation später erfolgt. Der Hauptloop-Zeitpunkt kann keine Frische erzeugen. Capture-Zeit bezeichnet FIFO-/Präfixbeobachtung auf Core 0, nicht eine behauptete exakte physische GDO-Endzeit.

`stop_queued`, `stop_verifying`, `stop_confirmed`, `stop_motor_blocking`, `stop_motor_overheated` und `stop_failed` sind getrennte Ergebnisse. Während Verifikation überschreibt die rohe Sensorpublikation nicht vorher den Cover-Befund mit altem STOPPED/UNKNOWN. IDLE bleibt eine UI-Schätzung. Aus RF-/Dispatch-Latenz wird kein mechanischer Nachlauf errechnet.

Unverändert zwei RF-Pakete pro STOP-Burst mit gemeinsamem Counter; maximal ein weiterer Verifikations-Burst und ein CHECK pro Burst, jeweils im bestehenden Zwei-Sekunden-Raster. Normales periodisches Polling wird während der eigenen STOP-Verifikation unterdrückt. Ohne Antworten entsteht im Host-Test nach ungefähr acht Sekunden ab erster erfolgreicher TX ein sichtbarer Fehler; dies ist **keine garantierte physische Gesamtlatenz** unter Funk-/Queue-/Scheduler-Last. Laufende Burst-Reste werden nicht stillschweigend durch fremde STOPs gestrichen. Ein wiederholter Nutzeraufruf innerhalb derselben Verifikation setzt das Budget nicht zurück.

### Radio

CRC_AUTOFLUSH bleibt aktiv. Während GDO einen laufenden Empfang anzeigt, werden keine FIFO-Bytes konsumiert. Nach höchstens 20 ms erfolgt begrenzte Recovery eines hängenden/abgebrochenen Empfangs; vollständige Vorgänger werden aus dem eingefrorenen FIFO getrennt abgearbeitet. Im GPIO→SIDLE-Rennen kann ein gerade begonnener Tail unterbrochen werden: dieser wird ausdrücklich verworfen/gezählt, nicht zu einem späteren Paket ergänzt. Bereits durch Hardware-Autoflush/Overflow verlorene Daten sind nicht wiederherstellbar.

CCA wartet zunächst mindestens 1 ms in RX. Busy-Entscheidungen haben 2–19 ms Backoff, höchstens fünf solche Entscheidungen oder 50 ms pro Radio-Versuch. STOP umgeht CCA nicht. Die vorhandenen drei Coordinator-Retries bleiben erhalten; FIFO-/CCA-Retrybudgets sind keine neue unbegrenzte Schleife. Normale Abstände gelten ab tatsächlicher erfolgreicher lokaler Completion, auch zwischen Intents/Profilen; STOP umgeht Abstand/Cooldown, nicht bereits zugelassene Pakete oder Radio-Sicherheit.

Dynamische Statusreads benötigen zwei gleiche aufeinanderfolgende Vollbytes, bei höchstens fünf SPI-Leseoperationen. Gesehene FIFO-Fehlerbits/Underflow werden nicht wegmaskiert. Ungewissheit kann warten/fehlschlagen, niemals ACK/Completion erfinden. Auch Recovery-SFRX/SFTX setzt verifiziertes IDLE voraus.

### Counter und gewöhnliche Befehle

Resync: >=30 s ohne akzeptierten Fortschritt **plus** mindestens drei unterschiedliche, plausibel steigende Kandidaten über >=1 s; Schrittweite <=16, Abstand <=10 s, Kettenalter <=30 s. Duplikate zählen nicht, Reihenfolge-/Zeitverletzungen beginnen eine neue Kette. 100→1…6 alle fünf Sekunden resynchronisiert bei 30 s. Der Diagnosezeitpunkt aller empfangenen Pakete bleibt getrennt und verschiebt die akzeptierte Frontier nicht.

Normales OPEN/CLOSE/TILT und Lichtbefehle enden lokal **unbestätigt**; spätere rohe Motorzustände sind unabhängige Beobachtungen. Dead-Reckoning bleibt Schätzung. Damit wird die Architekturgrenze sichtbar behandelt, nicht durch angebliche ACKs oder pauschal mehr Pakete verdeckt.

## Reproduzierbare Validierung

### Host

Toolchain: GCC 12.2.0, CMake 3.25.1, C++17, Python 3.11.2; bestehender GoogleTest-Pin aus `tests/CMakeLists.txt`. ESPHome/Pytest für ergänzende Python-Prüfung: 2026.3.1 / 9.0.2.

```bash
cmake -S tests -B /tmp/elero-rf-build -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/elero-rf-build --parallel 3
ctest --test-dir /tmp/elero-rf-build --output-on-failure

cmake -S tests -B /tmp/elero-rf-asan -DCMAKE_BUILD_TYPE=Debug -DELERO_SANITIZERS=ON
cmake --build /tmp/elero-rf-asan --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir /tmp/elero-rf-asan --output-on-failure

pytest tests/python/ -q
ruff check components/ tests/host/
ruff format --check components/ tests/host/
python3 scripts/check_markdown_links.py
git diff --check
```

Die vollständige lokale finale Suite besteht: **336/336 Debug und 336/336 ASan/UBSan**, davon **322 individuell registrierte Tests plus 14 zusätzliche Suite-Aufrufe**. Python: **39/39**. Ruff Check/Format für Komponenten und Host-Generatoren, Markdown-Links und Whitespace-Prüfung bestehen. Exakte Head-/Run-Identitäten und der abschließende Firmware-CI-Status werden im PR-Protokoll festgehalten; Erfolge eines früheren Heads ersetzen keine aktuelle Prüfung.

`tests/host/generate_cover_fixture.py` kompiliert vollständige produktive Cover-/Gruppen-Methoden mit ersetzten ESPHome-Includes. `generate_radio_fixture.py` übernimmt benannte Radio-Methoden unverändert aus der Quelle; SPI/GPIO/Queue/Clock und Übergaben an Parser/Entities sind Testgrenzen. FIFO-/Counter-/Timing-/Coordinator-Logik wird direkt eingebunden. Dies sind keine elektrisch realistischen CC1101- oder Motor-Simulationen. Die Firmware-Matrix ist nötig, weil diese Host-Grenzen nicht sämtliche ESPHome-Typen/ABI prüfen.

### Vollständige ESPHome-Firmware

Reproduktion mit Python 3.12 und dem vorhandenen CI-Pin:

```bash
python3.12 -m venv /tmp/elero-firmware-venv
/tmp/elero-firmware-venv/bin/pip install esphome==2026.3.1
/tmp/elero-firmware-venv/bin/esphome compile tests/configs/compile_test.yaml
for cfg in tests/configs/*.yaml; do
  /tmp/elero-firmware-venv/bin/esphome compile "$cfg"
done
```

Unveränderte acht Fixtures: `compile_test`, `custom_frequency`, `esp32s3-idf`, `light_only`, `minimal`, `multi_cover`, `no_auto_sensors`, `no_web`. Die Vollkonfiguration erzeugt mit diesem ESPHome-Pin pioarduino **55.03.37**, Arduino-ESP32 **3.3.7**, ESP-IDF-Paket **5.5.3.1** und RadioLib **7.1.2**; die CI verwendet die jeweiligen Fixture-Frameworks. Keine privaten WLAN-/Geräte-YAMLs verwendet.

- CI `34272854890` auf Teil 1 bestand vollständig, einschließlich aller acht vollständigen Firmware-Builds.
- CI `34275206823` auf Teil 2 fand einen echten Xtensa-Typfehler (`uint32_t` vs. `unsigned int` in `std::max`); Teil 3 verwendet den expliziten Template-Typ `uint32_t`. Der x86-Hosttest konnte diesen ABI-Unterschied nicht erkennen.
- Lokale Firmwareversuche scheiterten an Speicherplatz während Toolchain-Installation und anschließend an einem unvollständigen PlatformIO-Paket. Sie gelten **nicht** als erfolgreiche Builds. Die finale CI-Matrix ist ein Pflicht-Gate; ein grüner älterer Head wird nicht übernommen. Finale Run-/Job-/Head-Belege stehen im PR, nicht in einer vorweggenommenen Erfolgsbehauptung dieses Commits.

## Hardware-A/B/A-Plan und offene Risiken

1. Zuerst installierten Komponentencommit, ESPHome/Framework, RadioLib, CC1101-Version, Board, Versorgung und bereinigte Konfiguration erfassen. Für A den Review-Baseline-Commit `f15974b…`, für B den exakten PR-Head bauen; anschließend wieder A. Keine historische Firmware als ungeprüfte Produktions-Downgrade-Empfehlung verwenden.
2. Hardware, Netzteil, Antenne, Orientierung, Standort, Motor und YAML konstant halten. Gleiche Frequenz-/PA-/Modemwerte per Registerdump dokumentieren. Sicheren Fahrbereich, thermische Pausen und Abbruchkriterien vorab mit der Motor-Spezifikation festlegen.
3. Je A/B/A-Block zunächst einen Motor ohne Zusatzverkehr, dann kontrolliert mit Polling, mehreren Motoren, nativen/fallback Gruppen und Originalfernbedienung prüfen. 50–100 sichere OPEN→STOP-/CLOSE→STOP-Zyklen pro Variante anstreben, nur soweit Motorbelastbarkeit und Umgebung dies zulassen; Reihenfolge zwischen Versuchsserien wechseln.
4. Trigger, Admission, Counter/Transaktions-ID, RX-Sequenz/Epoche, lokale TX-Completion, CCA-Ablehnung/Backoff, FIFO-Recovery, RX-Queue-Drops und Verifikationsresultat gemeinsam loggen. GDO/SPI-Logic-Analyzer und unabhängige Beobachtung von Motorreaktion/Position verwenden; UI-IDLE reicht nicht.
5. STOP-Latenz vom Nutzertrigger bis **physischem Stillstand**, Fehlstopps, erneute Bewegung, Verifikationslatenz/-Fehler, P50/P95/P99/Maximum, RX-Verluste und CCA-/Overflow-Raten erfassen. RX-Freeze-Dauer und Paketfolge-Abstände gezielt messen, einschließlich zwei vollständiger Frames, Folgepräfix, CRC-Fehler und belegtem Kanal. Zulässige Maximalwerte vor dem Versuch festlegen, nicht nachträglich passend wählen.
6. Bei wiederholtem Nachlauf, unzulässiger Bewegung, Motorfehler/Überhitzung, Radio-Fatalfehlern oder unerwarteten Recovery-Spitzen abbrechen. Erst bei reproduzierbarem B-Vorteil und Rückkehr des A-Verhaltens über mehrere Blöcke einen Softwareeffekt diskutieren. Ein RSSI-Unterschied oder einzelner erfolgreicher STOP belegt weder Reichweitengewinn noch Kausalität.

Offen bleiben reale Empfangs-/Antwortzeiten, GDO→SIDLE-Rennen, FIFO-Verhalten unter sehr langen/back-to-back Frames, CCA/RSSI-Einschwingen, wiederholte Counter-Neustarts, Gruppen-Antwortverluste sowie Einflüsse von Versorgung, Antenne und Einbauort. Diese Risiken sind nicht mit Host-Tests geschlossen.

## Weiterführende Dokumentation

- [RF-Verhalten und Zustandssemantik](../rf-reliability.md)
- [ADR: Feedback-Fences und CCA](../adr/2026-09-08-rf-feedback-fences-and-cca.md)
- [Architektur](../architecture.md) und [Entwicklungsdokumentation](../development.md)
- [TI CC1101 Datenblatt SWRS061I](https://www.ti.com/lit/ds/swrs061i/swrs061i.pdf)
- [TI CC1101 Errata SWRZ020E](https://www.ti.com/lit/er/swrz020e/swrz020e.pdf)
