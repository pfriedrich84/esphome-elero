# Kanal-3-Licht: Evidenz und Hardware-Abnahme

Status: **BLOCKED_BY_EVIDENCE**. Diese Änderung liefert Parser-/Diagnoseabsicherung,
keine nachgewiesen funktionierende ON/OFF-Steuerung. Nicht flashen oder deployen,
um vermutete Codes auszuprobieren. Dimmen, RGB und Farbwechsel sind nicht Teil
dieser Untersuchung.

## Primäre Evidenz

Das vollständige [reale Log](../../tests/fixtures/lilygo-elero-channel3-light-2026-10-10.log)
wurde ausgewertet: 186 decodierte RX-Telegramme, davon 151 vom Typ `0x44`,
21 vom Typ `0x6A`, 14 vom Typ `0xCA` (11 vom Licht, 3 von anderen Geräten).
Die Fixture wurde in `cbf99517635e5e45a4a1124a0f115d0ac5cde66f` hinzugefügt;
die verwendete Version enthält die SSID-Redaktion aus `5d1c6fa`.

| Beobachtung | Evidenz | Gesicherte Aussage |
|---|---|---|
| Fernbedienung `0x458130`, Ziel `0xE99B2B`, Kanal 3 | Zeilen 330, 344 | Direkt adressierte `0x6A`-Telegramme existieren. |
| `typ=0x44`, Länge 27, `dst=0x000003`, Kanal 3 | Zeilen 332–336 | Das Ziel ist ein einzelnes Kanalbyte, keine 24-Bit-Geräteadresse. |
| `0x6A`, Länge 29 | Zeilen 344–350 | Drei Zielbytes; unterschiedliche Counter und Hop-/Relay-Felder. |
| Gleicher Counter/Inhalt mehrfach | Zeilen 332/334 und 346/348 | Wiederholung/Weiterleitung ist keine zusätzliche belegte Benutzeraktion. |
| Lichtantwort `0xCA`, Rohzustand `0x10` | Zeile 338 | Antwort vorhanden; ON-Bedeutung nicht nachgewiesen. |
| Lichtantwort `0xCA`, Rohzustand `0x03` | Suche nach `src=0xe99b2b` und `chl=29` | Andere Antwort vorhanden; OFF-Bedeutung nicht nachgewiesen. |
| Antwortkanäle 3, 9, 29, 35, 37, 106, 109, 115, 118, 124, 127 | Alle 11 Lichtantworten | Antwortkanal darf nicht pauschal mit konfiguriertem Bedienkanal gleichgesetzt werden. |
| Frequenzregister `21 71 7A` | Zeile 61 | Mit 26-MHz-Quarz rechnerisch 869,524963 MHz; passt zu den vorgegebenen ca. 869,525 MHz. Keine physikalische Frequenzmessung. |

Keine Zeitmarken kennzeichnen reale Tastenfunktionen oder den tatsächlich sichtbaren
Lichtzustand. Die Muster mit `20`, `40`, `10`, `00` sowie wechselnden letzten Bytes
können Bedienphasen enthalten; die Zuordnung zu Drücken, Halten, Loslassen,
Toggle oder explizitem ON/OFF bleibt offen. Auch `0x6A` mit `10` ist kein belegtes
ON/OFF. Dass direkt adressierte Befehle vorkommen, beweist nicht, dass die
Lichtsteuerung diese Adressierung zwingend benötigt. `0x44` und `0x6A` besitzen
unterschiedliche Zieladressformate; unterschiedliche Bedienmechanismen sind
plausibel, aber nicht belegt.

## Codevergleich und abgesicherte Änderung

- Das bestehende `light: platform: elero` besitzt bereits ON/OFF-Intents und
  konfigurierbare `command_on`/`command_off`. Defaults `20`/`40` sind bestehende
  Konventionen, keine Kanal-3-Protokollevidenz. Gleiches gilt für die bisherige
  Light-Zustandszuordnung `10` → ON und `0F` → OFF.
- `payload_1`/`payload_2` sind die zwei unverschlüsselten Prefixbytes. Im Log der
  direkt adressierten Fernbedienung stehen häufig `00 03`, während der bestehende
  Light-Default `00 04` lautet. `pck_inf1` ist der Pakettyp; `pck_inf2` entspricht
  `typ2` (hier häufig `10`, bei kurzen Frames auch `12`). Hop ist im Mitschnitt
  variabel; ein Empfangs-Relay-Hop ist keine nachgewiesene TX-Vorgabe.
- Der vorhandene Encoder schreibt 24-Bit-Ziele, Systemadresse, Remote-Adresse
  in src/bwd/fwd, konfigurierten Kanal und Prefix, dynamischen Counter sowie
  counterabhängigen Crypto-Code und Parität. Er überschreibt Payloadbytes 2/3
  mit dem Crypto-Code; die acht Bytes ab Offset 2 werden codiert. Die Logausgabe
  zeigt Prefix + acht decodierte Bytes. Nicht einfach Logbytes auf FIFO kopieren.
- Der gemeinsame ProfileDeliveryCoordinator verwaltet Counter, Queue, lokale
  Wiederholungen und Fehler. Wiederholungen eines Intents behalten dessen Counter;
  abgeschlossene/teilweise gesendete Intents dürfen keinen alten Counter wiederverwenden.
  Diese Infrastruktur und die Cover-Policies bleiben erhalten.
- Der Parser kennzeichnet `0x44` separat als `is_channel_command`. Diese Frames
  werden nicht als direkt adressierte Gerätebefehle weitergereicht: keine Adoption
  von Gerät `0x000003`, keine Cover-Statusänderung und keine daraus abgeleitete Aktion.
  Ein short-address Pakettyp wird vom bestehenden Direct-TX-Encoder zurückgewiesen,
  statt mit drei Zielbytes falsch serialisiert zu werden. `0x44`-TX ist weiterhin
  nicht implementiert.
- RX-Statusrouting erfolgt über die Quelladresse des Empfängers, nicht über bwd
  oder den wechselnden Antwortkanal. Die bestehende Entdoppelung/Countersicherung
  für Status bleibt erhalten. Direkte Fernbedienungsbefehle ändern keine Light-
  Zustände; Legacy-Polls bleiben zeitlich begrenzt. Kurze Frames lösen keine Polls aus.
- `get_last_state_raw()` liefert jetzt den letzten tatsächlich empfangenen Rohwert,
  anfangs UNKNOWN, statt aus lokalem ON/OFF einen vermeintlichen Rohstatus zu erzeugen.

## Assumed-State und Konfiguration

`assumed_state: true` ist eine optionale Absicherung für Empfänger ohne belegte
Statussemantik. Default `false` erhält die Legacy-Zuordnung bestehender YAMLs.
Im Assumed-Modus bleibt HA ON/OFF der nach Queue-Annahme geschätzte Sollzustand.
ESPHome Light besitzt keinen dreistufigen physikalischen ON/OFF/UNKNOWN-Wert;
UNKNOWN wird daher ausdrücklich über Logs und Diagnosen kommuniziert. Das ist
kein Hardwarezustand und keine Bestätigung. Die konfigurierte Light-Web-API
liefert zusätzlich `assumed_state` und den numerischen `last_state_raw`;
`last_state` bleibt im Assumed-Modus `unknown` statt unbewiesene Cover-/Light-
Statusnamen auf die Rohbytes anzuwenden.

| Ereignis | Aussage |
|---|---|
| Kommando akzeptiert | Intent in Queue aufgenommen; Log `Light command accepted; physical state unconfirmed`. |
| Lokale Funkübertragung erfolgreich | Bestehende Diagnose `delivery_unconfirmed`; kein Geräte-ACK. |
| Antwort empfangen | `response_unknown`, Rohwert und RSSI/last_seen; keine automatische Interpretation. |
| Zielzustand bestätigt | Nicht verfügbar: weder Statussemantik noch Zuordnung der Antwort zum TX nachgewiesen. |
| Keine Antwort, widersprüchliche oder verspätete Antwort | Physikalischer Zustand bleibt unbekannt. Kein ACK-Timeout, keine Statusbestätigung und kein antwortgetriebenes Retry werden erfunden. Lokale TX-Fehler bleiben über die vorhandene Delivery-Diagnose sichtbar. |
| Neustart | Rohstatus UNKNOWN; restaurierter/initialer HA-Sollzustand ist kein Beleg für den realen Zustand. |

Im Assumed-Modus sind automatische Boot-/Remote-Polls deaktiviert. Der explizite
Refresh-Button und Custom-Befehle können weiterhin das konfigurierte CHECK senden;
auch dessen Bedeutung ist für diesen Empfänger nicht bestätigt. Der Modus verändert
keine TX-Codes, Retry- oder Repeat-Policies und löst das unbekannte Toggle-Risiko
nicht. Bei nachgewiesenem Toggle reicht diese Infrastruktur **nicht**: vor Nutzung
müsste eine eigene sichere Intent-Policy entstehen, ohne blindes erneutes Senden.

Bekannte Entity-Identität (Schema-Beispiel, **keine betriebsfähige oder zur
Hardware-Abnahme freigegebene Konfiguration**):

```yaml
light:
  - platform: elero
    name: "Pergola Licht"
    blind_address: 0xE99B2B
    remote_address: 0x458130
    channel: 3
    assumed_state: true
```

Das nutzt weiterhin unbewiesene Legacy-TX-Defaults. Für einen freigegebenen
Betrieb fehlen exakte ON/OFF-Codes, ggf. Bediensequenz/Adressformat und bewiesene
TX-Profilwerte. Deshalb werden hier **keine Kandidaten als erforderliche
Konfigurationswerte** ergänzt. Die vorhandene Hub-/SPI-Konfiguration bleibt
notwendig; eine bewiesene vollständige Kanal-3-Betriebskonfiguration lässt sich
mit dieser Evidenz noch nicht angeben. Nach Verifikation soll `dim_duration` bei
`0s` bleiben; Dimmen/RGB sind außerhalb dieses Auftrags.

## Reproduzierbare Tests und Grenzen

`test_light_capture.cpp` liest sämtliche 186 decodierten RX-Zeilen direkt aus der
Fixture. Es rekonstruiert ausdrücklich **synthetische** FIFO-Daten mittels inverser
Codierung, erhält dabei die beobachteten Paritätsbytes und vergleicht Parserfelder
mit den unabhängigen Logfeldern. Das ist kein Nachweis erwarteter RX-Wire-Bytes:
diese fehlen im Log. Es prüft beide Zieladressformate, Kanal 3, Typen, Lichtquelle,
Rohzustände und Counter-/Hop-Varianten. Produktions-Entity-Hosttests prüfen
fehlende Antworten, verspätete/abweichende Werte, Duplikate, Legacy-Verhalten und
ON/OFF-Intent-/Counter-Plumbing. Test-Kommandobytes sind keine Protokollbehauptung.

Die tatsächlichen Raw-TX-Bytes eines Cover-CHECK aus Logzeile 224 dienen als
unabhängiger Golden-Vektor für den unveränderten gemeinsamen TX-Serializer.
Das spätere Hardwaretest-Log liefert zusätzlich unabhängige vollständige
HA-TX-Vektoren für die Kandidaten `20`/`40`. Sie sichern die tatsächlich erzeugten
Bytes ab, beweisen jedoch keine funktionierende ON/OFF-Steuerung. Unabhängige
Raw-RX-Vektoren einer erfolgreich schaltenden Originalfernbedienung fehlen. Bestehende Cover-,
Counter-, Dedup-, Radio- und Deliverytests ergänzen diese Tests.

## Fehlende Hardware-Evidenz und Abnahme

Zuerst einen neuen Mitschnitt erstellen, der jede Betätigung zeitlich annotiert:
Ausgangszustand, Taste/Funktion, Drücken/Halten/Loslassen, Dauer und sichtbarer
Endzustand. Jede vermutete ON/OFF-Funktion aus beiden Ausgangszuständen wiederholen;
eine Wiederholung derselben Funktion muss den Zustand erhalten, sonst Toggle-
Verdacht dokumentieren. Raw-RX-FIFO/Wire-Bytes zusätzlich zu decodierten Frames
aufzeichnen. Frequenz/Register und Geräte-/Fernbedienungsmodell notieren.
Erst danach Codes/Sequenz und Antworten festlegen und unabhängige ON/OFF-Wire-
Regressionstests ergänzen. Bis dahin bleibt die Lieferung BLOCKED_BY_EVIDENCE.

Nach dieser Protokollverifikation und ausdrücklicher Flash-Freigabe:

1. Licht sichtbar definiert ausschalten; Zeit und Fernbedienungsaktion notieren.
2. Über HA einschalten; Requested-/TX-Diagnose, Counter und Wire-Bytes aufzeichnen.
3. Tatsächlichen Lichtzustand und sämtliche Funkantworten mit Zeitstempel notieren.
4. Über HA ausschalten und dieselben Informationen aufzeichnen.
5. Tatsächlichen ausgeschalteten Zustand und Antworten dokumentieren.
6. Schritte 2–5 mindestens 20-mal wiederholen, jeden Halbzyklus protokollieren.
7. Schnelle ON/OFF-Wechsel testen; Reihenfolge von Sollwert, TX und realem Endzustand prüfen.
8. Originalfernbedienung und ESPHome abwechselnd bedienen; keine ungewollten Gegenzustände.
9. ESPHome jeweils bei ein- und ausgeschaltetem Licht neu starten; restaurierter Sollwert
   darf nicht als bestätigter Zustand erscheinen. Boot-Telegramme mitprotokollieren.
10. Fehlende Antwort reproduzieren (z.B. kontrolliert Empfänger stromlos); Diagnose
    unbekannt/unbestätigt prüfen. Keine automatische Toggle-Wiederholung zulassen.

Pro Halbzyklus erfassen: Zyklusnummer, Zeit, Ausgangszustand, HA-Ziel, TX-Counter,
Wire-Bytes, Antwortquelle/-counter/-kanal/-hop/-rohstatus, Antwortlatenz,
tatsächlicher Endzustand, unbeabsichtigte Zustandswechsel und Ergebnis.
Abnahme: **20/20 erfolgreiche ON/OFF-Zyklen ohne unbeabsichtigten Gegenzustand**.
Das ist ein initialer Hardwaretest, kein statistischer Zuverlässigkeitsnachweis.

## Ergänzung: Hardwaretest 11:39–11:41

Der Nutzer hat das vollständige neue Log im Gespräch bereitgestellt. Die
[RF-Auszüge](../../tests/fixtures/lilygo-elero-channel3-hardware-rf-excerpts.log)
enthalten ausgewählte Telegramme daraus, ohne Netzwerk-/Gerätestartdaten; sie
sind ausdrücklich kein vollständiger Mitschnitt. Laut Nutzer funktionierten
weder HA-Einschalten noch das anschließende Einschalten mit der Originalremote.
Die Fehlerursache bleibt offen.

| Feld | HA-TX | Originalremote RX |
|---|---|---|
| Typ/Länge inklusive Längenbyte | `6A`, 30 Bytes | `44`, 28 Bytes; daneben `6A`, 30 Bytes |
| typ2 | `10` | `10` oder `12` bei `44`; `10` bei `6A` |
| Ziel | 24-Bit `E99B2B` | Einbyte-Kanal `03` bei `44`, `E99B2B` bei `6A` |
| Hop | `00` | `00`, später `05`; Relays mit `15` |
| Prefix | `00 03` | `00 03`, bei direkten Nullbefehlen auch `02 03` |
| Kandidatenposition | Ganzes Payloadbyte 4, absoluter TX-Offset 24 | Ganzes Payloadbyte 4; absolut 22 bei `44`, 24 bei `6A` |
| Ganzes Payloadbyte 7 | `00` | Bei einigen kurzen Frames `40` |
| Versand | Ein direkter Intent, keine Phasensequenz | Doppelte kurze Frames und verschiedene aufeinanderfolgende Payloads |

Die bestehenden Light-Intents verwenden denselben Direct-Serializer wie Covers.
Damit wird keine besondere Lichtsequenz erzeugt. Das ist eine belegte Formatlücke,
aber kein Beweis, dass `44` für ON/OFF erforderlich ist. Ein bloßer YAML-Wechsel
auf `pck_inf1: 0x44` kann den fehlenden Einbyte-Encoder nicht ersetzen und wird
absichtlich zurückgewiesen. Kein neues Profil wird als funktionierend freigegeben,
solange die benötigte Sequenz unbekannt ist.

Um 11:40:13 folgen auf kurze `20`-Frames mit Counter 3 zwei Nullframes mit
Counter 4: erster Abstand 145 ms, Duplikate nach 18 bzw. 17 ms. Um 11:41:00
folgen `20`/Payloadbyte7=`40` und `10`/Payloadbyte7=`40` nach 160 ms. Weitere
`20`/Null-Paare liegen 127 ms und 1075 ms auseinander. Das passt als Hypothese
zu Bedienphasen; ohne Tastenannotation sind Drücken/Halten/Loslassen und deren
Notwendigkeit nicht identifiziert. Direkte Frames erscheinen mit neuem Counter
und Hop `05` nach etwa 407 ms; weitergeleitete Frames verändern bwd/fwd und Hop,
behalten Counter/Payload. Diese Beobachtungen rechtfertigen keine feste
ON/OFF-Sequenz und kein blindes Retry.

Nach HA-TX erscheinen `CA`-Antworten mit Rohwert `11` (11:39:32.705 und
11:39:55.074), ebenso nach Originalremote-Telegrammen. Das gesamte Payloadbyte 6
ist dabei `14`; bei den früheren `03`/`10`-Antworten unterscheiden sich weitere
Bytes. Weder Fehler-, Helligkeits- noch ON/OFF-Semantik ist belegt. Antwort-`chl`
ist nach HA-TX `02`/`03`, passend zu TX-Countern 2/3; nach direkten Remote-Countern
6/8 sind es `06`/`08`. Ein Counter-Echo ist eine plausible, noch nicht bestätigte
Hypothese und keine Zielzustandsbestätigung.

Das letzte decodierte Payloadbyte ist dynamisch: beispielsweise `40` gegenüber
`C0` auch bei sonst gleichen Nutzbytes. Der Decoder entfernt die ursprünglichen
Crypto-Key-Bytes; deshalb lässt sich aus decodierten Logs kein eindeutiger
Original-Wire-Frame rekonstruieren. Der zusätzliche Vergleichstest bewahrt diese
Abweichung ausdrücklich, statt ein konstantes letztes Byte zu erfinden.
VERBOSE protokolliert jetzt auch gültige RX-Wire-Bytes inklusive Längenbyte vor
der Statusentdoppelung und ohne CC1101-RSSI/LQI-Anhang. Damit kann der nächste
annotierte Mitschnitt eine unabhängige Encoderreferenz liefern.

Die frühere Aussage, `21 71 7A` entspreche 868,350 MHz, war ein Rechenfehler.
Die Softwareanzeige berechnet 869,524963 MHz aus denselben konfigurierten
Registern; sie ist keine unabhängige RF-Messung. Ein Regressionstest sichert den
korrekten Registerwert ab. P1 bleibt **BLOCKED_BY_EVIDENCE**.
