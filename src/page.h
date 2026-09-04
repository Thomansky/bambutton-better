#pragma once
#include <Arduino.h>

static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html><html lang="de"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Bambutton</title><style>
:root{--bg:#f1f5f9;--card:#fff;--ink:#0f172a;--mut:#64748b;--line:#e2e8f0;--acc:#2563eb;--ok:#059669;--err:#dc2626}
@media(prefers-color-scheme:dark){:root{--bg:#0f172a;--card:#1e293b;--ink:#f1f5f9;--mut:#94a3b8;--line:#334155}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.5 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
.wrap{max-width:560px;margin:0 auto;padding:16px}
h1{font-size:22px;margin:0 0 2px}.sub{color:var(--mut);font-size:13px;margin:0 0 16px}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px;margin-bottom:12px}
.step{font-size:12px;text-transform:uppercase;letter-spacing:.05em;color:var(--mut);margin:0 0 10px;font-weight:600}
label{display:block;font-size:13px;color:var(--mut);margin:10px 0 4px}
input,select{width:100%;padding:10px;border:1px solid var(--line);border-radius:8px;background:var(--card);color:var(--ink);font-size:15px}
button{width:100%;padding:11px;border:0;border-radius:8px;background:var(--acc);color:#fff;font-size:15px;font-weight:600;margin-top:12px;cursor:pointer}
button.sec{background:transparent;color:var(--acc);border:1px solid var(--acc)}
button:disabled{opacity:.5;cursor:default}
.row{display:flex;gap:8px;align-items:center}.row>*{margin-top:0}
.row .icon{flex:0 0 auto;width:auto;padding:10px 13px}
.msg{margin-top:10px;font-size:13px;padding:9px;border-radius:8px;display:none}
.msg.on{display:block}.ok{background:rgba(5,150,105,.12);color:var(--ok)}.err{background:rgba(220,38,38,.12);color:var(--err)}
.hide{display:none}
table{width:100%;border-collapse:collapse;font-size:13px}
td{padding:5px 0;border-bottom:1px solid var(--line);vertical-align:top}
td:first-child{color:var(--mut);width:42%}
pre{white-space:pre-wrap;word-break:break-word;background:var(--bg);padding:9px;border-radius:8px;font-size:12px;margin:8px 0 0;border:1px solid var(--line)}
.badge{font-size:11px;background:var(--bg);border:1px solid var(--line);padding:2px 7px;border-radius:99px;color:var(--mut)}
small{color:var(--mut);font-size:12px}
</style></head><body><div class="wrap">
<h1>Bambutton <span class="badge" id="ver"></span></h1>
<p class="sub" id="lead">Wird geladen …</p>

<div class="card">
  <p class="step">1 · WLAN</p>
  <label>Netzwerk</label>
  <div class="row">
    <select id="ssid"><option value="">— wählen —</option><option value="__m">andere (manuell)…</option></select>
    <button class="sec icon" onclick="scan()">&#8635;</button>
  </div>
  <input id="ssidm" class="hide" placeholder="WLAN-Name" style="margin-top:8px">
  <label>Passwort</label>
  <input id="pw" type="password" placeholder="WLAN-Passwort">
  <label>Gerätename im Netzwerk</label>
  <input id="hostname" placeholder="bambutton">
  <button onclick="saveWifi()">WLAN speichern &amp; verbinden</button>
  <div id="m1" class="msg"></div>
</div>

<div class="card">
  <p class="step">2 · Bambuddy</p>
  <label>Adresse (IP:Port)</label>
  <input id="host" placeholder="z. B. 192.168.1.50:8000" inputmode="url">
  <label>API-Key</label>
  <input id="key" type="password" placeholder="API-Key">
  <button onclick="loadPrinters()">Verbindung testen &amp; Drucker laden</button>
  <div id="m2" class="msg"></div>
</div>

<div class="card" id="cardSt">
  <p class="step">3 · Knöpfe zuordnen</p>
  <label>Knopf A</label>
  <div class="row"><select id="p0"></select><button class="sec icon" onclick="ident(0)">&#128294;</button></div>
  <label>Knopf B</label>
  <div class="row"><select id="p1"></select><button class="sec icon" onclick="ident(1)">&#128294;</button></div>
  <small>Die Taschenlampe lässt den passenden Knopf kurz blinken.</small>
  <button onclick="saveAll()">Speichern</button>
  <div id="m3" class="msg"></div>
</div>

<div class="card">
  <p class="step">4 · Status &amp; Diagnose</p>
  <table id="diag"><tr><td>Lade…</td><td></td></tr></table>
  <button class="sec" onclick="testClear(0)">Knopf A jetzt testen (Platte freigeben)</button>
  <button class="sec" onclick="testClear(1)">Knopf B jetzt testen (Platte freigeben)</button>
  <div id="m4" class="msg"></div>
  <small>Der Test löst denselben Aufruf aus wie ein echter Tastendruck und zeigt die Antwort im Klartext.</small>
</div>

<div class="card">
  <p class="step">5 · Firmware-Update</p>
  <label>Neue firmware.bin</label>
  <input id="fw" type="file" accept=".bin">
  <button class="sec" onclick="ota()">Hochladen &amp; neu starten</button>
  <div id="m5" class="msg"></div>
  <button class="sec" onclick="reboot()">Nur neu starten</button>
</div>
</div>
<script>
function $(i){return document.getElementById(i)}
function msg(id,cls,t){var m=$(id);m.className='msg on '+cls;m.textContent=t}
function J(u,o){return fetch(u,o).then(function(r){return r.json()})}
$('ssid').addEventListener('change',function(){$('ssidm').classList.toggle('hide',this.value!=='__m')});
function ssidVal(){var v=$('ssid').value;return v==='__m'?$('ssidm').value.trim():v}

function scan(){
  msg('m1','','Suche Netzwerke …');
  J('/api/scan').then(function(d){
    var s=$('ssid');s.innerHTML='<option value="">— wählen —</option>';
    (d.networks||[]).forEach(function(n){
      var o=document.createElement('option');o.value=n.ssid;
      o.textContent=n.ssid+' ('+n.rssi+' dBm)';s.appendChild(o);
    });
    var o=document.createElement('option');o.value='__m';o.textContent='andere (manuell)…';s.appendChild(o);
    msg('m1','ok',(d.networks||[]).length+' Netzwerke gefunden.');
  }).catch(function(){msg('m1','err','Suche fehlgeschlagen.')});
}
function saveWifi(){
  msg('m1','','Speichere …');
  J('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({ssid:ssidVal(),pass:$('pw').value,hostname:$('hostname').value.trim()})})
   .then(function(d){ d.ok?msg('m1','ok','Gespeichert. Das Board startet neu und verbindet sich.')
                          :msg('m1','err',d.error||'Fehler') })
   .catch(function(){msg('m1','ok','Gespeichert — das Board startet neu.')});
}
function loadPrinters(){
  msg('m2','','Verbinde …');
  J('/api/printers',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({host:$('host').value.trim(),key:$('key').value})})
   .then(function(d){
     if(!d.ok){msg('m2','err',d.error||'Fehler');return}
     var list=d.printers||[];
     [0,1].forEach(function(i){
       var s=$('p'+i),cur=s.getAttribute('data-cur')||'0';
       s.innerHTML='<option value="0">— kein Drucker —</option>';
       list.forEach(function(p){var o=document.createElement('option');o.value=p.id;o.textContent=p.name+' (ID '+p.id+')';s.appendChild(o)});
       s.value=cur;
     });
     msg('m2','ok',list.length+' Drucker geladen.');
   }).catch(function(){msg('m2','err','Verbindung fehlgeschlagen.')});
}
function saveAll(){
  msg('m3','','Speichere …');
  J('/api/save',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({host:$('host').value.trim(),key:$('key').value,
                         p0:parseInt($('p0').value||'0'),p1:parseInt($('p1').value||'0')})})
   .then(function(d){d.ok?msg('m3','ok','Gespeichert.'):msg('m3','err',d.error||'Fehler')})
   .catch(function(){msg('m3','err','Fehler beim Speichern.')});
}
function ident(i){fetch('/api/identify?i='+i,{method:'POST'}).catch(function(){})}
function testClear(i){
  msg('m4','','Sende clear-plate an Bambuddy …');
  J('/api/testclear?i='+i,{method:'POST'}).then(function(d){
    if(d.ok){msg('m4','ok','Erfolg — HTTP '+d.status+' nach '+d.ms+' ms.')}
    else{msg('m4','err','Fehlgeschlagen: '+(d.error||'unbekannt')+' (HTTP '+d.status+', '+d.ms+' ms)')}
    refresh();
  }).catch(function(){msg('m4','err','Keine Antwort vom Board.')});
}
function reboot(){fetch('/api/reboot',{method:'POST'}).catch(function(){});msg('m5','ok','Neustart …')}
function ota(){
  var f=$('fw').files[0];
  if(!f){msg('m5','err','Bitte zuerst eine .bin auswählen.');return}
  msg('m5','','Lade '+Math.round(f.size/1024)+' kB hoch — nicht trennen …');
  var fd=new FormData();fd.append('firmware',f,f.name);
  fetch('/api/ota',{method:'POST',body:fd}).then(function(r){return r.json()})
   .then(function(d){d.ok?msg('m5','ok','Update eingespielt — Board startet neu.'):msg('m5','err',d.error||'Update fehlgeschlagen.')})
   .catch(function(){msg('m5','ok','Upload beendet — Board startet neu.')});
}
function row(k,v){return '<tr><td>'+k+'</td><td>'+v+'</td></tr>'}
function esc(s){return String(s==null?'':s).replace(/[<>&]/g,function(c){return{'<':'&lt;','>':'&gt;','&':'&amp;'}[c]})}
function refresh(){
  J('/api/status').then(function(d){
    $('ver').textContent='v'+d.version;
    $('lead').textContent = d.mode==='setup'
      ? 'Ersteinrichtung — verbinde das Board mit deinem WLAN.'
      : 'Verbunden als '+d.hostname+' · '+d.ip;
    if(d.host&&!$('host').value)$('host').value=d.host;
    if(d.hostname&&!$('hostname').value)$('hostname').value=d.hostname;
    if(d.ssid&&$('ssid').options.length<3){var o=document.createElement('option');o.value=d.ssid;o.textContent=d.ssid;$('ssid').insertBefore(o,$('ssid').children[1]);$('ssid').value=d.ssid}
    var h='';
    (d.stations||[]).forEach(function(s,i){
      $('p'+i).setAttribute('data-cur',String(s.printerId));
      if($('p'+i).options.length<=1&&s.printerId>0){
        var o=document.createElement('option');o.value=s.printerId;o.textContent='Drucker-ID '+s.printerId;$('p'+i).appendChild(o);$('p'+i).value=s.printerId;
      }
      h+=row('Knopf '+(i?'B':'A'),
        s.printerId>0 ? ('Drucker '+s.printerId+' · '+(s.awaiting?'<b>Platte räumen</b>':'bereit')+' · Licht '+(s.light?'an':'aus'))
                      : 'kein Drucker zugeordnet');
    });
    h+=row('Abfragen gesamt',d.polls+' ('+d.errors+' Fehler)');
    if(d.lastError)h+=row('Letzter Fehler','<span style="color:var(--err)">'+esc(d.lastError)+'</span>');
    if(d.lastClear)h+=row('Letztes clear-plate',esc(d.lastClear));
    $('diag').innerHTML=h;
  }).catch(function(){});
}
refresh();setInterval(refresh,4000);
</script></body></html>)HTML";
