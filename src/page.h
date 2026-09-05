#pragma once
#include <Arduino.h>

static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html><html lang="de"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Bambutton</title><style>
:root{--bg:#f1f5f9;--card:#fff;--ink:#0f172a;--mut:#64748b;--line:#e2e8f0;--acc:#2563eb;--ok:#059669;--err:#dc2626;--warn:#b45309}
@media(prefers-color-scheme:dark){:root{--bg:#0f172a;--card:#1e293b;--ink:#f1f5f9;--mut:#94a3b8;--line:#334155;--warn:#f59e0b}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.5 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
.wrap{max-width:560px;margin:0 auto;padding:16px}
h1{font-size:22px;margin:0 0 2px;display:flex;align-items:center;gap:8px}.sub{color:var(--mut);font-size:13px;margin:0 0 14px}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px;margin-bottom:12px}
.step{font-size:12px;text-transform:uppercase;letter-spacing:.05em;color:var(--mut);margin:0 0 10px;font-weight:600}
label{display:block;font-size:13px;color:var(--mut);margin:10px 0 4px}
input,select{width:100%;padding:10px;border:1px solid var(--line);border-radius:8px;background:var(--card);color:var(--ink);font-size:15px}
button{width:100%;padding:11px;border:0;border-radius:8px;background:var(--acc);color:#fff;font-size:15px;font-weight:600;margin-top:12px;cursor:pointer}
button.sec{background:transparent;color:var(--acc);border:1px solid var(--acc)}
button.lang{width:auto;margin:0 0 0 auto;padding:3px 9px;font-size:12px;background:transparent;color:var(--mut);border:1px solid var(--line)}
button:disabled{opacity:.5;cursor:default}
.row{display:flex;gap:8px;align-items:center}.row>*{margin-top:0}
.row .icon{flex:0 0 auto;width:auto;padding:10px 13px}
.msg{margin-top:10px;font-size:13px;padding:9px;border-radius:8px;display:none;white-space:pre-line}
.msg.on{display:block}.ok{background:rgba(5,150,105,.12);color:var(--ok)}.err{background:rgba(220,38,38,.12);color:var(--err)}.warn{background:rgba(180,83,9,.12);color:var(--warn)}
.msg:not(.ok):not(.err):not(.warn){background:var(--bg);color:var(--ink)}
.hide{display:none}
table{width:100%;border-collapse:collapse;font-size:13px}
td{padding:5px 0;border-bottom:1px solid var(--line);vertical-align:top}
td:first-child{color:var(--mut);width:40%}
.badge{font-size:11px;background:var(--bg);border:1px solid var(--line);padding:2px 7px;border-radius:99px;color:var(--mut)}
small{color:var(--mut);font-size:12px;display:block;margin-top:8px}
details>summary{cursor:pointer;color:var(--acc);font-size:14px;font-weight:600;list-style:none;padding:4px 0}
details>summary::before{content:"▸ "}details[open]>summary::before{content:"▾ "}
.links{font-size:13px;margin-top:10px;color:var(--mut)}.links a{color:var(--acc);text-decoration:none}
.prog{margin-top:10px;font-size:13px;padding:9px;border-radius:8px;background:var(--bg);display:none}
.prog.on{display:block}.prog:before{content:"⏳ "}
.chk{display:flex;align-items:center;gap:8px;margin-top:12px;color:var(--ink);font-size:14px}.chk input{width:auto;margin:0}
.dot{display:inline-block;width:9px;height:9px;border-radius:50%;margin-right:5px;background:var(--mut)}.dot.g{background:var(--ok)}.dot.r{background:var(--err)}.dot.y{background:var(--warn)}
</style></head><body><div class="wrap">
<h1>Bambutton <span class="badge" id="ver"></span><button class="lang" id="langBtn" onclick="toggleLang()">EN</button></h1>
<p class="sub" id="lead" data-t="lead_loading">Wird geladen …</p>
<div id="offline" class="msg err" data-t="offline">Keine Antwort vom Board. Ist dein Gerät noch mit dem richtigen WLAN verbunden?</div>

<div class="card">
  <p class="step" data-t="s1">1 · WLAN</p>
  <table id="wifiTab"></table>
  <div id="wifiWarn" class="msg warn"></div>
  <details id="wifiForm">
    <summary id="wifiSum">WLAN einrichten</summary>
    <label data-t="lNet">Netzwerk</label>
    <div class="row">
      <select id="ssid"><option value="" data-t="optChoose">— wählen —</option><option value="__m" data-t="optManual">anderes Netz (manuell) …</option></select>
      <button class="sec icon" onclick="scan(1)" title="Scan">&#8635;</button>
    </div>
    <input id="ssidm" class="hide" data-p="pSsid" placeholder="WLAN-Name (SSID)" style="margin-top:8px">
    <label data-t="lPw">Passwort</label>
    <div class="row">
      <input id="pw" type="password" data-p="pPw" placeholder="WLAN-Passwort (leer = offenes Netz)">
      <button class="sec icon" onclick="togglePw()">&#128065;</button>
    </div>
    <label data-t="lHost">Gerätename im Netzwerk</label>
    <input id="hostname" placeholder="bambutton">
    <button onclick="connectWifi()" data-t="bConnect">Verbinden &amp; speichern</button>
    <div id="prog" class="prog"></div>
    <div id="m1" class="msg"></div>
    <div class="links"><a href="#" onclick="return saveBlind()" data-t="linkBlind">Ohne Test speichern und neu starten</a> · <a href="#" onclick="return forget()" data-t="linkForget">WLAN-Daten löschen</a></div>
    <small data-t="hintWifi">Nur 2,4-GHz-Netze. Beim Verbinden bleibt dein Handy im Setup-Netz und sieht das Ergebnis; die Verbindung kann dabei ein paar Sekunden aussetzen – einfach warten. Schließt sich die Setup-Ansicht des Handys dabei, einfach wieder öffnen: das Ergebnis steht dann hier.</small>
  </details>
  <div id="done" class="hide">
    <div class="msg ok on" id="doneMsg"></div>
    <button class="sec" onclick="finish()" data-t="bFinish">Setup-Netz jetzt schließen</button>
  </div>
</div>

<div class="card">
  <p class="step" data-t="s2">2 · Bambuddy</p>
  <label data-t="lAddr">Adresse (IP:Port)</label>
  <input id="host" data-p="pAddr" placeholder="z. B. 192.168.1.50:8000" inputmode="url">
  <label data-t="lKey">API-Key</label>
  <input id="key" type="password" data-p="pKey" placeholder="API-Key (Rechte: printers:read, printers:clear_plate)">
  <button onclick="loadPrinters()" data-t="bLoad">Verbindung testen &amp; Drucker laden</button>
  <label class="chk"><input type="checkbox" id="apiOn"> <span data-t="chkApi">Bambuddy-Abfrage aktiv</span></label>
  <label data-t="lPoll">Abfrageintervall</label>
  <select id="pollMs"><option value="2000">2 s</option><option value="3000">3 s</option><option value="5000">5 s</option><option value="10000">10 s</option><option value="30000">30 s</option></select>
  <div id="m2" class="msg"></div>
  <small data-t="hintApi">Zum Eingrenzen von Störungen abschaltbar: Das Board bleibt im WLAN erreichbar, fragt aber keine Druckerdaten ab.</small>
</div>

<div class="card">
  <p class="step" data-t="s3">3 · Knöpfe zuordnen</p>
  <label data-t="lBtnA">Knopf A</label>
  <div class="row"><select id="p0"><option value="0" data-t="optNoPrinter">— kein Drucker —</option></select><button class="sec icon" onclick="ident(0)">&#128294;</button></div>
  <label data-t="lBtnB">Knopf B</label>
  <div class="row"><select id="p1"><option value="0" data-t="optNoPrinter">— kein Drucker —</option></select><button class="sec icon" onclick="ident(1)">&#128294;</button></div>
  <label data-t="lIdle">LED im Ruhezustand</label>
  <select id="idleLed"><option value="0" data-t="optIdle0">folgt dem Kammerlicht</option><option value="1" data-t="optIdle1">immer an</option><option value="2" data-t="optIdle2">immer aus</option></select>
  <button onclick="saveAll('m3')" data-t="bSave">Speichern</button>
  <div id="m3" class="msg"></div>
  <small data-t="hintIdent">Die Taschenlampe lässt den passenden Knopf kurz blinken. Speichern übernimmt auch Adresse, Key und Intervall aus Schritt 2.</small>
</div>

<div class="card">
  <p class="step" data-t="s4">4 · Status &amp; Diagnose</p>
  <table id="diag"><tr><td>…</td><td></td></tr></table>
  <button class="sec" onclick="testClear(0)" data-t="bTestA">Knopf A jetzt testen (Platte freigeben)</button>
  <button class="sec" onclick="testClear(1)" data-t="bTestB">Knopf B jetzt testen (Platte freigeben)</button>
  <div id="m4" class="msg"></div>
  <small data-t="hintTest">Der Test löst denselben Aufruf aus wie ein Tastendruck und zeigt Bambuddys Antwort im Klartext. „Nichts zu räumen“ heißt: Verbindung OK, aber kein Druck wartet auf die Freigabe.</small>
</div>

<div class="card">
  <details>
    <summary data-t="s5">5 · Erweitert &amp; Firmware</summary>
    <label data-t="lTx">WLAN-Sendeleistung</label>
    <select id="txPower"><option value="34" data-t="optTx34">8,5 dBm – empfohlen für ESP32-C3 Super Mini</option><option value="44">11 dBm</option><option value="52">13 dBm</option><option value="60">15 dBm</option><option value="68">17 dBm</option><option value="78" data-t="optTx78">19,5 dBm – Maximum</option><option value="20">5 dBm</option></select>
    <small data-t="hintTx">Die Antenne des Super Mini verträgt keine volle Leistung: mit 19,5 dBm bricht die Verbindung oft ab oder kommt gar nicht zustande. Erst bei sehr großer Entfernung schrittweise erhöhen.</small>
    <label data-t="lApPass">Passwort für das Setup-Netz „Bambutton-Setup“</label>
    <input id="apPass" type="password" data-p="pApPass" placeholder="leer = offenes Setup-Netz (8–63 Zeichen)">
    <label data-t="lApTimeout">Setup-Netz ohne Heimnetz automatisch schließen nach</label>
    <select id="apTimeout"><option value="5">5 min</option><option value="15">15 min</option><option value="30">30 min</option><option value="60">60 min</option><option value="0" data-t="optNever">nie</option></select>
    <small data-t="hintApTimeout">Gilt, wenn das Heimnetz nicht erreichbar ist. Wieder öffnen: einen Knopf 5 Sekunden halten, Knopf A beim Einschalten, hier per Schaltfläche oder per USB über die Flash-Seite. Bei bestehender Heimnetz-Verbindung schließt es ohnehin nach 90 s.</small>
    <button onclick="saveAll('m5')" data-t="bSaveSettings">Einstellungen speichern</button>
    <button class="sec" onclick="portal()" data-t="bPortal">Setup-Netz für 10 Minuten öffnen</button>
    <div id="m5" class="msg"></div>
    <label data-t="lFw">Neue firmware.bin</label>
    <input id="fw" type="file" accept=".bin">
    <button class="sec" onclick="ota()" data-t="bOta">Hochladen &amp; neu starten</button>
    <div id="m6" class="msg"></div>
    <button class="sec" onclick="reboot()" data-t="bReboot">Nur neu starten</button>
  </details>
</div>
</div>
<script>
var I={de:{
lead_loading:'Wird geladen …',offline:'Keine Antwort vom Board. Ist dein Gerät noch mit dem richtigen WLAN verbunden?',
s1:'1 · WLAN',wifiSetup:'WLAN einrichten',wifiChange:'WLAN ändern',lNet:'Netzwerk',optChoose:'— wählen —',optManual:'anderes Netz (manuell) …',
pSsid:'WLAN-Name (SSID)',lPw:'Passwort',pPw:'WLAN-Passwort (leer = offenes Netz)',lHost:'Gerätename im Netzwerk',bConnect:'Verbinden & speichern',
linkBlind:'Ohne Test speichern und neu starten',linkForget:'WLAN-Daten löschen',
hintWifi:'Nur 2,4-GHz-Netze. Beim Verbinden bleibt dein Handy im Setup-Netz und sieht das Ergebnis; die Verbindung kann dabei ein paar Sekunden aussetzen – einfach warten. Schließt sich die Setup-Ansicht des Handys dabei, einfach wieder öffnen: das Ergebnis steht dann hier.',
bFinish:'Setup-Netz jetzt schließen',s2:'2 · Bambuddy',lAddr:'Adresse (IP:Port)',pAddr:'z. B. 192.168.1.50:8000',lKey:'API-Key',
pKey:'API-Key (Rechte: printers:read, printers:clear_plate)',pKeySet:'API-Key gespeichert – nur zum Ändern eintragen',
bLoad:'Verbindung testen & Drucker laden',chkApi:'Bambuddy-Abfrage aktiv',lPoll:'Abfrageintervall',
hintApi:'Zum Eingrenzen von Störungen abschaltbar: Das Board bleibt im WLAN erreichbar, fragt aber keine Druckerdaten ab.',
s3:'3 · Knöpfe zuordnen',lBtnA:'Knopf A',lBtnB:'Knopf B',optNoPrinter:'— kein Drucker —',lIdle:'LED im Ruhezustand',
optIdle0:'folgt dem Kammerlicht',optIdle1:'immer an',optIdle2:'immer aus',bSave:'Speichern',
hintIdent:'Die Taschenlampe lässt den passenden Knopf kurz blinken. Speichern übernimmt auch Adresse, Key und Intervall aus Schritt 2.',
s4:'4 · Status & Diagnose',bTestA:'Knopf A jetzt testen (Platte freigeben)',bTestB:'Knopf B jetzt testen (Platte freigeben)',
hintTest:'Der Test löst denselben Aufruf aus wie ein Tastendruck und zeigt Bambuddys Antwort im Klartext. „Nichts zu räumen“ heißt: Verbindung OK, aber kein Druck wartet auf die Freigabe.',
s5:'5 · Erweitert & Firmware',lTx:'WLAN-Sendeleistung',optTx34:'8,5 dBm – empfohlen für ESP32-C3 Super Mini',optTx78:'19,5 dBm – Maximum',
hintTx:'Die Antenne des Super Mini verträgt keine volle Leistung: mit 19,5 dBm bricht die Verbindung oft ab oder kommt gar nicht zustande. Erst bei sehr großer Entfernung schrittweise erhöhen.',
lApPass:'Passwort für das Setup-Netz „Bambutton-Setup“',pApPass:'leer = offenes Setup-Netz (8–63 Zeichen)',pApPassSet:'Passwort gesetzt – zum Ändern eintragen, leeren = offen',
bSaveSettings:'Einstellungen speichern',bPortal:'Setup-Netz für 10 Minuten öffnen',
lApTimeout:'Setup-Netz ohne Heimnetz automatisch schließen nach',optNever:'nie',
hintApTimeout:'Gilt, wenn das Heimnetz nicht erreichbar ist. Wieder öffnen: einen Knopf 5 Sekunden halten, Knopf A beim Einschalten, hier per Schaltfläche oder per USB über die Flash-Seite. Bei bestehender Heimnetz-Verbindung schließt es ohnehin nach 90 s.',
r_apTimedOut:'geschlossen (Zeit abgelaufen) – Knopf 5 s halten zum Öffnen',lFw:'Neue firmware.bin',bOta:'Hochladen & neu starten',bReboot:'Nur neu starten',
m_scan:'Suche Netzwerke …',m_scanFail:'Suche fehlgeschlagen – bitte noch einmal auf ↻ tippen.',m_scanOk:'%1 Netzwerke gefunden (nur 2,4 GHz sichtbar).',
m_scanTesting:'Während eines Verbindungsversuchs wird nicht gesucht.',m_noBoard:'Keine Antwort vom Board.',m_pickSsid:'Bitte ein WLAN auswählen oder eintragen.',
m_connecting:'Verbinde mit „%1“ …',m_wait:'Warte auf das Board … (die Verbindung zum Setup-Netz kann kurz aussetzen, bitte warten)',
m_lost:'Keine Antwort mehr vom Board. Prüfe, ob dein Handy noch mit „Bambutton-Setup“ verbunden ist. Ist das Setup-Netz verschwunden, hat die Verbindung geklappt – das Board ist dann im Heimnetz unter http://%1.local erreichbar.',
m_fail:'Verbindung fehlgeschlagen: %1',m_notArrived:'Die Anfrage ist nicht beim Board angekommen – bitte noch einmal auf „Verbinden“ tippen.',
done_html:'<b>Verbunden mit „%1“.</b><br>IP-Adresse: <b>%2</b><br>Name: <b>http://%3.local/</b><br>Empfang: %4 (%5 dBm)<br><br>Weiter geht es im Heimnetz: Handy oder PC mit „%1“ verbinden und eine der beiden Adressen öffnen, dann Bambuddy eintragen. Das Setup-Netz schließt sich von selbst, sobald du es verlassen hast (frühestens 90 s nach dem Verbinden, spätestens nach 5 Minuten) – oder sofort über den Knopf unten.',
m_closed:'Setup-Netz geschlossen.',m_closing:'Setup-Netz wird geschlossen …',m_armBlind:'Speichert ohne Verbindungstest und startet neu',m_arm:'%1 – zum Bestätigen noch einmal klicken.',
m_savedBlind:'Gespeichert – das Board startet neu und verbindet sich. Klappt das nicht, erscheint „Bambutton-Setup“ nach etwa 30 s wieder.',m_savedReboot:'Gespeichert – das Board startet neu.',
m_armForget:'Löscht die WLAN-Daten und startet ins Setup-Netz',m_forgot:'Gelöscht – Neustart, danach „Bambutton-Setup“.',m_portal:'Setup-Netz „%1“ ist jetzt 10 Minuten offen (http://192.168.4.1/).',
m_timeout:'Zeitüberschreitung',m_bbConnecting:'Verbinde mit Bambuddy …',m_printers:'%1 Drucker geladen (%2 ms). Jetzt in Schritt 3 die Knöpfe zuordnen und speichern.',
m_saving:'Speichere …',m_saved:'Gespeichert.',m_saveErr:'Fehler beim Speichern.',m_sendClear:'Sende clear-plate für Knopf %1 an Bambuddy …',
m_clearOk:'Erfolg – HTTP %1 nach %2 ms. Bambuddy hat die Platte freigegeben.',m_clearNa:'Verbindung OK – %1. Kein Fehler: der Knopf wirkt nur, wenn ein Druck fertig ist und Bambuddy auf die Freigabe wartet.',
m_clearFail:'Fehlgeschlagen: %1',m_reboot:'Neustart …',m_pickFw:'Bitte zuerst eine firmware.bin auswählen.',m_uploading:'Lade %1 kB hoch – nicht trennen …',
m_otaOk:'Update eingespielt – das Board startet neu.',m_otaFail:'Update fehlgeschlagen.',m_otaLost:'Upload abgebrochen (Verbindung verloren). Das Board läuft mit der alten Firmware weiter.',
lead_sta:'Im WLAN „%1“ · %2 · http://%3.local',lead_down:'WLAN „%1“: %2',lead_downSince:' – seit %1 ohne Verbindung',lead_setup:'Ersteinrichtung – verbinde das Board mit deinem WLAN.',
r_conn:'Verbindung',r_connWith:'verbunden mit „%1“',r_down:'„%1“ – %2',r_noWifi:'kein WLAN eingerichtet',r_ip:'IP-Adresse',r_ch:' · Kanal %1',r_sig:'Empfang',r_name:'Name',r_since:'Verbunden seit',r_reconn:' · %1× neu verbunden',
r_ap:'Setup-Netz',r_apOpen:'„%1“ offen (%2 %3, %4)',dev1:'Gerät',devN:'Geräte',noPw:'ohne Passwort',r_apClosed:'geschlossen',
w_last:'Letzter Verbindungsversuch: %1',w_retry:'Das Board versucht weiter, „%1“ zu erreichen (bisher %2 Versuche). Solange keine Verbindung steht, bleibt das Setup-Netz offen; sobald sie steht, schließt es sich von selbst.',w_lastErr:' Letzter Fehler: %1',
st_printer:'Drucker %1',st_noLink:'keine Verbindung',st_online:'online',st_offline:'Drucker offline',st_await:'Platte räumen',st_ready:'bereit',st_light:'Licht %1',on:'an',off:'aus',st_none:'kein Drucker zugeordnet',r_btn:'Knopf %1',
bb_off:'abgeschaltet',bb_unconf:'nicht konfiguriert',bb_none:'noch keine Antwort',bb_ok:'erreichbar',bb_last:'letzte Antwort vor %1',r_polls:'Abfragen',r_pollsV:'%1 (%2 Fehler)',
r_lastErr:'Letzter Fehler',r_lastClear:'Letztes clear-plate',r_board:'Board',r_boardV:'läuft seit %1 · %2 kB frei'
},en:{
lead_loading:'Loading …',offline:'No answer from the board. Is your device still on the right Wi-Fi network?',
s1:'1 · Wi-Fi',wifiSetup:'Set up Wi-Fi',wifiChange:'Change Wi-Fi',lNet:'Network',optChoose:'— choose —',optManual:'other network (manual) …',
pSsid:'Network name (SSID)',lPw:'Password',pPw:'Wi-Fi password (empty = open network)',lHost:'Device name on the network',bConnect:'Connect & save',
linkBlind:'Save without testing and restart',linkForget:'Forget Wi-Fi',
hintWifi:'2.4 GHz networks only. While connecting, your phone stays on the setup network and sees the result; the connection may drop for a few seconds – just wait. If the phone closes its setup view, simply reopen it: the result will be shown here.',
bFinish:'Close setup network now',s2:'2 · Bambuddy',lAddr:'Address (IP:port)',pAddr:'e.g. 192.168.1.50:8000',lKey:'API key',
pKey:'API key (permissions: printers:read, printers:clear_plate)',pKeySet:'API key stored – enter only to change it',
bLoad:'Test connection & load printers',chkApi:'Bambuddy polling active',lPoll:'Poll interval',
hintApi:'Can be switched off to isolate problems: the board stays reachable on Wi-Fi but does not poll printer data.',
s3:'3 · Assign buttons',lBtnA:'Button A',lBtnB:'Button B',optNoPrinter:'— no printer —',lIdle:'LED when idle',
optIdle0:'follows the chamber light',optIdle1:'always on',optIdle2:'always off',bSave:'Save',
hintIdent:'The flashlight makes the matching button blink briefly. Saving also stores address, key and interval from step 2.',
s4:'4 · Status & diagnostics',bTestA:'Test button A now (clear plate)',bTestB:'Test button B now (clear plate)',
hintTest:'The test fires the same call as a real button press and shows Bambuddy’s answer in plain text. “Nothing to clear” means: connection OK, but no print is waiting to be acknowledged.',
s5:'5 · Advanced & firmware',lTx:'Wi-Fi transmit power',optTx34:'8.5 dBm – recommended for ESP32-C3 Super Mini',optTx78:'19.5 dBm – maximum',
hintTx:'The Super Mini’s antenna cannot handle full power: at 19.5 dBm the connection often drops or never comes up. Only raise it step by step for very long distances.',
lApPass:'Password for the setup network “Bambutton-Setup”',pApPass:'empty = open setup network (8–63 characters)',pApPassSet:'Password set – enter to change, clear = open',
bSaveSettings:'Save settings',bPortal:'Open setup network for 10 minutes',
lApTimeout:'Close the setup network automatically after (without home network)',optNever:'never',
hintApTimeout:'Applies while the home network is unreachable. To reopen: hold a button for 5 seconds, hold button A while powering on, use the button here, or use USB via the flash page. With the home network connected it closes after 90 s anyway.',
r_apTimedOut:'closed (timed out) – hold a button 5 s to reopen',lFw:'New firmware.bin',bOta:'Upload & restart',bReboot:'Restart only',
m_scan:'Scanning for networks …',m_scanFail:'Scan failed – please tap ↻ again.',m_scanOk:'%1 networks found (only 2.4 GHz visible).',
m_scanTesting:'No scanning while a connection attempt is running.',m_noBoard:'No answer from the board.',m_pickSsid:'Please choose or enter a network.',
m_connecting:'Connecting to “%1” …',m_wait:'Waiting for the board … (the setup network may drop for a moment, please wait)',
m_lost:'No more answer from the board. Check that your phone is still on “Bambutton-Setup”. If the setup network is gone, the connection worked – the board is then reachable on your home network at http://%1.local.',
m_fail:'Connection failed: %1',m_notArrived:'The request did not reach the board – please tap “Connect” again.',
done_html:'<b>Connected to “%1”.</b><br>IP address: <b>%2</b><br>Name: <b>http://%3.local/</b><br>Signal: %4 (%5 dBm)<br><br>Continue on your home network: connect phone or PC to “%1” and open one of the two addresses, then enter Bambuddy. The setup network closes by itself once you have left it (90 s after connecting at the earliest, after 5 minutes at the latest) – or right now with the button below.',
m_closed:'Setup network closed.',m_closing:'Closing the setup network …',m_armBlind:'Saves without a connection test and restarts',m_arm:'%1 – click again to confirm.',
m_savedBlind:'Saved – the board restarts and connects. If that fails, “Bambutton-Setup” reappears after about 30 s.',m_savedReboot:'Saved – the board is restarting.',
m_armForget:'Erases the Wi-Fi credentials and restarts into the setup network',m_forgot:'Erased – restarting, then “Bambutton-Setup”.',m_portal:'Setup network “%1” is now open for 10 minutes (http://192.168.4.1/).',
m_timeout:'Timeout',m_bbConnecting:'Connecting to Bambuddy …',m_printers:'%1 printers loaded (%2 ms). Now assign the buttons in step 3 and save.',
m_saving:'Saving …',m_saved:'Saved.',m_saveErr:'Error while saving.',m_sendClear:'Sending clear-plate for button %1 to Bambuddy …',
m_clearOk:'Success – HTTP %1 after %2 ms. Bambuddy has cleared the plate.',m_clearNa:'Connection OK – %1. Not an error: the button only acts when a print is finished and Bambuddy waits for the acknowledgement.',
m_clearFail:'Failed: %1',m_reboot:'Restarting …',m_pickFw:'Please choose a firmware.bin first.',m_uploading:'Uploading %1 kB – do not disconnect …',
m_otaOk:'Update installed – the board is restarting.',m_otaFail:'Update failed.',m_otaLost:'Upload aborted (connection lost). The board keeps running the old firmware.',
lead_sta:'On Wi-Fi “%1” · %2 · http://%3.local',lead_down:'Wi-Fi “%1”: %2',lead_downSince:' – no connection for %1',lead_setup:'First setup – connect the board to your Wi-Fi.',
r_conn:'Connection',r_connWith:'connected to “%1”',r_down:'“%1” – %2',r_noWifi:'no Wi-Fi configured',r_ip:'IP address',r_ch:' · channel %1',r_sig:'Signal',r_name:'Name',r_since:'Connected for',r_reconn:' · reconnected %1×',
r_ap:'Setup network',r_apOpen:'“%1” open (%2 %3, %4)',dev1:'device',devN:'devices',noPw:'no password',r_apClosed:'closed',
w_last:'Last connection attempt: %1',w_retry:'The board keeps trying to reach “%1” (%2 attempts so far). The setup network stays open while there is no connection and closes by itself once there is.',w_lastErr:' Last error: %1',
st_printer:'Printer %1',st_noLink:'no link',st_online:'online',st_offline:'printer offline',st_await:'clear plate',st_ready:'ready',st_light:'light %1',on:'on',off:'off',st_none:'no printer assigned',r_btn:'Button %1',
bb_off:'switched off',bb_unconf:'not configured',bb_none:'no answer yet',bb_ok:'reachable',bb_last:'last answer %1 ago',r_polls:'Polls',r_pollsV:'%1 (%2 errors)',
r_lastErr:'Last error',r_lastClear:'Last clear-plate',r_board:'Board',r_boardV:'up for %1 · %2 kB free'
}};
var L='de';try{L=localStorage.getItem('lang')||'de'}catch(e){}if(!I[L])L='de';
function T(k){var a=arguments,s=I[L][k]!==undefined?I[L][k]:(I.de[k]||k);return s.replace(/%(\d)/g,function(m,n){return a[+n]!==undefined?a[+n]:m})}
function applyLang(l){L=l;try{localStorage.setItem('lang',l)}catch(e){}document.documentElement.lang=l;$('langBtn').textContent=l==='de'?'EN':'DE';
  var q=document.querySelectorAll('[data-t]');for(var i=0;i<q.length;i++){var k=q[i].getAttribute('data-t');if(I[l][k]!==undefined)q[i].textContent=I[l][k]}
  q=document.querySelectorAll('[data-p]');for(i=0;i<q.length;i++){k=q[i].getAttribute('data-p');if(I[l][k]!==undefined)q[i].placeholder=I[l][k]}
  $('wifiSum').textContent=T(S&&S.net.hasWifi?'wifiChange':'wifiSetup');
  if(lastScan)fillScan(lastScan);
  if(S)render(S)}
var langPending=false;
function toggleLang(){var l=L==='de'?'en':'de';applyLang(l);langPending=true;P('/api/save',{lang:l==='en'?1:0}).then(function(){langPending=false;refresh()}).catch(function(){langPending=false})}
function $(i){return document.getElementById(i)}
function msg(id,cls,t){var m=$(id);m.className='msg on '+cls;m.textContent=t;if(!t)m.className='msg'}
function esc(s){return String(s==null?'':s).replace(/[<>&"]/g,function(c){return{'<':'&lt;','>':'&gt;','&':'&amp;','"':'&quot;'}[c]})}
function J(u,o){return fetch(u,o).then(function(r){return r.json()})}
function P(u,b){return J(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b||{})})}
function row(k,v){return '<tr><td>'+k+'</td><td>'+v+'</td></tr>'}
function fmtS(s){s=Math.round(s);if(s<90)return s+' s';var m=Math.round(s/60);if(m<90)return m+' min';var h=Math.floor(m/60);return h+' h '+(m-h*60)+' min'}
function q(r){return r>=-55?(L==='de'?'sehr gut':'excellent'):r>=-67?(L==='de'?'gut':'good'):r>=-75?(L==='de'?'mittel':'fair'):r>=-85?(L==='de'?'schwach':'weak'):(L==='de'?'sehr schwach':'very weak')}
function prog(t){var p=$('prog');p.textContent=t;p.className='prog'+(t?' on':'')}
var dirty={},S=null,init=false,fails=0,armed={},doneShown=0,lastScan=null;
['host','key','apiOn','p0','p1','pollMs','idleLed','txPower','apPass','apTimeout','hostname','ssid','ssidm','pw'].forEach(function(i){
  ['input','change'].forEach(function(ev){$(i).addEventListener(ev,function(){dirty[i]=1})})});
function setIf(id,v){if(dirty[id]||document.activeElement===$(id))return;var e=$(id);if(e.type==='checkbox')e.checked=!!v;else e.value=v}
function arm(key,text,fn){if(armed[key]&&Date.now()-armed[key]<8000){armed[key]=0;fn();return}armed[key]=Date.now();msg('m1','warn',T('m_arm',text))}
$('ssid').addEventListener('change',function(){$('ssidm').classList.toggle('hide',this.value!=='__m')});
function ssidVal(){var v=$('ssid').value;return v==='__m'?$('ssidm').value.trim():v}
function togglePw(){var i=$('pw');i.type=i.type==='password'?'text':'password'}
function dot(c){return '<span class="dot '+c+'"></span>'}

function scan(fresh){
  msg('m1','',T('m_scan'));
  J('/api/scan'+(fresh?'?fresh=1':'')).then(function(d){
    if(d.scanning){setTimeout(function(){scan(0)},1200);return}
    var list=d.networks||[];fillScan(list);
    if(d.testing&&!list.length)msg('m1','warn',T('m_scanTesting'));
    else if(d.failed&&!list.length)msg('m1','err',T('m_scanFail'));
    else msg('m1','ok',T('m_scanOk',list.length));
  }).catch(function(){msg('m1','err',T('m_noBoard'))});
}
function fillScan(list){
  lastScan=list;
  var s=$('ssid'),cur=ssidVal()||(S&&S.net.ssid)||'';
  s.innerHTML='<option value="" data-t="optChoose">'+esc(T('optChoose'))+'</option>';
  list.forEach(function(n){var o=document.createElement('option');o.value=n.ssid;o.textContent=n.ssid+'  ('+q(n.rssi)+(n.secure?'':(L==='de'?', offen':', open'))+')';s.appendChild(o)});
  var m=document.createElement('option');m.value='__m';m.setAttribute('data-t','optManual');m.textContent=T('optManual');s.appendChild(m);
  if(cur){var f=false;for(var i=0;i<s.options.length;i++)if(s.options[i].value===cur)f=true;
    if(f)s.value=cur;else{s.value='__m';$('ssidm').value=cur;$('ssidm').classList.remove('hide')}}
}
var tf=0,mySeq=0,chainId=0;
function connectWifi(){
  var ssid=ssidVal();if(!ssid){msg('m1','err',T('m_pickSsid'));return}
  $('done').classList.add('hide');msg('m1','','');
  mySeq=Date.now()%2000000000;
  var chain=++chainId;  // a second click starts a new chain; the old one goes quiet
  prog(T('m_connecting',ssid));
  P('/api/wifi/connect',{ssid:ssid,pass:$('pw').value,hostname:$('hostname').value.trim(),seq:mySeq})
   .then(function(d){if(chain!==chainId)return;if(!d.ok){prog('');msg('m1','err',d.error||'?');return}tf=0;setTimeout(function(){pollTest(chain)},1500)})
   .catch(function(){if(chain!==chainId)return;tf=0;setTimeout(function(){pollTest(chain)},2000)});
}
function pollTest(chain){
  J('/api/wifi/state').then(function(d){
    if(chain!==chainId)return;
    tf=0;
    if(d.testing&&d.testSeq===mySeq){prog(d.phaseText+' …');setTimeout(function(){pollTest(chain)},1500);return}
    if(d.testing){setTimeout(function(){pollTest(chain)},1500);return}
    prog('');
    if(d.testSeq!==mySeq){msg('m1','err',T('m_notArrived'));return}
    if(d.testDone&&d.testOk&&d.sta)showDone(d);
    else msg('m1','err',T('m_fail',d.testText||d.reason||d.phaseText));
  }).catch(function(){
    if(chain!==chainId)return;
    tf++;
    if(tf<=20){prog(T('m_wait'));setTimeout(function(){pollTest(chain)},2500)}
    else{prog('');msg('m1','err',T('m_lost',$('hostname').value.trim()||'bambutton'))}
  });
}
function showDone(d){
  doneShown=d.testSeq||1;
  $('done').classList.remove('hide');
  $('doneMsg').innerHTML=T('done_html',esc(d.ssid),esc(d.ip),esc(d.hostname),esc(d.rssiText),d.rssi);
  $('wifiForm').open=false;dirty.ssid=dirty.ssidm=dirty.pw=0;
}
function finish(){
  msg('m1','',T('m_closing'));
  P('/api/wifi/finish').then(function(d){if(d.ok)msg('m1','ok',T('m_closed'));else msg('m1','err',d.error)})
   .catch(function(){msg('m1','ok',T('m_closed'))});
}
function saveBlind(){
  var ssid=ssidVal();if(!ssid){msg('m1','err',T('m_pickSsid'));return false}
  arm('blind',T('m_armBlind'),function(){
    P('/api/wifi/save',{ssid:ssid,pass:$('pw').value,hostname:$('hostname').value.trim()})
     .then(function(d){d.ok?msg('m1','ok',T('m_savedBlind')):msg('m1','err',d.error)})
     .catch(function(){msg('m1','ok',T('m_savedReboot'))})});
  return false;
}
function forget(){
  arm('forget',T('m_armForget'),function(){
    P('/api/wifi/forget').then(function(){msg('m1','ok',T('m_forgot'))}).catch(function(){msg('m1','ok',T('m_forgot'))})});
  return false;
}
function portal(){P('/api/wifi/portal').then(function(d){msg('m5','ok',T('m_portal',d.apSsid||'Bambutton-Setup'))}).catch(function(){msg('m5','err',T('m_noBoard'))})}

function jobPoll(id,cb,n){
  J('/api/job?id='+id).then(function(d){if(d.pending){n>90?cb({ok:false,error:T('m_timeout')}):setTimeout(function(){jobPoll(id,cb,n+1)},700)}else cb(d)})
   .catch(function(){n>90?cb({ok:false,error:T('m_noBoard')}):setTimeout(function(){jobPoll(id,cb,n+1)},1000)});
}
function loadPrinters(){
  msg('m2','',T('m_bbConnecting'));
  P('/api/printers',{host:$('host').value.trim(),key:$('key').value}).then(function(d){
    if(!d.ok){msg('m2','err',d.error||'?');return}
    jobPoll(d.job,function(r){
      if(!r.ok){msg('m2','err',(r.error||'?')+(r.status>0?' (HTTP '+r.status+', '+r.ms+' ms)':''));return}
      var list=r.printers||[];
      [0,1].forEach(function(i){var s=$('p'+i),cur=s.value||s.getAttribute('data-cur')||'0';
        s.innerHTML='<option value="0" data-t="optNoPrinter">'+esc(T('optNoPrinter'))+'</option>';
        list.forEach(function(p){var o=document.createElement('option');o.value=p.id;o.textContent=p.name+' (ID '+p.id+')';s.appendChild(o)});
        s.value=cur;if(s.value!==cur)s.value='0'});
      msg('m2','ok',T('m_printers',list.length,r.ms));
    },0);
  }).catch(function(){msg('m2','err',T('m_noBoard'))});
}
function saveAll(m){
  msg(m,'',T('m_saving'));
  var b={key:$('key').value,apiEnabled:$('apiOn').checked,
         p0:parseInt($('p0').value||'0'),p1:parseInt($('p1').value||'0'),pollMs:parseInt($('pollMs').value),
         idleLed:parseInt($('idleLed').value),txPower:parseInt($('txPower').value),apTimeout:parseInt($('apTimeout').value)};
  if(dirty.host)b.host=$('host').value.trim();
  if(dirty.apPass)b.apPass=$('apPass').value;
  P('/api/save',b).then(function(d){if(d.ok){msg(m,'ok',T('m_saved'));['host','key','apiOn','p0','p1','pollMs','idleLed','txPower','apPass','apTimeout'].forEach(function(k){delete dirty[k]});refresh()}else msg(m,'err',d.error||'?')})
   .catch(function(){msg(m,'err',T('m_saveErr'))});
}
function ident(i){fetch('/api/identify?i='+i,{method:'POST'}).catch(function(){})}
function testClear(i){
  msg('m4','',T('m_sendClear',i?'B':'A'));
  P('/api/testclear?i='+i).then(function(d){
    if(!d.ok){msg('m4','err',d.error||'?');return}
    jobPoll(d.job,function(r){
      if(r.ok)msg('m4','ok',T('m_clearOk',r.status,r.ms)+(r.body?'\n'+r.body:''));
      else if(r.notAwaiting)msg('m4','ok',T('m_clearNa',r.error));
      else msg('m4','err',T('m_clearFail',(r.error||'?')+(r.status>0?' (HTTP '+r.status+', '+r.ms+' ms)':'')));
      refresh();
    },0);
  }).catch(function(){msg('m4','err',T('m_noBoard'))});
}
function reboot(){fetch('/api/reboot',{method:'POST'}).catch(function(){});msg('m6','ok',T('m_reboot'))}
function ota(){
  var f=$('fw').files[0];
  if(!f){msg('m6','err',T('m_pickFw'));return}
  msg('m6','',T('m_uploading',Math.round(f.size/1024)));
  var fd=new FormData();fd.append('firmware',f,f.name);
  fetch('/api/ota',{method:'POST',body:fd}).then(function(r){return r.json()})
   .then(function(d){d.ok?msg('m6','ok',T('m_otaOk')):msg('m6','err',d.error||T('m_otaFail'))})
   .catch(function(){msg('m6','err',T('m_otaLost'))});
}

function render(d){
  $('ver').textContent='v'+d.version;
  var n=d.net,h='';
  if(n.sta)$('lead').textContent=T('lead_sta',n.ssid,n.ip,n.hostname);
  else if(n.hasWifi)$('lead').textContent=T('lead_down',n.ssid,n.phaseText)+(n.downSec?T('lead_downSince',fmtS(n.downSec)):'');
  else $('lead').textContent=T('lead_setup');
  h+=row(T('r_conn'),n.sta?dot('g')+T('r_connWith',esc(n.ssid)):(n.testing?dot('y')+esc(n.phaseText)+' …':dot('r')+(n.hasWifi?T('r_down',esc(n.ssid),esc(n.phaseText)):T('r_noWifi'))));
  if(n.sta){h+=row(T('r_ip'),esc(n.ip)+T('r_ch',n.channel));h+=row(T('r_sig'),esc(n.rssiText)+' ('+n.rssi+' dBm)');h+=row(T('r_name'),'http://'+esc(n.hostname)+'.local');h+=row(T('r_since'),fmtS(n.upSec)+(n.reconnects?T('r_reconn',n.reconnects):''))}
  h+=row(T('r_ap'),n.ap?dot('y')+T('r_apOpen',esc(n.apSsid),n.apClients,T(n.apClients==1?'dev1':'devN'),n.apOpen?T('noPw'):'WPA2'):(n.apTimedOut?T('r_apTimedOut'):T('r_apClosed')));
  $('wifiTab').innerHTML=h;
  var w='';
  if(n.reason)w=T('w_last',n.reason);
  else if(!n.sta&&n.hasWifi&&!n.testing)w=T('w_retry',n.ssid,n.attempts)+(n.lastReason?T('w_lastErr',n.lastReason):'');
  $('wifiWarn').className='msg warn'+(w?' on':'');$('wifiWarn').textContent=w;
  $('wifiSum').textContent=T(n.hasWifi?'wifiChange':'wifiSetup');
  if(!init){init=true;if(!n.sta){$('wifiForm').open=true;if(!n.hasWifi)scan(0)}}
  // Phone came back after the setup sheet closed during the channel hop: show the result.
  if(n.ap&&n.sta&&!n.testing&&n.testDone&&n.testOk&&n.testSeq&&n.testSeq!==doneShown&&location.hostname===n.apIp&&$('done').classList.contains('hide'))showDone(n);
  setIf('host',d.host||'');setIf('apiOn',d.apiEnabled!==false);setIf('pollMs',String(d.pollMs));
  setIf('idleLed',String(d.idleLed));setIf('txPower',String(n.txPower));setIf('apTimeout',String(n.apTimeoutMin));setIf('hostname',n.hostname||'');
  if(!dirty.key)$('key').placeholder=T(d.hasKey?'pKeySet':'pKey');
  if(!dirty.apPass)$('apPass').placeholder=T(d.apPassSet?'pApPassSet':'pApPass');
  if(!dirty.ssid&&n.ssid&&$('ssid').value===''&&$('ssid').options.length<=2){var o=document.createElement('option');o.value=n.ssid;o.textContent=n.ssid;$('ssid').insertBefore(o,$('ssid').children[1]);$('ssid').value=n.ssid}
  h='';
  (d.stations||[]).forEach(function(s,i){
    var sel=$('p'+i);sel.setAttribute('data-cur',String(s.printerId));
    if(!dirty['p'+i]){var have=false;for(var k=0;k<sel.options.length;k++)if(sel.options[k].value==String(s.printerId))have=true;
      if(!have&&s.printerId>0){var o2=document.createElement('option');o2.value=s.printerId;o2.textContent=T('st_printer',s.printerId);sel.appendChild(o2)}
      sel.value=String(s.printerId)}
    var t;
    if(s.printerId>0){t=T('st_printer',s.printerId)+' · '+(s.noLink?dot('r')+T('st_noLink'):(s.online?dot('g')+T('st_online'):dot('y')+T('st_offline')))+(s.state?' · '+esc(s.state):'')+' · '+(s.awaiting?'<b>'+T('st_await')+'</b>':T('st_ready'))+' · '+T('st_light',T(s.light?'on':'off'));
      if(s.error)t+='<br><span style="color:var(--err)">'+esc(s.error)+'</span>'}
    else t=T('st_none');
    h+=row(T('r_btn',i?'B':'A'),t);
  });
  var bb=d.apiEnabled===false?'<b>'+T('bb_off')+'</b>':(!d.host?T('bb_unconf'):(d.lastOkAgo<0?T('bb_none'):(d.lastOkAgo<30?dot('g')+T('bb_ok'):dot('r')+T('bb_last',fmtS(d.lastOkAgo)))));
  h+=row('Bambuddy',bb+(d.host?' · '+esc(d.host):''));
  h+=row(T('r_polls'),T('r_pollsV',d.polls,d.errors));
  if(d.lastError)h+=row(T('r_lastErr'),'<span style="color:var(--err)">'+esc(d.lastError)+'</span>');
  if(d.lastClear)h+=row(T('r_lastClear'),esc(d.lastClear));
  h+=row(T('r_board'),T('r_boardV',fmtS(d.uptime),Math.round(d.heap/1024)));
  $('diag').innerHTML=h;
}
function refresh(){
  J('/api/status').then(function(d){
    fails=0;$('offline').className='msg err';S=d;
    if(d.lang!==undefined&&!langPending){var bl=d.lang===1?'en':'de';if(bl!==L){applyLang(bl);return}}
    render(d);
  }).catch(function(){fails++;if(fails>=3)$('offline').className='msg err on'});
}
applyLang(L);refresh();setInterval(refresh,4000);
</script></body></html>)HTML";
