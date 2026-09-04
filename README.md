# Bambutton (ESP32-C3, C++)

Ein physischer Knopf, der in [Bambuddy](https://bambuddy.cool) die Druckplatte freigibt.
Die LED zeigt den Druckerzustand, ein Tastendruck meldet die Platte frei und stößt
den nächsten Job an. **Ein ESP32-C3 bedient zwei Drucker.**

Diese Firmware ist eine Neuentwicklung in C++ (Arduino/PlatformIO). Sie ersetzt die
frühere MicroPython-Variante und behebt deren Kernprobleme:

| Problem vorher | Jetzt |
|---|---|
| Board war nach einigen Minuten nicht mehr erreichbar | WLAN-Stromsparmodus wird abgeschaltet (`WiFi.setSleep(false)`) |
| Fehler landeten nur auf der seriellen Konsole | Jeder HTTP-Status, die Antwort und die Dauer stehen in der Weboberfläche |
| Oberfläche fror während API-Aufrufen ein | Bambuddy-Aufrufe laufen in einem eigenen Task |
| Update war ein Datei-Puzzle aus mehreren `.py` | **Eine** `firmware.bin` — im Browser flashbar, danach OTA |
| Gerät hieß im Netzwerk irgendwie | Setzt einen echten Hostnamen (Standard `bambutton`) |

## Installieren

**Der einfachste Weg:** die Flash-Seite im Browser öffnen (GitHub Pages dieses Repos),
Board per Datenkabel anstecken, „Verbinden & installieren" klicken. Chrome oder Edge
am Desktop nötig (Web Serial).

Alternativ lokal:

```bash
pio run -t upload
```

## Einrichten

1. Nach dem Flashen öffnet das Board das offene WLAN **`Bambutton-Setup`** (kein Passwort).
2. Handy verbinden — die Einrichtungsseite öffnet sich automatisch (Captive Portal).
3. WLAN wählen, Passwort eingeben, speichern. Das Board startet neu und verbindet sich.
4. Danach die Weboberfläche unter der IP des Boards aufrufen: Bambuddy-Adresse
   (`IP:Port`) und API-Key eintragen, Drucker laden, den zwei Knöpfen zuordnen, speichern.

Die Taste beim Einschalten gedrückt halten erzwingt wieder das Setup-Portal.

## Verdrahtung

```
Knopf A:  LED = GPIO3,  Taster = GPIO4
Knopf B:  LED = GPIO5,  Taster = GPIO6
```

Taster gegen `3V3` (interner Pull-down, Auslösung bei steigender Flanke),
LED über Vorwiderstand gegen `GND`. Keine 5 V an die GPIOs.

## LED-Bedeutung

| Muster | Bedeutung |
|---|---|
| aus | kein Drucker zugeordnet |
| folgt dem Kammerlicht | Normalbetrieb |
| langsames Blinken | Platte muss geräumt werden |
| sehr schnelles Blinken | Anfrage läuft gerade |
| schnelles Blinken auf Knopfdruck in der Oberfläche | „Identify" — zeigt, welcher Knopf gemeint ist |

## Diagnose

Der Bereich **Status & Diagnose** in der Weboberfläche zeigt live den Zustand beider
Stationen, die Zahl der Abfragen und Fehler sowie den letzten Fehler im Klartext.
Mit **„Knopf jetzt testen"** löst du exakt denselben `clear-plate`-Aufruf aus wie ein
echter Tastendruck und siehst HTTP-Status, Antwort und Dauer direkt — ohne serielle Konsole.

## Firmware-Update

- **Über die Oberfläche:** neue `firmware.bin` hochladen, das Board startet neu.
- **Über den Browser:** erneut über die Flash-Seite installieren.

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
