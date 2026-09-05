#pragma once
#include <Arduino.h>

static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html><html lang="de"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Bambutton</title><style>
:root{--bg:#f1f5f9;--card:#fff;--ink:#0f172a;--mut:#64748b;--line:#e2e8f0;--acc:#2563eb;--ok:#059669;--err:#dc2626;--warn:#b45309}
@media(prefers-color-scheme:dark){:root{--bg:#0f172a;--card:#1e293b;--ink:#f1f5f9;--mut:#94a3b8;--line:#334155;--warn:#f59e0b}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.5 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
.wrap{max-width:560px;margin:0 auto;padding:16px}
h1{font-size:22px;margin:0 0 2px}.sub{color:var(--mut);font-size:13px;margin:0 0 14px}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px;margin-bottom:12px}
.step{font-size:12px;text-transform:uppercase;letter-spacing:.05em;color:var(--mut);margin:0 0 10px;font-weight:600}
label{display:block;font-size:13px;color:var(--mut);margin:10px 0 4px}
input,select{width:100%;padding:10px;border:1px solid var(--line);border-radius:8px;background:var(--card);color:var(--ink);font-size:15px}
button{width:100%;padding:11px;border:0;border-radius:8px;background:var(--acc);color:#fff;font-size:15px;font-weight:600;margin-top:12px;cursor:pointer}
button.sec{background:transparent;color:var(--acc);border:1px solid var(--acc)}
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
<h1>Bambutton <span class="badge" id="ver"></span></h1>
<p class="sub" id="lead">Wird geladen …</p>
<div id="offline" class="msg err">Keine Antwort vom Board. Ist dein Gerät noch mit dem richtigen WLAN verbunden?</div>

<div class="card">
  <p class="step">1 · WLAN</p>
  <table id="wifiTab"></table>
  <div id="wifiWarn" class="msg warn"></div>
  <details id="wifiForm">
    <summary id="wifiSum">WLAN einrichten</summary>
    <label>Netzwerk</label>
    <div class="row">
      <select id="ssid"><option value="">— wählen —</option><option value="__m">anderes Netz (manuell) …</option></select>
      <button class="sec icon" onclick="scan(1)" title="Netzwerke neu suchen">&#8635;</button>
    </div>
    <input id="ssidm" class="hide" placeholder="WLAN-Name (SSID)" style="margin-top:8px">
    <label>Passwort</label>
    <div class="row">
      <input id="pw" type="password" placeholder="WLAN-Passwort (leer = offenes Netz)">
      <button class="sec icon" onclick="togglePw()" title="anzeigen">&#128065;</button>
    </div>
    <label>Gerätename im Netzwerk</label>
    <input id="hostname" placeholder="bambutton">
    <button onclick="connectWifi()">Verbinden &amp; speichern</button>
    <div id="prog" class="prog"></div>
    <div id="m1" class="msg"></div>
    <div class="links"><a href="#" onclick="return saveBlind()">Ohne Test speichern und neu starten</a> · <a href="#" onclick="return forget()">WLAN-Daten löschen</a></div>
    <small>Nur 2,4-GHz-Netze. Beim Verbinden bleibt dein Handy im Setup-Netz und sieht das Ergebnis; die Verbindung kann dabei ein paar Sekunden aussetzen – einfach warten.</small>
  </details>
  <div id="done" class="hide">
    <div class="msg ok on" id="doneMsg"></div>
    <button class="sec" onclick="finish()">Setup-Netz jetzt schließen</button>
  </div>
</div>

<div class="card">
  <p class="step">2 · Bambuddy</p>
  <label>Adresse (IP:Port)</label>
  <input id="host" placeholder="z. B. 192.168.1.50:8000" inputmode="url">
  <label>API-Key</label>
  <input id="key" type="password" placeholder="API-Key (Rechte: printers:read, printers:clear_plate)">
  <button onclick="loadPrinters()">Verbindung testen &amp; Drucker laden</button>
  <label class="chk"><input type="checkbox" id="apiOn"> Bambuddy-Abfrage aktiv</label>
  <label>Abfrageintervall</label>
  <select id="pollMs"><option value="2000">2 s</option><option value="3000">3 s</option><option value="5000">5 s</option><option value="10000">10 s</option><option value="30000">30 s</option></select>
  <div id="m2" class="msg"></div>
  <small>Zum Eingrenzen von Störungen abschaltbar: Das Board bleibt im WLAN erreichbar, fragt aber keine Druckerdaten ab.</small>
</div>

<div class="card">
  <p class="step">3 · Knöpfe zuordnen</p>
  <label>Knopf A</label>
  <div class="row"><select id="p0"><option value="0">— kein Drucker —</option></select><button class="sec icon" onclick="ident(0)" title="LED blinken lassen">&#128294;</button></div>
  <label>Knopf B</label>
  <div class="row"><select id="p1"><option value="0">— kein Drucker —</option></select><button class="sec icon" onclick="ident(1)" title="LED blinken lassen">&#128294;</button></div>
  <label>LED im Ruhezustand</label>
  <select id="idleLed"><option value="0">folgt dem Kammerlicht</option><option value="1">immer an</option><option value="2">immer aus</option></select>
  <button onclick="saveAll('m3')">Speichern</button>
  <div id="m3" class="msg"></div>
  <small>Die Taschenlampe lässt den passenden Knopf kurz blinken. Speichern übernimmt auch Adresse, Key und Intervall aus Schritt 2.</small>
</div>

<div class="card">
  <p class="step">4 · Status &amp; Diagnose</p>
  <table id="diag"><tr><td>Lade …</td><td></td></tr></table>
  <button class="sec" onclick="testClear(0)">Knopf A jetzt testen (Platte freigeben)</button>
  <button class="sec" onclick="testClear(1)">Knopf B jetzt testen (Platte freigeben)</button>
  <div id="m4" class="msg"></div>
  <small>Der Test löst denselben Aufruf aus wie ein Tastendruck und zeigt Bambuddys Antwort im Klartext. „Nichts zu räumen“ heißt: Verbindung OK, aber kein Druck wartet auf die Freigabe.</small>
</div>

<div class="card">
  <details>
    <summary>5 · Erweitert &amp; Firmware</summary>
    <label>WLAN-Sendeleistung</label>
    <select id="txPower"><option value="34">8,5 dBm – empfohlen für ESP32-C3 Super Mini</option><option value="44">11 dBm</option><option value="52">13 dBm</option><option value="60">15 dBm</option><option value="68">17 dBm</option><option value="78">19,5 dBm – Maximum</option><option value="20">5 dBm</option></select>
    <small>Die Antenne des Super Mini verträgt keine volle Leistung: mit 19,5 dBm bricht die Verbindung oft ab oder kommt gar nicht zustande. Erst bei sehr großer Entfernung schrittweise erhöhen.</small>
    <label>Passwort für das Setup-Netz „Bambutton-Setup“</label>
    <input id="apPass" type="password" placeholder="leer = offenes Setup-Netz (8–63 Zeichen)">
    <button onclick="saveAll('m5')">Einstellungen speichern</button>
    <button class="sec" onclick="portal()">Setup-Netz für 10 Minuten öffnen</button>
    <div id="m5" class="msg"></div>
    <label>Neue firmware.bin</label>
    <input id="fw" type="file" accept=".bin">
    <button class="sec" onclick="ota()">Hochladen &amp; neu starten</button>
    <div id="m6" class="msg"></div>
    <button class="sec" onclick="reboot()">Nur neu starten</button>
  </details>
</div>
</div>
<script>
function $(i){return document.getElementById(i)}
function msg(id,cls,t){var m=$(id);m.className='msg on '+cls;m.textContent=t;if(!t)m.className='msg'}
function esc(s){return String(s==null?'':s).replace(/[<>&"]/g,function(c){return{'<':'&lt;','>':'&gt;','&':'&amp;','"':'&quot;'}[c]})}
function J(u,o){return fetch(u,o).then(function(r){return r.json()})}
function P(u,b){return J(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b||{})})}
function row(k,v){return '<tr><td>'+k+'</td><td>'+v+'</td></tr>'}
function fmtS(s){s=Math.round(s);if(s<90)return s+' s';var m=Math.round(s/60);if(m<90)return m+' min';var h=Math.floor(m/60);return h+' h '+(m-h*60)+' min'}
function q(r){return r>=-55?'sehr gut':r>=-67?'gut':r>=-75?'mittel':r>=-85?'schwach':'sehr schwach'}
function prog(t){var p=$('prog');p.textContent=t;p.className='prog'+(t?' on':'')}
var dirty={},S=null,init=false,fails=0,armed={};
['host','key','apiOn','p0','p1','pollMs','idleLed','txPower','apPass','hostname','ssid','ssidm','pw'].forEach(function(i){
  ['input','change'].forEach(function(ev){$(i).addEventListener(ev,function(){dirty[i]=1})})});
function setIf(id,v){if(dirty[id]||document.activeElement===$(id))return;var e=$(id);if(e.type==='checkbox')e.checked=!!v;else e.value=v}
function arm(key,text,fn){if(armed[key]&&Date.now()-armed[key]<8000){armed[key]=0;fn();return}armed[key]=Date.now();msg('m1','warn',text+' – zum Bestätigen noch einmal klicken.')}
$('ssid').addEventListener('change',function(){$('ssidm').classList.toggle('hide',this.value!=='__m')});
function ssidVal(){var v=$('ssid').value;return v==='__m'?$('ssidm').value.trim():v}
function togglePw(){var i=$('pw');i.type=i.type==='password'?'text':'password'}
function dot(c){return '<span class="dot '+c+'"></span>'}

function scan(fresh){
  msg('m1','','Suche Netzwerke …');
  J('/api/scan'+(fresh?'?fresh=1':'')).then(function(d){
    if(d.scanning){setTimeout(function(){scan(0)},1200);return}
    var list=d.networks||[];fillScan(list);
    if(d.failed&&!list.length)msg('m1','err','Suche fehlgeschlagen – bitte noch einmal auf ↻ tippen.');
    else msg('m1','ok',list.length+' Netzwerke gefunden (nur 2,4 GHz sichtbar).');
  }).catch(function(){msg('m1','err','Keine Antwort vom Board.')});
}
function fillScan(list){
  var s=$('ssid'),cur=ssidVal()||(S&&S.net.ssid)||'';
  s.innerHTML='<option value="">— wählen —</option>';
  list.forEach(function(n){var o=document.createElement('option');o.value=n.ssid;o.textContent=n.ssid+'  ('+q(n.rssi)+(n.secure?'':', offen')+')';s.appendChild(o)});
  var m=document.createElement('option');m.value='__m';m.textContent='anderes Netz (manuell) …';s.appendChild(m);
  if(cur){var f=false;for(var i=0;i<s.options.length;i++)if(s.options[i].value===cur)f=true;
    if(f)s.value=cur;else{s.value='__m';$('ssidm').value=cur;$('ssidm').classList.remove('hide')}}
}
var tf=0;
function connectWifi(){
  var ssid=ssidVal();if(!ssid){msg('m1','err','Bitte ein WLAN auswählen oder eintragen.');return}
  $('done').classList.add('hide');msg('m1','','');
  prog('Verbinde mit „'+ssid+'“ …');
  P('/api/wifi/connect',{ssid:ssid,pass:$('pw').value,hostname:$('hostname').value.trim()})
   .then(function(d){if(!d.ok){prog('');msg('m1','err',d.error||'Fehler');return}tf=0;setTimeout(pollTest,1500)})
   .catch(function(){tf=0;setTimeout(pollTest,2000)});
}
function pollTest(){
  J('/api/wifi/state').then(function(d){
    tf=0;
    if(d.testing){prog(d.phaseText+' …');setTimeout(pollTest,1500);return}
    prog('');
    if(d.sta&&d.phase==='connected')showDone(d);
    else msg('m1','err','Verbindung fehlgeschlagen: '+(d.reason||d.phaseText));
  }).catch(function(){
    tf++;
    if(tf<=20){prog('Warte auf das Board … (die Verbindung zum Setup-Netz kann kurz aussetzen, bitte warten)');setTimeout(pollTest,2500)}
    else{prog('');msg('m1','err','Keine Antwort mehr vom Board. Prüfe, ob dein Handy noch mit „Bambutton-Setup“ verbunden ist. Ist das Setup-Netz verschwunden, hat die Verbindung geklappt – das Board ist dann im Heimnetz unter http://'+($('hostname').value.trim()||'bambutton')+'.local erreichbar.')}
  });
}
function showDone(d){
  $('done').classList.remove('hide');
  $('doneMsg').innerHTML='<b>Verbunden mit „'+esc(d.ssid)+'“.</b><br>IP-Adresse: <b>'+esc(d.ip)+'</b><br>Name: <b>http://'+esc(d.hostname)+'.local/</b><br>Empfang: '+esc(d.rssiText)+' ('+d.rssi+' dBm)<br><br>Weiter geht es im Heimnetz: Handy oder PC mit „'+esc(d.ssid)+'“ verbinden und eine der beiden Adressen öffnen, dann Bambuddy eintragen. Das Setup-Netz schließt sich in etwa 90 Sekunden von selbst.';
  $('wifiForm').open=false;dirty.ssid=dirty.ssidm=dirty.pw=0;
}
function finish(){P('/api/wifi/finish').then(function(d){d.ok?$('doneMsg').innerHTML+='<br><br>Setup-Netz geschlossen.':msg('m1','err',d.error)}).catch(function(){})}
function saveBlind(){
  var ssid=ssidVal();if(!ssid){msg('m1','err','Bitte ein WLAN auswählen.');return false}
  arm('blind','Speichert ohne Verbindungstest und startet neu',function(){
    P('/api/wifi/save',{ssid:ssid,pass:$('pw').value,hostname:$('hostname').value.trim()})
     .then(function(d){d.ok?msg('m1','ok','Gespeichert – das Board startet neu und verbindet sich. Klappt das nicht, erscheint „Bambutton-Setup“ nach etwa 30 s wieder.'):msg('m1','err',d.error)})
     .catch(function(){msg('m1','ok','Gespeichert – das Board startet neu.')})});
  return false;
}
function forget(){
  arm('forget','Löscht die WLAN-Daten und startet ins Setup-Netz',function(){
    P('/api/wifi/forget').then(function(){msg('m1','ok','Gelöscht – Neustart, danach „Bambutton-Setup“.')}).catch(function(){msg('m1','ok','Gelöscht – Neustart.')})});
  return false;
}
function portal(){P('/api/wifi/portal').then(function(d){msg('m5','ok','Setup-Netz „'+(d.apSsid||'Bambutton-Setup')+'“ ist jetzt 10 Minuten offen (http://192.168.4.1/).')}).catch(function(){msg('m5','err','Keine Antwort vom Board.')})}

function jobPoll(id,cb,n){
  J('/api/job?id='+id).then(function(d){if(d.pending){n>60?cb({ok:false,error:'Zeitüberschreitung'}):setTimeout(function(){jobPoll(id,cb,n+1)},700)}else cb(d)})
   .catch(function(){n>60?cb({ok:false,error:'Keine Antwort vom Board'}):setTimeout(function(){jobPoll(id,cb,n+1)},1000)});
}
function loadPrinters(){
  msg('m2','','Verbinde mit Bambuddy …');
  P('/api/printers',{host:$('host').value.trim(),key:$('key').value}).then(function(d){
    if(!d.ok){msg('m2','err',d.error||'Fehler');return}
    jobPoll(d.job,function(r){
      if(!r.ok){msg('m2','err',(r.error||'Fehler')+(r.status>0?' (HTTP '+r.status+', '+r.ms+' ms)':''));return}
      var list=r.printers||[];
      [0,1].forEach(function(i){var s=$('p'+i),cur=s.value||s.getAttribute('data-cur')||'0';
        s.innerHTML='<option value="0">— kein Drucker —</option>';
        list.forEach(function(p){var o=document.createElement('option');o.value=p.id;o.textContent=p.name+' (ID '+p.id+')';s.appendChild(o)});
        s.value=cur;if(s.value!==cur)s.value='0'});
      msg('m2','ok',list.length+' Drucker geladen ('+r.ms+' ms). Jetzt in Schritt 3 die Knöpfe zuordnen und speichern.');
    },0);
  }).catch(function(){msg('m2','err','Keine Antwort vom Board.')});
}
function saveAll(m){
  msg(m,'','Speichere …');
  var b={host:$('host').value.trim(),key:$('key').value,apiEnabled:$('apiOn').checked,
         p0:parseInt($('p0').value||'0'),p1:parseInt($('p1').value||'0'),pollMs:parseInt($('pollMs').value),
         idleLed:parseInt($('idleLed').value),txPower:parseInt($('txPower').value)};
  if(dirty.apPass)b.apPass=$('apPass').value;
  P('/api/save',b).then(function(d){if(d.ok){msg(m,'ok','Gespeichert.');dirty={};refresh()}else msg(m,'err',d.error||'Fehler')})
   .catch(function(){msg(m,'err','Fehler beim Speichern.')});
}
function ident(i){fetch('/api/identify?i='+i,{method:'POST'}).catch(function(){})}
function testClear(i){
  msg('m4','','Sende clear-plate für Knopf '+(i?'B':'A')+' an Bambuddy …');
  P('/api/testclear?i='+i).then(function(d){
    if(!d.ok){msg('m4','err',d.error||'Fehler');return}
    jobPoll(d.job,function(r){
      if(r.ok)msg('m4','ok','Erfolg – HTTP '+r.status+' nach '+r.ms+' ms. Bambuddy hat die Platte freigegeben.');
      else if(r.notAwaiting)msg('m4','ok','Verbindung OK – '+r.error+'. Kein Fehler: der Knopf wirkt nur, wenn ein Druck fertig ist und Bambuddy auf die Freigabe wartet.');
      else msg('m4','err','Fehlgeschlagen: '+(r.error||'unbekannt')+(r.status>0?' (HTTP '+r.status+', '+r.ms+' ms)':''));
      refresh();
    },0);
  }).catch(function(){msg('m4','err','Keine Antwort vom Board.')});
}
function reboot(){fetch('/api/reboot',{method:'POST'}).catch(function(){});msg('m6','ok','Neustart …')}
function ota(){
  var f=$('fw').files[0];
  if(!f){msg('m6','err','Bitte zuerst eine firmware.bin auswählen.');return}
  msg('m6','','Lade '+Math.round(f.size/1024)+' kB hoch – nicht trennen …');
  var fd=new FormData();fd.append('firmware',f,f.name);
  fetch('/api/ota',{method:'POST',body:fd}).then(function(r){return r.json()})
   .then(function(d){d.ok?msg('m6','ok','Update eingespielt – das Board startet neu.'):msg('m6','err',d.error||'Update fehlgeschlagen.')})
   .catch(function(){msg('m6','err','Upload abgebrochen (Verbindung verloren). Das Board läuft mit der alten Firmware weiter.')});
}

function refresh(){
  J('/api/status').then(function(d){
    fails=0;$('offline').className='msg err';S=d;
    $('ver').textContent='v'+d.version;
    var n=d.net,h='';
    if(n.sta)$('lead').textContent='Im WLAN „'+n.ssid+'“ · '+n.ip+' · http://'+n.hostname+'.local';
    else if(n.hasWifi)$('lead').textContent='WLAN „'+n.ssid+'“: '+n.phaseText+(n.downSec?' – seit '+fmtS(n.downSec)+' ohne Verbindung':'');
    else $('lead').textContent='Ersteinrichtung – verbinde das Board mit deinem WLAN.';
    h+=row('Verbindung',n.sta?dot('g')+'verbunden mit „'+esc(n.ssid)+'“':(n.testing?dot('y')+esc(n.phaseText)+' …':dot('r')+(n.hasWifi?'„'+esc(n.ssid)+'“ – '+esc(n.phaseText):'kein WLAN eingerichtet')));
    if(n.sta){h+=row('IP-Adresse',esc(n.ip)+' · Kanal '+n.channel);h+=row('Empfang',esc(n.rssiText)+' ('+n.rssi+' dBm)');h+=row('Name','http://'+esc(n.hostname)+'.local');h+=row('Verbunden seit',fmtS(n.upSec)+(n.reconnects?' · '+n.reconnects+'× neu verbunden':''))}
    h+=row('Setup-Netz',n.ap?dot('y')+'„'+esc(n.apSsid)+'“ offen ('+n.apClients+' Gerät'+(n.apClients==1?'':'e')+', '+(n.apOpen?'ohne Passwort':'WPA2')+')':'geschlossen');
    $('wifiTab').innerHTML=h;
    var w='';
    if(n.reason)w='Letzter Verbindungsversuch: '+n.reason;
    else if(!n.sta&&n.hasWifi&&!n.testing)w='Das Board versucht weiter, „'+n.ssid+'“ zu erreichen (bisher '+n.attempts+' Versuche). Solange keine Verbindung steht, bleibt das Setup-Netz offen; sobald sie steht, schließt es sich von selbst.';
    $('wifiWarn').className='msg warn'+(w?' on':'');$('wifiWarn').textContent=w;
    $('wifiSum').textContent=n.hasWifi?'WLAN ändern':'WLAN einrichten';
    if(!init){init=true;if(!n.sta){$('wifiForm').open=true;if(!n.hasWifi)scan(0)}}
    setIf('host',d.host||'');setIf('apiOn',d.apiEnabled!==false);setIf('pollMs',String(d.pollMs));
    setIf('idleLed',String(d.idleLed));setIf('txPower',String(n.txPower));setIf('hostname',n.hostname||'');
    if(!dirty.key)$('key').placeholder=d.hasKey?'API-Key gespeichert – nur zum Ändern eintragen':'API-Key (Rechte: printers:read, printers:clear_plate)';
    if(!dirty.apPass)$('apPass').placeholder=d.apPassSet?'Passwort gesetzt – zum Ändern eintragen, leeren = offen':'leer = offenes Setup-Netz (8–63 Zeichen)';
    if(!dirty.ssid&&n.ssid&&$('ssid').options.length<=2){var o=document.createElement('option');o.value=n.ssid;o.textContent=n.ssid;$('ssid').insertBefore(o,$('ssid').children[1]);$('ssid').value=n.ssid}
    h='';
    (d.stations||[]).forEach(function(s,i){
      var sel=$('p'+i);sel.setAttribute('data-cur',String(s.printerId));
      if(!dirty['p'+i]){var have=false;for(var k=0;k<sel.options.length;k++)if(sel.options[k].value==String(s.printerId))have=true;
        if(!have&&s.printerId>0){var o2=document.createElement('option');o2.value=s.printerId;o2.textContent='Drucker-ID '+s.printerId;sel.appendChild(o2)}
        sel.value=String(s.printerId)}
      var t;
      if(s.printerId>0){t='Drucker '+s.printerId+' · '+(s.noLink?dot('r')+'keine Verbindung':(s.online?dot('g')+'online':dot('y')+'Drucker offline'))+(s.state?' · '+esc(s.state):'')+' · '+(s.awaiting?'<b>Platte räumen</b>':'bereit')+' · Licht '+(s.light?'an':'aus');
        if(s.error)t+='<br><span style="color:var(--err)">'+esc(s.error)+'</span>'}
      else t='kein Drucker zugeordnet';
      h+=row('Knopf '+(i?'B':'A'),t);
    });
    var bb=d.apiEnabled===false?'<b>abgeschaltet</b>':(!d.host?'nicht konfiguriert':(d.lastOkAgo<0?'noch keine Antwort':(d.lastOkAgo<30?dot('g')+'erreichbar':dot('r')+'letzte Antwort vor '+fmtS(d.lastOkAgo))));
    h+=row('Bambuddy',bb+(d.host?' · '+esc(d.host):''));
    h+=row('Abfragen',d.polls+' ('+d.errors+' Fehler)');
    if(d.lastError)h+=row('Letzter Fehler','<span style="color:var(--err)">'+esc(d.lastError)+'</span>');
    if(d.lastClear)h+=row('Letztes clear-plate',esc(d.lastClear));
    h+=row('Board','läuft seit '+fmtS(d.uptime)+' · '+Math.round(d.heap/1024)+' kB frei');
    $('diag').innerHTML=h;
  }).catch(function(){fails++;if(fails>=3)$('offline').className='msg err on'});
}
refresh();setInterval(refresh,4000);
</script></body></html>)HTML";
