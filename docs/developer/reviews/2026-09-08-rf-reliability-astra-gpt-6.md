# RF-Reliability- und Regressionsreview

**Review von Astra GPT 6**

**Datum: 08.09.2026**

**Repository:** `pfriedrich84/esphome-elero`

**Geprüfter Commit:** `f15974bb8d6f40cbb728b823bc2e98d96f6a360b`

> Archivierung des ursprünglich read-only durchgeführten Reviews auf ausdrücklichen Nutzerwunsch. Die nachträgliche Ablage dieses Berichts ist eine reine Dokumentationsänderung; die beschriebenen Fixes wurden nicht implementiert. Aussagen über den aktuellen Code beziehen sich ausschließlich auf den oben genannten Commit. Temporäre Host-Reproduktionen lagen außerhalb des Checkouts und sind nicht Bestandteil dieser Ablage.

## 1. Verdict

**Der aktuelle Code enthält mehrere belegte Mechanismen, die unzuverlässiges STOP erklären können. Eine tatsächliche Verringerung der physikalischen Funkreichweite ist dagegen nicht nachgewiesen.**

Die wichtigsten Ergebnisse:

1. **STOP kann fälschlich als bestätigt gelten.** UNKNOWN, TIMEOUT und andere nicht als Bewegung klassifizierte Zustände beenden die Verifikation. Auch eine vor STOP empfangene, später zugestellte Statusmeldung kann dafür genügen.
2. **Die RX-FIFO-Verarbeitung kann korrekt empfangene Antworten verlieren.** Sie liest alle verfügbaren Bytes, verarbeitet aber nur das erste Paket dieses Lesevorgangs.
3. **Ein erneut manuell ausgelöster STOP kann nach fehlgeschlagener Verifikation drei Sekunden blockiert bleiben.**
4. **Nicht alle Bedienwege besitzen dieselbe STOP-Verifikation.** Web-/Custom-Button-Befehle umgehen deren Aktivierung; native Gruppenbefehle berücksichtigen die Verifikationssperren ihrer Mitglieder nicht vollständig.
5. **CCA ist im normalen Sendepfad faktisch umgangen.** Zusammen mit fehlenden reservierten Antwortfenstern kann das Kollisionen und Empfangsunterbrechungen begünstigen.
6. **Lokales TX-Ende bestätigt keine Motorreaktion.** Normale Befehle werden standardmäßig einmal gesendet; eine ausbleibende Motorantwort löst keine erneute Befehlszustellung aus.

Damit ist eine **softwarebedingt schlechter wirkende Reichweite bei unveränderter Signalqualität plausibel**. Welcher Mechanismus beim Nutzer tatsächlich auftritt, bleibt ohne geflashten Commit, Konfiguration und korrelierte Motor-/RF-Beobachtung offen. Die zuverlässige Originalfernbedienung ist ein wichtiges Indiz, aber kein Beweis gegen ein Problem mit Antenne, Versorgung oder CC1101 des ESP-Aufbaus.

### Ausgangsbeobachtung

- ESP nutzt `main`; tatsächlich geflashter Commit unbekannt.
- Befehle kommen gelegentlich nicht an; insbesondere STOP muss manuell wiederholt werden.
- Reichweite fühlt sich seit ungefähr zwei Monaten schlechter an; Zeitangabe unsicher.
- Original-Elero-Fernbedienung funktioniert zuverlässig.
- Eine Firmwareursache ist damit plausibel, aber nicht bewiesen.

### Baseline und Arbeitsweise

- `origin/main` wurde abgerufen und zum Abschluss des Reviews nochmals gegen den Remote geprüft.
- Exakter Stand: **[`f15974bb8d6f40cbb728b823bc2e98d96f6a360b`][head]**, Commitdatum **22.07.2026**.
- **Identisch mit dem früheren Reviewstand.**
- Untersucht: Juni–September 2026; relevante Mechanismen zusätzlich bis 2024 zurückverfolgt. Auf dem abgerufenen `main` liegen keine neueren August-/September-Commits.
- Repositorydaten belegen **kein Flashdatum**.
- `AGENTS.md` und relevante Governance-Dokumente wurden beachtet. Der ursprüngliche Review endete mit sauberem Checkout und ohne Implementierungsänderungen, Commits, Pushes, Issues oder PRs. Die spätere Dokumentationsablage ist davon getrennt.

## 2. Befundtabelle

**P1:** zuerst beheben, da unmittelbar STOP-relevant.

**P2:** relevante Zuverlässigkeitslücke bzw. last-/konfigurationsabhängiger Verstärker.

**P3:** defensive Absicherung; kein belegter Hauptverursacher.

„Reproduziert“ bezeichnet nachfolgend **Host-Softwaretests, nicht Hardwarevalidierung**.

| Befund | Priorität | Aktueller Codebeleg | Einführung / historische Zuordnung | Symptombezug | Evidenzgrad |
|---|---|---|---|---|---|
| Nichtbewegungszustand wird mit bestätigtem Stillstand gleichgesetzt; keine STOP-bezogene Frischeprüfung | **P1** | [Statusbewertung][stop-state], [RX-Zuordnung][dispatch] | Verifikation seit [09c4db5], 28.02.2026; fehlende Frischeprüfung bereits dort | Verlorener STOP wird nicht weiter verfolgt, wenn ungeeigneter Status eintrifft | **Reproduziert** |
| FIFO-Gesamtinhalt wird gelesen, aber nur ein Paket interpretiert | **P1** | [process_rx()][rx] | Grundmuster seit [5625075], 27.01.2024; heutiges „erstes Paket trotz zusätzlicher Bytes“ seit [43c89a3], 18.05.2024 | Motorantworten verschwinden trotz erfolgreichem RF-Empfang | **Reproduziert**, Hardwarebedingungen eingegrenzt |
| Neuer manueller STOP unterliegt dreisekündigem Fehler-Cooldown | **P1** | [Fehlerbehandlung][stop-retry], [Lane-Auswahl][selection] | Funktionierende explizite Cooldown-Sperre: [d1d3162], 18.07.2026; heute im Coordinator | Gerade die manuelle Wiederholung kann verspätet gesendet werden | **Reproduziert** |
| Web-/Custom-Button-STOP aktiviert keine Cover-Verifikation | **P2** | [Web-Aufruf][web-stop], [Button][button], [submit_intent()][submit] | Direkter Web-Queue-Weg bereits im [140bb70], 22.02.2026; heutiger semantischer Weg seit [b6030d2], 18.07.2026 | Bedienwegabhängig fehlen CHECK/zusätzlicher STOP bei Antwortverlust | **Reproduziert** für direkten Intent-Aufruf |
| Native Gruppenbewegung kann während Mitglieds-STOP-Verifikation senden; konkurrierender STOP kann fremden STOP-Burst verkürzen | **P2** | [Gruppenweg][group], [Preemption][selection] | Native Sonderwege seit [c52bffa], 06.04.2026; heutige Coordinator-Form seit [f9f0bf0], 18.07.2026 | Andere Bewegung kann nach STOP erneut starten; weniger STOP-Pakete als vorgesehen | **Lane-/Coordinator-Ablauf reproduziert** |
| IDLE vor STX umgeht CCA; keine garantierte Antwortphase | **P2** | [TX-Vorbereitung][send], [Radio-Scheduling][radio-loop] | CCA-Umgehung ausdrücklich seit [47eae6e], 23.02.2026; TX-first/Cooldown-Verkürzung [f9c84ab], 03.04.2026 | Kollisionen und abgebrochener Antwortempfang statt geringerer Sendeleistung | **Code + TI belegen Mechanismus**; Häufigkeit ungemessen |
| `send_delay` begrenzt nicht mehr den Abstand zwischen unterschiedlichen Intents | **P2**, bei explizitem Delay | [Attempt-/Reset-Logik][coordinator], [Auswahl/Reset][selection] | [f9f0bf0], 18.07.2026 | Zuvor entzerrte Befehlsfolgen können dichter werden | **Vorher/nachher reproduziert** |
| Verworfene Statuscounter verlängern die notwendige Empfangspause für Resync | **P2** | [Counterentscheidung][counter], [Integration][interpret] | [69843f6], 13.05.2026 | Neustartende Counter können lange gültige Statusinformationen ausblenden | **Reproduziert**, bewusst implementierte Quiet-gap-Policy |
| Normale Zustellung endet ohne Motorbestätigung; standardmäßig ein Paket | **P2**, Architekturgrenze | [Defaults][defaults], [Completion][coordinator] | Default 5→1: [06aad04], 23.03.2026; heutige lokale TX-Bestätigung [b13770a], 21.07.2026 | Ein einzelner RF-Verlust bleibt ohne Befehlsretry | **Code belegt** |
| Dynamische Statusregister werden einmal gelesen; ISR-Erfolgspfad behandelt Underflow nicht ausdrücklich als Fehler | **P3** | [Statuszugriff][status-read], [advance_tx()][tx] | Einzelzugriff seit Initialimport; ISR-Fastpath [843d170], 09.04.2026 | Seltene falsche Entscheidungen können Recovery oder Zustellung beeinflussen | **TI + Code**; Underflow-Zweig nur mit injiziertem Fehler reproduziert |

**Wichtig:** Einführungsdatum eines Mechanismus und Zeitpunkt einer Nutzerregression sind nicht gleichzusetzen. Mehrere der stärksten Befunde sind deutlich älter als die vermuteten zwei Monate.

## 3. Vollständiger STOP-Ablauf

### 3.1 ESPHome-Aufruf und Annahme

Der reguläre Cover-Aufruf läuft über:

`control(stop)` → `start_movement(IDLE)` → `submit_intent(STOP)`.

Dabei werden STOP-Auslösezeit und geschätzte Position gespeichert, die Verifikation aktiviert und ein ausstehender STOP-Übergang markiert. Der automatische Zwischenpositions-STOP benutzt dieselben wesentlichen Zustände. [Code][control]

Die semantische Lane:

- ersetzt durch STOP ihre ausstehenden Befehle;
- kann identische STOP-Anforderungen **zusammenfassen**;
- nimmt STOP auch bei sonst voller Lane durch Ersetzen an;
- benötigt allerdings eine registrierte, gültige Coordinator-Anbindung.

**Zusammenfassen bedeutet nicht automatisch einen neuen vollständigen Sendeburst oder einen erneuerten Zeitstempel.**

### 3.2 Priorisierung und mögliche Verzögerungen

Der Hub lässt radioübergreifend nur **eine zugelassene TX-Transaktion** zu. Ein bereits zugelassener älterer Befehl wird zunächst abgeschlossen; danach wird zulässiger STOP gegenüber normaler Arbeit priorisiert. Das verhindert, dass ein neuer Counter einen bereits physisch eingequeueten älteren Befehl überholt. [Hub-Gate][admission]

Positiv:

- Normale Befehle können einen bereits wartenden, zulässigen STOP nicht beliebig überholen.
- `QUEUE_FULL` verbraucht kein Fehlerbudget.
- Profile werden innerhalb der Prioritätsklasse rotierend bedient.

Einschränkungen:

- Ein laufendes Paket wird nicht durch STOP abgebrochen.
- Lokale TX-Fehler verursachen Backoff.
- Nach `stop_failed` sperrt `not_before_ms_` die gesamte Lane, **einschließlich eines neuen STOP**.
- Nicht fortschreitende Queue-Einträge können nach mehr als 30 Sekunden verworfen werden.
- Ein STOP einer anderen Lane desselben Profils kann einen bereits teilweise gesendeten STOP abbrechen. Im Test blieb vom vorgesehenen Zweierburst **ein Paket** übrig. Das ist keine vollständige STOP-Verwerfung, reduziert aber seine Redundanz.

### 3.3 Counter und Paketbildung

Der Coordinator hält einen Counter pro vollständigem Remote-Profil, nicht mehr separat pro Cover. Er beginnt bei 1 und läuft von 255 wieder auf 1.

- Wiederholungen eines Intents verwenden denselben Counter.
- Nach erfolgreichem Abschluss wird weitergezählt.
- Unterbrochene, bereits teilweise erfolgreich gemeldete Zustellungen verbrauchen ihren Counter.
- Statuscounter sind eine **andere Verarbeitungsebene**; sie werden nicht mit der STOP-Transaktion korreliert.

Das Paket enthält Counter, Pakettyp, Hop, System-/Kanalinformationen, Remote-Adressen, Zieladressen und codierten Payload. Default-STOP ist `0x10`. [Paketbildung][send]

**Aktuell werden pro STOP-Burst zwei Pakete vorgesehen – unabhängig vom normalen `send_repeats`.** Der zweite STOP ist keine zweite Motorbestätigung.

### 3.4 TX-Erfolg

Core 0:

1. erzwingt mit RadioLib `standby()` IDLE;
2. prüft IDLE;
3. leert TX-FIFO;
4. behandelt erkannten RX-Overflow;
5. setzt die Software-Richtung auf TX;
6. lädt das komplette Paket und strobt STX;
7. beobachtet GDO/MARCSTATE/TXBYTES;
8. meldet Erfolg oder Fehler mit Transaktions-ID zurück.

Seit [b13770a] zählt erst dieses Ergebnis als Übertragungsfortschritt, nicht schon die FreeRTOS-Queue-Annahme.

**Damit sind Queue-Annahme und lokales TX-Ende sauberer getrennt. Motorantwort und bestätigter Stillstand sind davon jedoch weiterhin nicht ausreichend getrennt.**

Bei lokal gemeldetem Fehler gibt es bis zu drei zusätzliche Versuche mit Backoff. Eine lokal erfolgreiche Aussendung, die der Motor nicht empfängt, löst diesen Fehlerretry **nicht** aus.

### 3.5 RX und Statuszuordnung

`process_rx()` liest den FIFO. Der Parser prüft unter anderem Länge, Zielanzahl, Grenzen und CRC-Status und decodiert das Paket. Danach folgen optionales Dedup und Statuscounterprüfung. [RX][rx] · [Parser][parser] · [Filter][interpret]

Akzeptierte Statuspakete werden anhand ihrer **Quell-/Motoradresse** dem Cover zugeordnet. Weitergeleitete Remote-Adresse, Kanal und Counter werden nicht zur Bestätigung einer konkreten STOP-Anforderung herangezogen.

Das ist nicht automatisch falsch: Auch spontane Motorstatusmeldungen und Originalfernbedienungsverkehr sollen nutzbar sein. Es reicht aber **nicht als Nachweis einer STOP-Antwort**.

Besonders relevant:

- `timestamp_ms` entsteht beim Interpretieren, nicht als gesicherter RF-Ankunftszeitpunkt am GDO.
- Die Cover-Verifikation verwendet diesen Metadatenzeitpunkt ohnehin nicht.
- Im Hub werden TX-Completions **vor** wartenden RX-Ergebnissen verarbeitet. Eine alte, bereits empfangene Stillstandsmeldung kann deshalb erst nach Aktivierung der STOP-Verifikation zugestellt werden.
- Ein neueres RX-Counter-Ergebnis bedeutet nicht „nach diesem STOP entstanden“.

### 3.6 Verifikation und Wiederholung

Nach dem ersten lokal erfolgreichen STOP-Paket wird der Cover-Zustand auf IDLE gesetzt und die Verifikation terminiert:

| Ereignis | Aktuelles Verhalten |
|---|---|
| Keine Statusmeldung | Nach etwa 2 s CHECK; nach weiteren etwa 2 s zusätzlicher STOP-Burst |
| Weiterhin keine Statusmeldung | Erneut CHECK; ungefähr 8 s nach erstem TX Verifikationsfehler, zuzüglich Scheduling/TX-Verzögerungen |
| Bewegungsstatus | Zusätzlichen STOP-Burst sofort anfordern, sofern Budget verfügbar |
| Weiterer Bewegungsstatus nach Retry-TX | Verifikation kann sofort scheitern, ohne die nächste Frist abzuwarten |
| Frischer STOPPED-Status | Beendet Verifikation – gewünschter Fall |
| UNKNOWN, TIMEOUT oder unbekannter Wert | **Beendet ebenfalls Verifikation – kein ausreichender Stillstandsnachweis** |
| Alte, spät zugestellte Stillstandsmeldung | **Beendet ebenfalls Verifikation** |
| Ungültige oder weggefilterte Antwort | Erreicht Verifikation nicht; entspricht dort fehlender Antwort |

Die nachträgliche Positionskorrektur benutzt die Zeit bis zur Statusverarbeitung. Darin stecken aber auch Antwort-, Relay- und Queue-Verzögerungen. **Sie misst nicht ausschließlich die tatsächliche Weiterfahrt nach STOP.**

### 3.7 Andere Bedienwege und Gruppen

- Web und Custom-Button rufen direkt `submit_intent()` auf. Dadurch fehlen die Initialisierungsschritte aus `start_movement(IDLE)`. Zwei STOP-Pakete sind möglich, aber normalerweise keine zugehörige Verifikationssequenz.
- Inkompatible Gruppen berücksichtigen bei atomarer Mitgliedszustellung deren Deferred-Status.
- Der native Gruppenweg reicht seinen Intent ohne entsprechende Mitgliedersperre ein. Eine neue native Gruppenbewegung kann deshalb während laufender STOP-Verifikation eines Mitglieds übertragen werden.
- Gruppenantworten und Relay-Kopien erhöhen außerdem RX-Verkehr und FIFO-/Scheduling-Druck.
- Polling reserviert kein Antwortfenster. CHECK kann damit Antworten erzeugen, während bereits die nächste Aussendung vorbereitet wird.

## 4. Historische Regressionen und Gegenargumente

### 4.1 Keine belegte Verringerung von Leistung oder Empfindlichkeit

Ich habe **141 relevante RF-Quelldatei-Commits** hinsichtlich Registerkonfiguration und relevanter RadioLib-Aufrufe reduziert und die tatsächliche aktuelle Initialisierungsreihenfolge geprüft.

Im untersuchten Zeitraum Juni–September fand ich **keine Änderung** an PATABLE, Modulation, Bandbreite, Datenrate, AGC oder Kalibrierungsregistern, die eine verschlechterte physikalische Reichweite belegen würde.

Die wirksame aktuelle Konfiguration kommt **nach** `RadioLib::begin()` aus `reset()`/`init()`. Die Argumente `868.35, 47.607, …, 10` des vorherigen `begin()` sind daher **nicht die abschließenden RF-Einstellungen**. [Setup][setup] · [Register][registers]

Bei angenommenem 26-MHz-Quarz ergeben sich aus den abschließend geschriebenen Registern:

| Größe | Aktueller Standard |
|---|---|
| Frequenzwort | `0x21717A` → ungefähr **869,525 MHz** |
| Modulation | **GFSK**, `MDMCFG2=0x13` |
| Datenrate | ungefähr **76,767 kBaud** |
| RX-Bandbreite | ungefähr **232,143 kHz** |
| Frequenzhub | ungefähr **34,912 kHz** |
| AGC | `C7 / 00 / B2` |
| Kalibrierung | `MCSM0=0x18`: automatische Kalibrierung beim Übergang IDLE→RX/TX |
| PATABLE | achtmal `0xC0` |
| PA-Auswahl | `FREND0=0x10`: `PA_POWER=0`, also Eintrag 0 |

Die Modem-/AGC-/Kalibrierungswerte und PATABLE sind bereits im Initialimport vorhanden. Der heutige YAML-Frequenzdefault wurde mit [01f0c54] am **05.02.2024** korrigiert.

**Leistungsschluss:** Bei GFSK und `PA_POWER=0` wird PATABLE-Eintrag 0 tatsächlich verwendet. Daraus folgt aber keine universelle, gemessene dBm-Angabe: TI unterscheidet unter anderem Referenzbeschaltungen mit unterschiedlichen Induktortypen. Insbesondere ist `0xC0` kein linear vergleichbarer „Leistungsprozentsatz“. **Ein Leistungsrückgang ist weder aus diesem Wert noch aus dem zwischenzeitlichen RadioLib-Initialisierungsparameter ableitbar.**

Primärquelle: **TI CC1101-Datenblatt SWRS061I**, Registerbeschreibungen sowie §24, Tabellen 37–39. [TI-Datenblatt][ti-ds]

### 4.2 Nachweisbare Änderungen mit möglichem Symptombezug

| Änderung | Vorher → nachher | Bedeutung und Gegenargument |
|---|---|---|
| **CCA**, [47eae6e], 23.02.2026 | STX aus RX → IDLE vor STX | Laut TI greift TX-if-CCA bei STX **aus RX**. Aktuell wird diese Prüfung umgangen. Kann Kollisionen begünstigen; wurde historisch aber ausdrücklich gegen dauerhaft abgewiesene TX-Versuche eingeführt. Einfaches Zurückdrehen wäre nicht ausreichend. |
| **Wiederholungsdefault**, [06aad04], 23.03.2026 | 5 → 1 | Verringert Redundanz normaler Befehle. Die Commitbegründung mit Echo-/Dedup-Filter beweist keine zuverlässige Motorzustellung. Andererseits weniger Airtime und Kollisionen; lange vor dem genannten Zeitraum. |
| **Dual-Core**, [52752e0], 26.03.2026 | Gemeinsame Verarbeitung → Radio-Task/Core 0, Ergebnisqueue/Core 1 | Entkoppelt SPI vom ESPHome-Loop, bringt aber FIFO-Ankunft, Parsing und Entity-Dispatch zeitlich auseinander. Kein Beleg, dass dieser Umbau insgesamt schlechter war. |
| **TX-first**, [f9c84ab], 03.04.2026 | 3→1 ms Cooldown; RX-Verarbeitung nach Cooldown zurückgestellt | Reduziert STOP-Sendelatenz, kann aber RX-Antworten zugunsten der nächsten TX verdrängen. PLL-Settling-Zeit ist kein ausreichendes Argument für ein Motorantwortfenster. |
| **Sendepause**, [44219ab], 07.05.2026 | Default 10→0 ms | Verdichtet Befehlsfolgen. Kann bei Verkehr relevant sein, beweist aber weder Kollisionen noch geringere Signalqualität. |
| **Statuscounter**, [1518cfb], 07.05.; [f9043fd], 07.05.; [69843f6], 13.05.2026 | Stale-Filter eingeführt → Resync nach 30 s seit akzeptiertem Status → 30 s seit letzter Counteraktivität, auch verworfener | Letzte Änderung verhindert Fortschritt bei regelmäßigem als stale bewerteten Empfang. Gewollter Schutz gegen alte Wiederholungen, aber problematisch bei legitimen Neustarts. |
| **Gruppenmapping**, [6df83cd], 04.06.2026 | Feste Gruppenbytes → konfigurierte Mitgliedsbytes/Kompatibilitätsprüfung | Für Standard-STOP kein Hinweis auf Reichweitenverlust; behebt vielmehr falsche Befehle bei abweichendem Mapping. |
| **Command-Intent/Profilzustellung**, [b6030d2] und [f9f0bf0], 18.07.2026 | Mehrere Sonderpfade/Counter → semantische Lanes und Profilcounter | Wesentliche Ordnungsverbesserung, aber relevante Änderungen an Timing, Preemption und Sperren. **Mit `send_delay=1000ms` wartet der Vorgänger zwischen OPEN und CHECK; aktueller Code erlaubt den nächsten Intent bereits beim nächsten Aufruf.** |
| **TX-Abschluss und Admission**, [b13770a], [358e067], [52ed6c8], 21.07.2026 | Queue-Annahme als Fortschritt/mehrere zugelassene Pakete → echte lokale Completion/ein radioweiter Slot/Fairness | Belegte Verbesserungen gegen lokale Drops und Überholen. Keine Motor-ACK-Lösung. |
| **STOP-Budget**, [2a665ba], 21.07.2026 | STOP zuvor abhängig von normalen Repeats; fehlende Antwort führte nach CHECK zum Aufgeben → zwei STOP-Pakete plus begrenzter zusätzlicher Burst | **Aktuelle Verbesserung**, keine pauschale Verschlechterung. Die ungeeignete Statusbestätigung bleibt bestehen; neue Sperr-/Gruppeninteraktionen müssen separat betrachtet werden. |
| **Gruppen-Zwischenposition**, [60dadcf], 21.07.2026 | Mitgliedsstarts → bei geeigneten Gruppen synchroner nativer Start, individuelle Stops | Kann gleichzeitige STOP-/Statuslast verändern. Nur relevant bei dieser Gruppenfunktion, nicht für isolierten manuellen Einzel-STOP. |

**Am besten zeitlich passende Softwarekandidaten sind damit die Änderungen vom 18.–21. Juli – insbesondere Timing und STOP-/Gruppeninteraktionen.** Das ist eine Eingrenzung für einen A/B-Test, keine Zuordnung zum unbekannten Nutzerflash.

## 5. Frühere Befunde: unabhängig bewertet

1. **„process_rx liest alles, verarbeitet nur ein Paket“ – bestätigt.**
   Präzisierung: Es kann mehrere Schleifeniterationen ausführen, aber pro FIFO-Gesamtlesung nur ein Paket interpretieren. Die Schleife rettet bereits mitgelesene Folgepakete nicht.

2. **„IDLE vor STX umgeht CCA“ – bestätigt.**
   `standby()` strobt tatsächlich IDLE; anschließend verlangt der Sendepfad IDLE. TI beschreibt CCA für STX aus RX. Das ist hier kein bloßer Verdacht aufgrund von `MCSM1=0x3F`.

3. **„UNKNOWN/TIMEOUT/alte Statusmeldungen bestätigen STOP“ – bestätigt.**
   Alle drei Varianten wurden softwareseitig reproduziert. Es fehlt sowohl eine geeignete positive Stillstandsklassifikation als auch eine STOP-bezogene Frische-/Zuordnungsprüfung.

4. **„Verworfene Counter verhindern Resync-Fortschritt“ – bestätigt, aber eingeschränkt.**
   Sie aktualisieren absichtlich den Aktivitätszeitpunkt. Bei Empfangsabständen unter 30 Sekunden entsteht keine Quiet Gap. **Nicht zwangsläufig dauerhaft:** Ein ausreichend vorwärts liegender Counter oder eine echte Empfangspause wird akzeptiert. Der normale Fünf-Minuten-Poll allein erzeugt dieses Problem nicht.

5. **„Dynamische Register nur einmal; TI-Errata relevant“ – bestätigt als Robustheitslücke.**
   TI nennt insbesondere MARCSTATE sowie RXBYTES/TXBYTES während Änderung. Stabile Register und einzelne Overflow-Bits sind nicht gleichermaßen betroffen. TI beschreibt den Fehler als selten; ohne Messung ist er kein überzeugender Hauptverursacher.

6. **„COMPLETED ist lokales TX-Ende; normale Befehle einmal, kein Retry ohne Antwort“ – bestätigt.**
   Einschränkung: Es existieren lokale Fehlerretries und eine besondere Cover-STOP-Verifikation. **„Gar keine Retries“ wäre falsch.**

## 6. Validierung und Hardware-Erreichbarkeit

### Vorhandene Tests

Ausführung außerhalb des Checkouts:

```text
cmake -S tests -B /tmp/elero-review/build -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/elero-review/build --parallel 4
ctest --output-on-failure
```

Der letzte Befehl wurde im externen Build-Verzeichnis ausgeführt.

**Ergebnis: 297/297 CTest-Einträge bestanden.** Darin sind 283 einzeln registrierte Tests und 14 zusätzliche Suite-Ausführungen enthalten.

Toolchain: GCC **12.2.0**, CMake **3.25.1**, GoogleTest **1.14.0**.

Die vorhandenen Tests enthalten keine vollständige Hardware-/ESPHome-Integration des STOP- und FIFO-Pfades. Insbesondere testet der vorhandene Replay-Test die Quiet-gap-Policy sogar ausdrücklich als gewünschtes Verhalten.

### Gezielte Reproduktionen

Vollständige relevante Methoden wurden **unverändert aus dem geprüften Checkout extrahiert**; Parser, Counter- und Coordinator-Header wurden direkt eingebunden. Ersetzt wurden Umgebungsschnittstellen wie Uhr, SPI/FIFO, FreeRTOS-Queue und State-Publishing. Cover-TXs durchliefen die asynchrone Coordinator-Annahme und passende Completion.

Build mit **AddressSanitizer und UndefinedBehaviorSanitizer**; keine gemeldeten Sanitizerfehler.

| Test | Beobachtung |
|---|---|
| Zwei vollständige, gültige Statuspakete, zusammen 64 FIFO-Bytes | Nur erstes zugestellt; zweites verbraucht |
| Vollständiges Paket + zehn Bytes des nächsten | Präfix verbraucht; späterer Rest ergibt keine zweite Zustellung |
| Dieselben Pakete einzeln ausgelesen | Beide zugestellt |
| STOP lokal abgeschlossen, danach UNKNOWN/TIMEOUT/`0xFF` | Verifikation beendet |
| STOP, danach ältere Metadatenzeit und STOPPED | Verifikation beendet |
| STOP, danach frischer STOPPED | Verifikation korrekt beendet |
| Keine Antwort | Zwei STOP-Pakete, CHECK, zwei STOP-Pakete, CHECK, Fehler nach ungefähr acht Sekunden |
| Neuer STOP unmittelbar nach diesem Fehler | Durch dreisekündigen Cooldown ausgeschlossen |
| Counter 100, Neustart auf 1…24 alle fünf Sekunden | Über 120 Sekunden alle verworfen; nach 30 Sekunden Ruhe Resync |
| Laufender OPEN, danach STOP | STOP wartet auf passende Completion; anschließend neuer Counter |
| RX-Ende während TX-Cooldown | FIFO-Nachprüfung rettet den Empfang – **positiver Gegenbeleg gegen pauschalen IRQ-Verlust** |
| Zweiter Lane-STOP zwischen Wiederholungen | Erster STOP nach nur einem Paket beendet |
| Native Lane sendet OPEN während Mitgliedsverifikation | Sendung zugelassen |

### Sind die FIFO-Zustände mit der CC1101-Konfiguration möglich?

**Es gibt keinen konfigurierten Einpaket-Stopp:**

- `MCSM1=0x3F`: nach RX wieder RX; nach TX RX.
- `PKTCTRL0=0x45`: variable Paketlänge, CRC.
- `PKTCTRL1=0x8C`: angehängte Statusbytes und CRC-Autoflush.
- Zwei typische 32-Byte-FIFO-Pakete passen zusammen in 64 Bytes.

**CRC-Autoflush verhindert nicht automatisch das Ansammeln mehrerer gültiger Pakete.** Es leert bei CRC-Fehler den gesamten FIFO. TI fordert deshalb, das vorherige Paket rechtzeitig auszulesen und das aktuelle nicht vor erfolgreicher CRC-Prüfung zu konsumieren. Die Konfiguration garantiert diese Anforderung nicht selbst.

Bei entsprechend verzögerter Bedienung sind vollständiges Paket plus Folgepaket/-anfang daher plausible Zustände. Das aktuelle Lesen sämtlicher verfügbarer Bytes verletzt außerdem gerade die Trennung zwischen abgeschlossenem und noch empfangenem Paket.

**Grenze:** Weder die notwendige Bedienungsverzögerung noch konkrete FIFO-Zeitverläufe wurden an einem CC1101 gemessen. Ein simuliertes CRC-fehlerhaftes Paket im FIFO prüft nur den Softwarefilter – bei normalem CRC-Autoflush würde die Hardware diesen Fall typischerweise vorher entfernen.

### Konkurrierende TX-/RX-Ereignisse

Zusätzlich wurden zwei Fehlerzustände injiziert:

- Ein als TX-Ende eingeordnetes GDO-Ereignis bei noch gefülltem TX-FIFO führt zum Abbruch.
- GDO-Ende zusammen mit `MARCSTATE=TXFIFO_UFLOW` und leerem FIFO wird im ISR-Fastpath als Erfolg gemeldet.

TI bestätigt, dass GDO-Funktion `0x06` auch bei TX-Underflow abfällt. **Ein normaler Underflow ist hier aber nicht gezeigt:** Produktionspakete werden vollständig vor STX geladen und passen in den FIFO. Dieser Test belegt einen fehlerhaften Fehlerbehandlungszweig, nicht seine Häufigkeit im Normalbetrieb.

Primärquelle: **TI SWRZ020E**, „RX FIFO“, „SPI Read Synchronization Issue“ und „RXFIFO_OVERFLOW Issue“. [TI-Errata][ti-errata]

**Nicht durchgeführt:** echter Funkbetrieb, Motorversuche, vollständiger ESPHome-Firmwarebuild, Laufzeitmessungen auf ESP32 oder CI-Neustarts.

## 7. Kleine priorisierte Fix-Liste – noch nichts implementiert

### 1. STOP-Bestätigung fachlich korrekt machen

**Akzeptanzkriterien:**

- UNKNOWN, TIMEOUT und unbekannte Statuswerte bestätigen keinen Stillstand.
- Fehlerzustände bleiben von erfolgreicher STOP-Bestätigung unterscheidbar.
- Vor STOP entstandene/gepufferte Statusinformationen beenden keine aktuelle Verifikation.
- Eine gültige frische Stillstandsantwort beendet sie zuverlässig.
- Ausbleibende Bestätigung führt zu einem begrenzten, sichtbaren Fehlerzustand; IDLE allein darf nicht als physischer Nachweis gelten.

**Nicht ausreichend:** nur `timestamp_ms > stop_time` ergänzen. Der heutige RX-Zeitstempel entsteht bereits zu spät im Verarbeitungspfad. Auch die Gleichheit von TX- und Statuscounter darf ohne Protokollbeleg nicht verlangt werden.

### 2. RX-FIFO paketweise und TI-konform behandeln

**Akzeptanzkriterien:**

- Zwei vollständige Pakete werden beide genau einmal verarbeitet.
- Ein Folgepräfix bleibt erhalten bzw. wird durch eine ausdrücklich korrekte Empfangsstrategie geschützt.
- Keine konsumierten Teilpakete vor CRC-Abschluss bei aktiviertem Autoflush.
- Overflow, ungültige Länge und verlorenes Endereignis führen zu begrenzter Recovery.
- Tests decken die tatsächliche FIFO-/IRQ-Orchestrierung ab, nicht nur den Einzelpaketparser.

### 3. STOP-Semantik über Bedienwege und Gruppen vereinheitlichen

**Akzeptanzkriterien:**

- Manueller STOP umgeht einen normalen Fehler-Cooldown.
- Cover, Web, Button und Gruppen aktivieren eine konsistente Verifikation.
- Native Gruppenbewegung kann eine Mitglieds-STOP-Sperre nicht unbeabsichtigt umgehen.
- Konkurrierende STOPs reduzieren vereinbarte Zustellgarantien nicht stillschweigend.
- Bereits physisch zugelassene Pakete behalten weiterhin eine korrekte Counter-/Completion-Zuordnung.

### 4. Kanalzugriff und Antwortzeit ausdrücklich modellieren

**Akzeptanzkriterien:**

- CCA-Verhalten ist bewusst definiert und unter belegtem Kanalzustand geprüft.
- Kanalbelegung führt weder zu unbegrenztem Warten noch zu blindem Dauer-STX.
- Antwortempfang und maximale STOP-Latenz besitzen messbare, miteinander vereinbare Grenzen.
- Semantik von `send_delay` ist geprüft und dokumentiert.
- Keine pauschale Lösung durch mehr Wiederholungen.

### 5. Resync und seltene Radiofehler absichern

**Akzeptanzkriterien:**

- Legitimer Counter-Neustart erholt sich auch unter regelmäßigem Empfang innerhalb eines definierten Zeitbudgets.
- Alte Relay-/Replay-Pakete können dieses Verfahren nicht beliebig ausnutzen.
- Dynamische Statusregister werden in den relevanten Situationen erratafest ausgewertet.
- Expliziter Underflow kann nie eine erfolgreiche Completion erzeugen.

## 8. Hardware-A/B-Testplan

1. **Zuerst aktuelle Firmware identifizieren und reproduzierbar bauen:** exakter Komponentencommit, ESPHome-Version, Framework und bereinigte YAML festhalten.
2. **Gleiche Hardware, Versorgung, Antenne, Orientierung, Standort und Konfiguration.** Zunächst ein einzelner Motor ohne Gruppen-/Zusatzverkehr, anschließend kontrolliert mit Polling, Gruppen und Originalfernbedienung.
3. **A/B/A in wechselnder Reihenfolge**, beispielsweise 50–100 sichere OPEN→STOP-/CLOSE→STOP-Zyklen je Variante. Motorbelastbarkeit, Pausen und sichere Verfahrwege beachten.
4. Messen:
   - tatsächlich ausgeführte Befehle;
   - Anteil der STOPs ohne manuelle Wiederholung;
   - Zeit vom Aufruf bis zum **sichtbaren/mechanisch gemessenen Stillstand**;
   - Median, P95 und größte STOP-Verzögerung;
   - zusätzlicher Fahrweg und nicht bestätigte Fälle.
5. Parallel erfassen: TX-/RX-Zeitpunkte, Counter, Zustände, FIFO-/Parserdrops, Recovery, Queue-/Dispatch-Latenzen; möglichst externer RF-Mitschnitt bzw. GDO/SPI-Logic-Analyzer.
6. Nahbereich und festen problematischen Standort getrennt vergleichen. RSSI/LQI ergänzend auswerten: Sie betreffen den **Motor→ESP-Empfang** und nur überlebende Pakete, nicht unmittelbar ESP→Motor-Zustellung.

### Begründete historische Vergleichsreferenz

**[`9f46196e2b42119ad3c4ba343c02358296be25d7`][pre-july]** ist der `main`-Stand unmittelbar vor dem Command-Intent-Umbau vom 18. Juli. Er ist geeignet, **den Juli-Zustellungsblock als Ganzes** gegen aktuell zu isolieren.

Das ist **keine Empfehlung als stabile Produktionsfirmware**: Er enthält ältere bekannte Schwächen und isoliert nicht einen einzelnen Fix. Beide Varianten müssen mit derselben kompatiblen ESPHome-/Framework-Version und gemeinsamer Minimal-YAML gebaut werden. Falls das nicht möglich ist, wäre der Vergleich durch Toolchain-/Konfigurationsunterschiede verfälscht.

## 9. Noch benötigte Informationen

Besonders wichtig:

- **Tatsächlich geflashter Komponentencommit**, nicht nur `ref: main`.
- ESPHome-Version, Arduino/ESP-IDF-Version, Board und CC1101-Modul.
- YAML für:
  - `send_repeats`, `send_delay`, `dedup_window`;
  - Frequenzregister bzw. Laufzeitfrequenzänderungen;
  - Pollintervalle und Fahrzeiten;
  - Gruppen, Remote-/Motoradressen, Kanal, Hop und Paket-/Command-Mapping.
- Auslösender Bedienweg: HA-Cover, Automation, ESPHome-Button, `/elero`-Weboberfläche oder Gruppe?
- Zeitpunkt des letzten nachweislich funktionierenden Builds und späterer OTA-Updates.
- Sanitierte, zeitlich zusammenhängende Logs eines fehlgeschlagenen und eines erfolgreichen STOP samt tatsächlicher Motorreaktion.
- Versorgung, Antenne, Verkabelung, mögliche Hardwareänderungen und reproduzierbare Störsituationen.

**Gesamturteil:** STOP-Verifikation und RX-Verarbeitung liefern die stärksten aktuellen Fehlerbelege. Die Juli-Änderungen liefern zusätzlich konkrete Timing-/Sperr-Regressionskandidaten. **Eine abgesunkene Sendeleistung oder eine kausal bewiesene Firmware-Reichweitenregression lässt sich aus den verfügbaren Informationen nicht behaupten.**

[head]: https://github.com/pfriedrich84/esphome-elero/commit/f15974bb8d6f40cbb728b823bc2e98d96f6a360b
[rx]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero/elero_cc1101.cpp#L59-L136
[stop-state]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero/cover/EleroCover.cpp#L284-L418
[dispatch]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero/elero_protocol.cpp#L124-L217
[stop-retry]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero/cover/EleroCover.cpp#L543-L598
[selection]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero/elero_profile_delivery_coordinator.h#L313-L387
[web-stop]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero_web/elero_web_server.cpp#L672-L681
[button]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero/button/elero_button.cpp#L15-L35
[submit]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero/cover/EleroCover.cpp#L250-L267
[group]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero_group/EleroGroupCover.cpp#L110-L202
[send]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero/elero_cc1101.cpp#L649-L804
[radio-loop]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero/elero.cpp#L250-L372
[coordinator]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero/elero_profile_delivery_coordinator.h#L97-L311
[counter]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero/elero_counter_logic.h#L9-L32
[interpret]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero/elero_protocol.cpp#L65-L163
[defaults]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero/elero.h#L109-L124
[status-read]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero/elero_cc1101.cpp#L629-L637
[tx]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero/elero_cc1101.cpp#L142-L238
[control]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero/cover/EleroCover.cpp#L422-L515
[admission]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero/elero.cpp#L518-L620
[parser]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero/elero_packet_parser.h#L56-L137
[setup]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero/elero.cpp#L395-L427
[registers]: https://github.com/pfriedrich84/esphome-elero/blob/f15974bb8d6f40cbb728b823bc2e98d96f6a360b/components/elero/elero_cc1101.cpp#L508-L579
[ti-ds]: https://www.ti.com/lit/ds/swrs061i/swrs061i.pdf
[ti-errata]: https://www.ti.com/lit/er/swrz020e/swrz020e.pdf
[5625075]: https://github.com/pfriedrich84/esphome-elero/commit/5625075f6b8bcc16e636896f49220e3074ce07ca
[43c89a3]: https://github.com/pfriedrich84/esphome-elero/commit/43c89a3818ffe271f232de05afd2c1e66d8edf75
[09c4db5]: https://github.com/pfriedrich84/esphome-elero/commit/09c4db560a9e08c2d5256c00454a470b195b19f5
[d1d3162]: https://github.com/pfriedrich84/esphome-elero/commit/d1d31621497dc22dce22b44cb780cf63354b7530
[140bb70]: https://github.com/pfriedrich84/esphome-elero/commit/140bb7040a87bd39404b7d2db52197a2357edcb5
[b6030d2]: https://github.com/pfriedrich84/esphome-elero/commit/b6030d28c14c1b8f98764b83c1cb7a3643eceb82
[c52bffa]: https://github.com/pfriedrich84/esphome-elero/commit/c52bffa3a6f848e2c6e1fd2162d6c94034c92015
[f9f0bf0]: https://github.com/pfriedrich84/esphome-elero/commit/f9f0bf0094eb1d445c7295f4c0765a125f3e5377
[47eae6e]: https://github.com/pfriedrich84/esphome-elero/commit/47eae6ef0bc671233094235c48fcdd3186bc32aa
[f9c84ab]: https://github.com/pfriedrich84/esphome-elero/commit/f9c84ab486fa95603a3048432e0a42dff112bc86
[69843f6]: https://github.com/pfriedrich84/esphome-elero/commit/69843f64a91183b0ddef5ed2924365a7a878f00e
[06aad04]: https://github.com/pfriedrich84/esphome-elero/commit/06aad0465091ae4a0d17b3bd88efffd3d407c3ca
[b13770a]: https://github.com/pfriedrich84/esphome-elero/commit/b13770ad5275262b13d88e5f1373565c70e64a40
[843d170]: https://github.com/pfriedrich84/esphome-elero/commit/843d1707ee364375315b865cc38255b4a65c1d72
[01f0c54]: https://github.com/pfriedrich84/esphome-elero/commit/01f0c54085bab446fb2cec80b954b921d7a44c0f
[52752e0]: https://github.com/pfriedrich84/esphome-elero/commit/52752e0db26d63d7147a8a248754ef1f344037b6
[44219ab]: https://github.com/pfriedrich84/esphome-elero/commit/44219abeef93edadcd1216342f2054a6f55e2f51
[1518cfb]: https://github.com/pfriedrich84/esphome-elero/commit/1518cfb7f718401adf1993fb06a3b1d0bda7db4f
[f9043fd]: https://github.com/pfriedrich84/esphome-elero/commit/f9043fd1bce548975607887e213ffd5d6be9c5dd
[6df83cd]: https://github.com/pfriedrich84/esphome-elero/commit/6df83cdfad8044d30cb55a917cfd3a51a4c23bd5
[358e067]: https://github.com/pfriedrich84/esphome-elero/commit/358e067dd2bdaa7881aa0f1d43274bdff79dec15
[52ed6c8]: https://github.com/pfriedrich84/esphome-elero/commit/52ed6c837636d0da44b61973ed22537dc1085097
[2a665ba]: https://github.com/pfriedrich84/esphome-elero/commit/2a665ba9f70d872c8ad8a231b85f8cd7e713f7df
[60dadcf]: https://github.com/pfriedrich84/esphome-elero/commit/60dadcfafaeedb76d4f5140ea10c00aff6533d1c
[pre-july]: https://github.com/pfriedrich84/esphome-elero/commit/9f46196e2b42119ad3c4ba343c02358296be25d7
