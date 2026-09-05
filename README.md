# Bambutton (ESP32-C3, C++)

Ein physischer Knopf, der in [Bambuddy](https://bambuddy.cool) die Druckplatte freigibt.
Die LED zeigt den Druckerzustand, ein Tastendruck meldet die Platte frei und stößt
den nächsten Job an. **Ein ESP32-C3 bedient zwei Drucker.**

## Was Version 2.0 ändert

Version 2.0 ist eine Überarbeitung der WLAN-Einrichtung und der Verbindungsverwaltung.
Die häufigsten Probleme der 1.x-Firmware und ihre Ursachen:

| Problem vorher | Ursache | Jetzt |
|---|---|---|
| WLAN-Einrichtung schlug oft fehl, Board blieb im Setup-Netz | Der ESP32-C3 Super Mini hat eine schlecht angepasste Antenne; mit voller Sendeleistung (19,5 dBm) kommt die Verbindung häufig gar nicht zustande. Die Firmware hat die Leistung nie begrenzt. | Sendeleistung standardmäßig 8,5 dBm (in der Oberfläche änderbar) |
| Nach einem Stromausfall blieb das Board dauerhaft im Setup-Netz | Genau ein Verbindungsversuch von 20 s beim Start, danach nur noch Setup-Netz und nie wieder ein Versuch. Der Router braucht nach einem Stromausfall aber 1–3 Minuten. | Das Board versucht endlos, ins Heimnetz zu kommen. Das Setup-Netz erscheint nur, solange keine Verbindung steht, und schließt sich danach von selbst. |
| Falsches Passwort → keine Rückmeldung, Board startet neu und ist weg | Speichern und Neustart ohne Test | Der Verbindungstest läuft live, während das Handy im Setup-Netz bleibt. Fehlermeldung im Klartext (Netz nicht gefunden, Passwort abgelehnt, keine IP vom Router …). Gespeichert wird nur, was funktioniert. |
| Nach der Einrichtung war das Board nicht auffindbar | Kein mDNS, Hostname wurde nicht gesetzt | Erreichbar als `http://bambutton.local/`; IP und Name werden nach dem Verbinden angezeigt |
| Setup-Seite öffnete sich auf manchen Handys nicht | DNS-Antworten wurden bei modernen Geräten (EDNS) verweigert, keine Umleitung der Systemabfragen | Eigener Captive-Portal-DNS, Umleitung für Android, iOS und Windows |
| Oberfläche und Knöpfe froren bei „Drucker laden“ / „Knopf testen“ ein; Watchdog-Neustart alle 3 Minuten im Setup-Modus | Bambuddy-Aufrufe liefen im Hauptprogramm; der Bambuddy-Task wurde im Setup-Modus nie gestartet, sein Herzschlag blieb stehen | Alle Bambuddy-Aufrufe laufen im Hintergrund-Task, die Seite fragt das Ergebnis ab |
| Tastendruck wurde Minuten später ausgeführt, wenn das WLAN zurückkam | Druck blieb unbegrenzt in der Warteschlange | Druck verfällt nach 10 s; die LED flackert kurz, wenn er nicht zugestellt werden konnte |
| „Knopf testen“ meldete Fehler, obwohl alles ging | Bambuddy antwortet mit HTTP 400, wenn kein Druck auf die Freigabe wartet | Wird als „Verbindung OK, nichts zu räumen“ angezeigt |

Weiterhin dabei: WLAN-Stromsparmodus aus, alle Fehler in der Weboberfläche, eine
`firmware.bin` (im Browser flashbar, danach OTA), Watchdog gegen Hänger.

## Installieren

**Der einfachste Weg:** die Flash-Seite öffnen —

### 👉 https://thomansky.github.io/bambutton-better/

Board per Datenkabel anstecken, „Verbinden & installieren" klicken, fertig.
Chrome oder Edge am Desktop nötig (Web Serial); Firefox und Safari können das nicht.

Alternativ lokal:

```bash
pio run -t upload
```

## Einrichten

1. Nach dem Flashen öffnet das Board das WLAN **`Bambutton-Setup`** (ohne Passwort;
   in der Oberfläche lässt sich später eines setzen).
2. Handy damit verbinden. Die Einrichtungsseite öffnet sich von selbst; falls nicht,
   im Browser **http://192.168.4.1/** eingeben.
3. Dein WLAN aus der Liste wählen (nur 2,4 GHz), Passwort eingeben, optional den
   Gerätenamen ändern, **„Verbinden & speichern"**. Das Board probiert die Verbindung
   sofort aus, während dein Handy im Setup-Netz bleibt. Nach wenigen Sekunden steht
   das Ergebnis auf der Seite: bei Erfolg **IP-Adresse und Name** (z. B.
   `http://bambutton.local/`), bei Misserfolg der Grund.
4. Handy oder PC wieder mit dem Heimnetz verbinden und `http://bambutton.local/`
   (oder die angezeigte IP) öffnen. Das Setup-Netz schließt sich etwa 90 s nach der
   erfolgreichen Verbindung von selbst.
5. Bambuddy-Adresse (`IP:Port`) und API-Key eintragen, **„Verbindung testen & Drucker
   laden"**, den zwei Knöpfen Drucker zuordnen, speichern.

Der API-Key wird in Bambuddy unter *Settings → API Keys* angelegt und braucht die
Rechte **printers:read** und **printers:clear_plate**.

### Wann das Setup-Netz erscheint und wann es verschwindet

- Es ist offen, solange **kein WLAN gespeichert** ist.
- Es erscheint **automatisch**, wenn das gespeicherte WLAN beim Start nach 30 s nicht
  erreichbar ist oder später für mehr als eine Minute wegbleibt. Das Board versucht
  parallel weiter, ins Heimnetz zu kommen (Stromausfall: der Router kommt irgendwann
  wieder, das Board verbindet sich dann ohne Zutun).
- Es **schließt sich von selbst**, sobald das Heimnetz steht: 90 s nach dem Verbinden,
  wenn niemand mehr im Setup-Netz ist, spätestens nach 5 Minuten.
- Es lässt sich jederzeit in der Oberfläche (Erweitert → „Setup-Netz für 10 Minuten
  öffnen") oder durch **Halten von Knopf A beim Einschalten** öffnen.
- Ohne jede Verbindung startet das Board nach 30 Minuten neu, sofern niemand im
  Setup-Netz ist (Selbstheilung, falls sich der WLAN-Stack verhakt hat).

## Verdrahtung

```
Knopf A:  LED = GPIO3,  Taster = GPIO4
Knopf B:  LED = GPIO5,  Taster = GPIO6
```

Taster gegen `3V3` (interner Pull-down, Auslösung bei steigender Flanke),
LED über Vorwiderstand gegen `GND`. Keine 5 V an die GPIOs. Die Pins sind in der
Firmware festgelegt (`src/Settings.cpp`).

## LED-Bedeutung

| Muster | Bedeutung |
|---|---|
| aus | kein Drucker zugeordnet (oder Ruhezustand „immer aus“) |
| folgt dem Kammerlicht | Normalbetrieb (umstellbar auf „immer an“ / „immer aus“) |
| langsames Blinken (0,4 s) | Platte muss geräumt werden |
| sehr schnelles Blinken | Anfrage an Bambuddy läuft gerade |
| zwei ruhige Blinker nach dem Druck | Bambuddy hat die Freigabe angenommen |
| kurzes Flackern nach dem Druck | Freigabe abgelehnt oder nicht zustellbar (kein WLAN, Bambuddy nicht erreichbar, Drucker wartet nicht) |
| kurzer Blitz alle 2 s | keine Verbindung: kein WLAN oder Bambuddy antwortet nicht |
| schnelles Blinken auf Knopfdruck in der Oberfläche | „Identify" — zeigt, welcher Knopf gemeint ist |

## Diagnose

Der Bereich **Status & Diagnose** in der Weboberfläche zeigt live: WLAN-Zustand mit
Empfangsqualität, Grund des letzten fehlgeschlagenen Verbindungsversuchs, ob das
Setup-Netz offen ist, pro Knopf den Drucker (online/offline, Zustand, Platte, Licht)
und den letzten Fehler, sowie Zahl und Fehler der Bambuddy-Abfragen.
**„Knopf jetzt testen"** löst exakt denselben `clear-plate`-Aufruf aus wie ein echter
Tastendruck und zeigt HTTP-Status, Antwort und Dauer — ohne serielle Konsole.

Die serielle Konsole (115200 Baud) protokolliert zusätzlich jedes WLAN-Ereignis mit
Grund, alle 30 s eine Gesundheitszeile (Heap, RSSI, Setup-Netz, Abfragen) und jeden
Bambuddy-Fehler.

## Fehlersuche

**Verbindung zum WLAN klappt nicht**
- Der ESP32-C3 kann nur **2,4 GHz**. Bei Routern mit gemeinsamem Namen für 2,4 und
  5 GHz funktioniert es meist trotzdem; sonst ein reines 2,4-GHz-Netz anlegen.
- „Passwort abgelehnt" trotz richtigem Passwort: reine **WPA3**-Netze werden nicht
  unterstützt, WPA2 oder WPA2/WPA3-Mischbetrieb einstellen.
- „Router hat die Verbindung abgebrochen" / „Netzwerk nicht gefunden" bei kurzer
  Entfernung: **Sendeleistung** unter *Erweitert* prüfen. 8,5 dBm ist für den Super Mini
  fast immer die beste Wahl; höhere Werte verschlechtern die Verbindung häufig.
- Schlechte USB-Stromversorgung (dünnes Kabel, schwaches Netzteil) lässt das Board beim
  Senden abstürzen — anderes Kabel/Netzteil probieren.
- Versteckte Netze: „anderes Netz (manuell)" wählen und den Namen eintippen.

**Setup-Seite öffnet sich nicht von selbst**
- Im Browser `http://192.168.4.1/` eingeben. Mobile Daten kurz abschalten, wenn das
  Handy sonst über LTE ins Internet geht.

**Board nach der Einrichtung nicht auffindbar**
- `http://bambutton.local/` funktioniert auf iPhone, Mac, Windows 10/11 und den meisten
  Android-Geräten; sonst die auf der Setup-Seite angezeigte IP verwenden oder im
  Router unter dem Hostnamen (Standard `bambutton`) nachsehen.

**Bambuddy-Fehler**
- *HTTP 401/403*: API-Key falsch oder ohne die Rechte `printers:read` /
  `printers:clear_plate`.
- *HTTP 404*: Adresse zeigt nicht auf Bambuddy oder die Drucker-ID existiert nicht mehr.
- *HTTP 400 beim Test*: kein Fehler — der Drucker wartet gerade nicht auf eine
  Plattenfreigabe.
- *HTTP 429*: Bambuddy begrenzt Abfragen (100/min); das Board schaltet automatisch für
  zwei Minuten auf ein längeres Intervall. Intervall in Schritt 2 erhöhen.
- *Keine Verbindung / Zeitüberschreitung*: Adresse als `IP:Port` (z. B.
  `192.168.1.50:8000`), kein `https`, Firewall des Bambuddy-Rechners prüfen.

## Firmware-Update

- **Über die Oberfläche:** neue `firmware.bin` hochladen (Erweitert), das Board startet
  neu. Falsche Dateien (bootloader.bin, partitions.bin) werden abgewiesen.
- **Über den Browser:** erneut über die Flash-Seite installieren. Einstellungen bleiben
  erhalten, wenn beim Flashen nicht „Erase" gewählt wird.

## Selbst bauen

```bash
pip install platformio
pio run                 # baut .pio/build/esp32-c3-super-mini/firmware.bin
pio device monitor      # serielle Ausgabe, 115200 Baud
```

GitHub Actions baut die Firmware bei jedem Push automatisch und hängt die Binärdateien
an jeden Tag (`v*`) als Release an.

## Lizenz

MIT
