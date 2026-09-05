#include "Net.h"
#include "Settings.h"
#include <ESPmDNS.h>
#include <mdns.h>
#include <esp_netif.h>
#include <algorithm>

Net net;

// ------------------------------------------------------------------ captive DNS

void CaptiveDns::begin(IPAddress ip) {
  _ip = ip;
  _up = _udp.begin(53);
}

void CaptiveDns::stop() {
  if (_up) _udp.stop();
  _up = false;
}

void CaptiveDns::poll() {
  if (!_up) return;
  int len = _udp.parsePacket();
  if (len <= 0) return;
  if (len > (int)sizeof(_buf)) {
    _udp.flush();
    return;
  }
  len = _udp.read(_buf, sizeof(_buf));
  if (len < 12 || (_buf[2] & 0x80)) return;  // too short, or a response
  uint16_t qd = (_buf[4] << 8) | _buf[5];
  if (qd < 1) return;

  // Walk the first question name to find the type.
  int p = 12;
  while (p < len && _buf[p] != 0) {
    if (_buf[p] & 0xC0) { p += 1; break; }
    p += _buf[p] + 1;
  }
  p += 1;
  if (p + 4 > len) return;
  uint16_t qtype = (_buf[p] << 8) | _buf[p + 1];
  int qend = p + 4;

  // Reply: same ID, standard response (recursion available), one question,
  // one A answer for A/ANY queries, none otherwise (so AAAA lookups end
  // quickly instead of timing out). Additional records are dropped.
  uint8_t out[512];
  if (qend + 16 > (int)sizeof(out)) return;  // absurdly long name: ignore
  int o = 0;
  bool answer = (qtype == 1 || qtype == 255);
  out[o++] = _buf[0]; out[o++] = _buf[1];
  out[o++] = 0x81; out[o++] = 0x80;
  out[o++] = 0; out[o++] = 1;
  out[o++] = 0; out[o++] = answer ? 1 : 0;
  out[o++] = 0; out[o++] = 0;
  out[o++] = 0; out[o++] = 0;
  memcpy(out + o, _buf + 12, qend - 12);
  o += qend - 12;
  if (answer) {
    out[o++] = 0xC0; out[o++] = 0x0C;          // pointer to the question name
    out[o++] = 0; out[o++] = 1;                // type A
    out[o++] = 0; out[o++] = 1;                // class IN
    out[o++] = 0; out[o++] = 0; out[o++] = 0; out[o++] = 30;  // TTL 30 s
    out[o++] = 0; out[o++] = 4;
    out[o++] = _ip[0]; out[o++] = _ip[1]; out[o++] = _ip[2]; out[o++] = _ip[3];
  }
  _udp.beginPacket(_udp.remoteIP(), _udp.remotePort());
  _udp.write(out, o);
  _udp.endPacket();
}

// ------------------------------------------------------------------ timing

static const uint32_t BOOT_FALLBACK_MS   = 30000;   // setup network appears if the boot connect fails
static const uint32_t DOWN_FALLBACK_MS   = 60000;   // ... or after a minute of lost connection later
static const uint32_t KICK_STA_MS        = 20000;   // station only: kick begin() if the core stalls
static const uint32_t KICK_AP_MS         = 30000;   // with the AP up: slower (scans disturb AP clients)
static const uint32_t STALL_MS           = 25000;   // no event for this long -> attempt is stuck
static const uint32_t TEST_TIMEOUT_MS    = 35000;
static const uint32_t TEST_DHCP_MS       = 20000;   // link up but no IP for this long -> DHCP problem
static const uint32_t TEST_STALL_MS      = 12000;   // begin() produced no event at all -> retry
static const uint32_t AP_GRACE_MS        = 90000;   // AP stays this long after connecting
static const uint32_t AP_MAX_AFTER_UP_MS = 300000;  // even with a phone still attached
static const uint32_t REBOOT_DOWN_MS     = 30UL * 60UL * 1000UL;
static const uint32_t SCAN_TIMEOUT_MS    = 15000;
static const uint32_t SCAN_MS_PER_CHAN   = 300;     // the core's own scan timeout is 20x this

// ------------------------------------------------------------------ events

void Net::onEvent(WiFiEvent_t ev, WiFiEventInfo_t info) {
  switch (ev) {
    case ARDUINO_EVENT_WIFI_STA_START:
    case ARDUINO_EVENT_WIFI_AP_START:
      // Now the radio is up; the tx-power limit only sticks from here on.
      net.applyTxPower();
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      net._evLink = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      net._evGotIp = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      net._evReason = info.wifi_sta_disconnected.reason;
      net._evDisc = true;
      break;
    default:
      break;
  }
}

// ------------------------------------------------------------------ helpers

bool Net::hasCredentials() const { return settings.hasWifi(); }

void Net::applyTxPower() {
  WiFi.setTxPower((wifi_power_t)settings.txPower);
}

void Net::applyHostname() {
  // Default for interfaces created later, the live station interface for the
  // next DHCP request, and the mDNS name (renamed in place — never torn down,
  // the worker may be inside a .local lookup right now).
  WiFi.setHostname(settings.hostname.c_str());
  esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (sta) esp_netif_set_hostname(sta, settings.hostname.c_str());
  if (_mdnsUp) mdns_hostname_set(settings.hostname.c_str());
}

String Net::currentSsid() const {
  if (_testing) return _testSsid;
  return settings.wifiSsid;
}

uint32_t Net::upSeconds() const {
  return (staConnected() && _connectedAt) ? (millis() - _connectedAt) / 1000 : 0;
}

uint32_t Net::downSeconds() const {
  return (!staConnected() && _downSince) ? (millis() - _downSince) / 1000 : 0;
}

String Net::rssiText() const {
  if (!staConnected()) return "";
  int r = WiFi.RSSI();
  if (r >= -55) return tr("sehr gut", "excellent");
  if (r >= -67) return tr("gut", "good");
  if (r >= -75) return tr("mittel", "fair");
  if (r >= -85) return tr("schwach", "weak");
  return tr("sehr schwach", "very weak");
}

const char *Net::phaseName() const {
  switch (_phase) {
    case NetPhase::Connecting: return "connecting";
    case NetPhase::GotLink:    return "link";
    case NetPhase::Connected:  return "connected";
    case NetPhase::Failed:     return "failed";
    default:                   return "idle";
  }
}

String Net::phaseText() const {
  switch (_phase) {
    case NetPhase::Connecting: return tr("Suche Netzwerk und melde mich an", "Looking for the network and authenticating");
    case NetPhase::GotLink:    return tr("Angemeldet, warte auf IP-Adresse vom Router", "Associated, waiting for an IP address from the router");
    case NetPhase::Connected:  return tr("Verbunden", "Connected");
    case NetPhase::Failed:     return tr("Fehlgeschlagen", "Failed");
    default:
      return settings.hasWifi() ? tr("Nicht verbunden", "Not connected") : tr("Kein WLAN eingerichtet", "No Wi-Fi configured");
  }
}

String Net::reasonText(uint8_t reason) const {
  switch (reason) {
    case WIFI_REASON_NO_AP_FOUND:
      return tr("Netzwerk nicht gefunden. Der ESP32-C3 kann nur 2,4-GHz-Netze; Namen prüfen und näher an den Router gehen.",
                "Network not found. The ESP32-C3 only supports 2.4 GHz; check the name and move closer to the router.");
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_802_1X_AUTH_FAILED:
      return tr("Passwort abgelehnt. Bitte prüfen (Groß-/Kleinschreibung). Reine WPA3-Netze werden nicht unterstützt.",
                "Password rejected. Please check it (case matters). WPA3-only networks are not supported.");
    case WIFI_REASON_AUTH_EXPIRE:
      return tr("Anmeldung am Router abgelaufen. Meist falsches Passwort oder zu schwacher Empfang.",
                "Authentication with the router timed out. Usually a wrong password or a weak signal.");
    case WIFI_REASON_ASSOC_FAIL:
    case WIFI_REASON_ASSOC_EXPIRE:
    case WIFI_REASON_ASSOC_TOOMANY:
    case WIFI_REASON_CONNECTION_FAIL:
    case WIFI_REASON_BEACON_TIMEOUT:
      return tr("Der Router hat die Verbindung abgebrochen. Meist zu schwacher Empfang: näher an den Router, andere Sendeleistung probieren.",
                "The router dropped the connection. Usually a weak signal: move closer, try another transmit power.");
    case 0:
      return tr("Keine Antwort vom Router (Zeitüberschreitung).", "No answer from the router (timeout).");
    default: {
      String s = String(tr("Fehler ", "Error ")) + String(reason);
      const char *n = WiFi.disconnectReasonName((wifi_err_reason_t)reason);
      if (n && *n) s += String(" (") + n + ")";
      return s;
    }
  }
}

// ------------------------------------------------------------------ station

void Net::kickSta(const String &ssid, const String &pass) {
  uint32_t now = millis();
  WiFi.mode(_apUp ? WIFI_AP_STA : WIFI_STA);
  WiFi.setSleep(false);  // power save makes the board vanish after a few idle minutes
  WiFi.disconnect(false, false);
  WiFi.begin(ssid.c_str(), pass.length() ? pass.c_str() : nullptr);
  WiFi.setSleep(false);
  applyTxPower();
  _inProgress = true;
  _lastKick = now ? now : 1;
  _attempts++;
  if (_phase != NetPhase::Connected) _phase = NetPhase::Connecting;
  Serial.printf("[wifi] Verbinde mit '%s' (Versuch %u)\n", ssid.c_str(), (unsigned)_attempts);
}

void Net::handleGotIp(uint32_t now) {
  _inProgress = false;
  bool wasDown = _connectedAt == 0 || _downSince != 0;
  if (_connectedAt != 0 && wasDown) _reconnects++;
  _connectedAt = now ? now : 1;
  _downSince = 0;
  _apTimedOut = false;  // a future outage may open the fallback AP again
  _phase = NetPhase::Connected;
  _failText = "";
  // Only an IP obtained after this test's own kick counts (a DHCP renewal of
  // the old network while the kick is still deferred behind a scan does not).
  if (_testing && _lastKick != 0) _testGotIp = true;
  applyTxPower();
  startMdns();
  Serial.printf("[wifi] Verbunden: %s  IP %s  Kanal %d  RSSI %d dBm (%s)  Hostname %s\n",
                WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), (int)WiFi.channel(),
                (int)WiFi.RSSI(), rssiText().c_str(), settings.hostname.c_str());
}

void Net::handleDisconnect(uint32_t now, uint8_t reason) {
  _lastDiscAt = now ? now : 1;
  if (_downSince == 0) _downSince = now ? now : 1;
  if (_phase == NetPhase::Connected) _phase = NetPhase::Connecting;
  if (reason == WIFI_REASON_ASSOC_LEAVE) {
    // Our own WiFi.disconnect() before begin()/scan. Says nothing about the
    // network and must not abort the connect attempt that follows it.
    Serial.println("[wifi] Getrennt (eigene Abmeldung)");
    return;
  }
  _inProgress = false;
  _lastReason = reason;
  if (_testing) {
    _testDiscs++;
    if (reason == WIFI_REASON_AUTH_FAIL || reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
        reason == WIFI_REASON_HANDSHAKE_TIMEOUT || reason == WIFI_REASON_802_1X_AUTH_FAILED)
      _testAuthFails++;
  }
  Serial.printf("[wifi] Getrennt (%u %s)\n", (unsigned)reason,
                WiFi.disconnectReasonName((wifi_err_reason_t)reason));
}

void Net::startMdns() {
  if (_mdnsUp) return;
  if (MDNS.begin(settings.hostname.c_str())) {
    MDNS.addService("http", "tcp", 80);
    _mdnsUp = true;
    Serial.printf("[wifi] Erreichbar als http://%s.local/\n", settings.hostname.c_str());
  } else {
    Serial.println("[wifi] mDNS konnte nicht gestartet werden");
  }
}

// ------------------------------------------------------------------ access point

void Net::openAp() {
  if (_apUp) return;
  WiFi.mode(WIFI_AP_STA);
  // While the setup network is up we retry the station ourselves, slowly:
  // the core's auto-reconnect would scan all channels every few seconds and
  // keep kicking phones off the setup network.
  WiFi.setAutoReconnect(false);
  int channel = staConnected() ? WiFi.channel() : 1;
  const char *pw = settings.apPass.length() >= 8 ? settings.apPass.c_str() : nullptr;
  if (!WiFi.softAP(AP_SSID, pw, channel, 0, 4)) {
    WiFi.softAP(AP_SSID, nullptr, channel, 0, 4);
    pw = nullptr;
  }
  _apSecured = pw != nullptr;
  delay(150);
  _dns.begin(WiFi.softAPIP());
  applyTxPower();
  _apUp = true;
  _closeAt = 0;  // a pending close never outlives the AP it targeted
  _apOpenedAt = millis();
  Serial.printf("[wifi] Setup-Netz '%s' %s -> http://%s/\n", AP_SSID, pw ? "(WPA2)" : "(offen)",
                WiFi.softAPIP().toString().c_str());
}

void Net::closeAp() {
  if (!_apUp) return;
  _dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  _apUp = false;
  _apHoldUntil = 0;
  _closeAt = 0;
  Serial.println("[wifi] Setup-Netz geschlossen");
}

bool Net::openPortal(uint32_t holdMs) {
  _apTimedOut = false;  // explicitly asked for: the timeout starts afresh
  openAp();
  uint32_t until = millis() + holdMs;
  if (until == 0) until = 1;
  _apHoldUntil = until;
  return true;
}

bool Net::closePortal() {
  if (!_apUp) return true;
  if (!staConnected() || _testing) return false;
  closeAp();
  return true;
}

void Net::closePortalSoon(uint32_t inMs) {
  if (!_apUp) return;  // nothing to close; a stale timestamp would kill the next AP at once
  uint32_t t = millis() + inMs;
  _closeAt = t ? t : 1;
}

void Net::forgetWifi() {
  settings.lock();
  settings.wifiSsid = "";
  settings.wifiPass = "";
  settings.save();
  settings.unlock();
  _staWanted = false;
  WiFi.disconnect(false, false);
}

// ------------------------------------------------------------------ scanning

bool Net::startScan() {
  if (_scanRunning) return true;
  if (_testing) return false;  // a scan would abort the connect attempt under test
  // A scan cannot start while a connect attempt is running; abort that, the
  // supervisor retries later anyway.
  if (_inProgress && !staConnected()) {
    WiFi.disconnect(false, false);
    _inProgress = false;
    delay(100);
  }
  int16_t r = WiFi.scanNetworks(true, false, false, SCAN_MS_PER_CHAN);
  if (r == WIFI_SCAN_FAILED) {
    _scanFailed = true;
    return false;
  }
  _scanRunning = true;
  _scanFailed = false;
  _scanStartedAt = millis();
  return true;
}

bool Net::scanning() { return _scanRunning; }

void Net::pollScan(uint32_t now) {
  if (!_scanRunning) return;
  int16_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) {
    if (now - _scanStartedAt > SCAN_TIMEOUT_MS) {
      _scanRunning = false;
      _scanFailed = true;
      WiFi.scanDelete();
    }
    return;
  }
  _scanRunning = false;
  if (n < 0) {
    _scanFailed = true;
    return;
  }
  _scan.clear();
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (s.length() == 0) continue;
    int rssi = WiFi.RSSI(i);
    bool dup = false;
    for (auto &e : _scan) {
      if (e.ssid == s) {  // several access points with one name: keep the strongest
        dup = true;
        if (rssi > e.rssi) {
          e.rssi = rssi;
          e.channel = WiFi.channel(i);
        }
        break;
      }
    }
    if (dup) continue;
    ScanEntry e;
    e.ssid = s;
    e.rssi = rssi;
    e.channel = WiFi.channel(i);
    e.secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    _scan.push_back(e);
  }
  std::sort(_scan.begin(), _scan.end(), [](const ScanEntry &a, const ScanEntry &b) { return a.rssi > b.rssi; });
  WiFi.scanDelete();
  _scanDone = true;
  Serial.printf("[wifi] Scan: %d Netze\n", (int)_scan.size());
}

void Net::fillScan(JsonArray arr) {
  int count = 0;
  for (auto &e : _scan) {
    if (count++ >= 30) break;
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = e.ssid;
    o["rssi"] = e.rssi;
    o["ch"] = e.channel;
    o["secure"] = e.secure;
  }
}

// ------------------------------------------------------------------ portal test

bool Net::startTest(const String &ssid, const String &pass, uint32_t seq) {
  if (_testing) return false;
  _testSsid = ssid;
  _testPass = pass;
  _testSeq = seq;
  _testing = true;
  _testStart = millis();
  _testAuthFails = 0;
  _testDiscs = 0;
  _testGotIp = false;
  _lastReason = 0;
  _failText = "";
  _evGotIp = false;  // a GOT_IP still pending for the old network is not this test's result
  _evLink = false;
  _restoreAfterTest = settings.hasWifi();
  // Keep (or open) the setup network so the phone can watch the result.
  if (!_apUp) openPortal(AP_GRACE_MS);
  // Already on exactly this network with this password: nothing to try.
  // (WiFi.begin() with an unchanged config returns without reconnecting,
  // which would otherwise look like a 12 s stall.)
  if (staConnected() && WiFi.SSID() == ssid && pass == settings.wifiPass) {
    _lastKick = millis() ? millis() : 1;
    _testGotIp = true;
    return true;
  }
  if (_scanRunning) {
    // begin() would fail while the driver scans; testLoop() kicks as soon as
    // the scan is over.
    _inProgress = false;
    _lastKick = 0;
    _lastDiscAt = 0;
  } else {
    kickSta(ssid, pass);
  }
  return true;
}

void Net::finishTest(uint32_t now, bool ok, const String &why) {
  _testing = false;
  _lastTestSeq = _testSeq;
  _lastTestOk = ok;
  _lastTestSsid = _testSsid;
  _lastTestText = ok ? String("") : why;  // survives the reconnect to the stored network
  if (ok) {
    settings.lock();
    settings.wifiSsid = _testSsid;
    settings.wifiPass = _testPass;
    settings.save();
    settings.unlock();
    _staWanted = true;
    _phase = NetPhase::Connected;
    _failText = "";
    Serial.println("[wifi] Test erfolgreich, Zugangsdaten gespeichert");
  } else {
    _phase = NetPhase::Failed;
    _failText = why;
    WiFi.disconnect(false, false);
    _inProgress = false;
    Serial.printf("[wifi] Test fehlgeschlagen: %s\n", why.c_str());
    _staWanted = _restoreAfterTest;
    // Go back to the stored network in a few seconds.
    _lastKick = now - KICK_AP_MS + 8000;
  }
}

void Net::testLoop(uint32_t now, bool up) {
  // Success only counts when this test produced the IP (not a leftover
  // connection to the previous network).
  if (_testGotIp && up && WiFi.SSID() == _testSsid) {
    finishTest(now, true, "");
    return;
  }
  if (_testAuthFails >= 2) {
    finishTest(now, false, reasonText(_lastReason ? _lastReason : WIFI_REASON_AUTH_FAIL));
    return;
  }
  if (_testDiscs >= 4) {
    finishTest(now, false, reasonText(_lastReason));
    return;
  }
  if (_evLink && _phase == NetPhase::Connecting) _phase = NetPhase::GotLink;
  if (_phase == NetPhase::GotLink && now - _lastKick > TEST_DHCP_MS) {
    finishTest(now, false, tr("Angemeldet, aber der Router hat keine IP-Adresse vergeben (DHCP). Router prüfen oder neu starten.",
                              "Associated, but the router did not hand out an IP address (DHCP). Check or restart the router."));
    return;
  }
  if (now - _testStart > TEST_TIMEOUT_MS) {
    finishTest(now, false, reasonText(_lastReason));
    return;
  }
  // begin() may fail silently (e.g. a scan was running): no event ever
  // arrives. Treat a long silence as "try again".
  if (_inProgress && _phase == NetPhase::Connecting && now - _lastKick > TEST_STALL_MS) _inProgress = false;
  // The core does not retry while the AP is up (auto-reconnect off); try
  // again ourselves shortly after each failure so "network not found" gets
  // a few scans before we give up.
  if (!_scanRunning && !_inProgress && now - _lastDiscAt > 1500 && now - _lastKick > 3000) {
    _evLink = false;
    kickSta(_testSsid, _testPass);
  }
}

// ------------------------------------------------------------------ supervision

void Net::supervise(uint32_t now, bool up) {
  if (up) return;
  if (!_staWanted) return;
  uint32_t down = now - _downSince;

  if (_scanRunning) {
    // A connect attempt would abort the scan someone requested in the portal.
  } else if (_apUp) {
    // Slow cadence: every attempt scans all channels and briefly unsettles
    // phones on the setup network.
    bool stalled = _inProgress && now - _lastKick > STALL_MS;
    if ((!_inProgress && now - _lastKick > KICK_AP_MS) || stalled) kickSta(settings.wifiSsid, settings.wifiPass);
  } else {
    // The core's auto-reconnect handles the usual cases within seconds; we
    // only step in when nothing has happened for a while.
    uint32_t lastActivity = _lastDiscAt > _lastKick ? _lastDiscAt : _lastKick;
    if (now - lastActivity > KICK_STA_MS) kickSta(settings.wifiSsid, settings.wifiPass);
  }

  // Fallback AP — but not again after it already timed out during this
  // outage; the user asked for it to stay off until someone opens it.
  if (!_apUp && !_apTimedOut) {
    bool neverConnected = _connectedAt == 0;
    if (down > (neverConnected ? BOOT_FALLBACK_MS : DOWN_FALLBACK_MS)) {
      Serial.printf("[wifi] Seit %lu s keine Verbindung -> Setup-Netz als Fallback\n", (unsigned long)(down / 1000));
      openAp();
    }
  }

  if (down > REBOOT_DOWN_MS && apClients() == 0) {
    Serial.println("[wifi] 30 min ohne Verbindung und niemand im Setup-Netz -> Neustart");
    Serial.flush();
    delay(100);
    ESP.restart();
  }
}

void Net::apHousekeeping(uint32_t now, bool up) {
  if (!_apUp) return;
  if (_closeAt && (int32_t)(now - _closeAt) > 0) {
    _closeAt = 0;
    if (up && !_testing) closeAp();
    return;
  }
  if (_testing) return;
  if (_apHoldUntil && (int32_t)(_apHoldUntil - now) > 0) return;
  if (up) {
    uint32_t sinceUp = now - _connectedAt;
    if (sinceUp < AP_GRACE_MS) return;
    if (apClients() == 0 || sinceUp > AP_MAX_AFTER_UP_MS) closeAp();
    return;
  }
  // Station down: the setup network still goes away after the configured
  // time so an unattended board does not broadcast it for ever. A 5 s press
  // on a button, button A at boot, the web UI or the flash page (USB) bring
  // it back; the station keeps retrying regardless.
  if (settings.apTimeoutMin == 0) return;
  uint32_t age = now - _apOpenedAt;
  uint32_t limit = (uint32_t)settings.apTimeoutMin * 60000UL;
  if (age < limit) return;
  if (apClients() == 0 || age > limit + AP_MAX_AFTER_UP_MS) {
    Serial.printf("[wifi] Setup-Netz nach %u min ohne Heimnetz geschlossen\n", (unsigned)settings.apTimeoutMin);
    _apTimedOut = true;
    closeAp();
  }
}

// ------------------------------------------------------------------ lifecycle

void Net::begin(bool forcePortal) {
  uint32_t now = millis();
  _bootAt = now;
  _downSince = now ? now : 1;
  WiFi.persistent(false);
  WiFi.onEvent(onEvent);
  // Must precede the first WiFi.mode(): the core copies the name when it
  // creates the network interface.
  WiFi.setHostname(settings.hostname.c_str());
  WiFi.setAutoReconnect(true);

  _staWanted = settings.hasWifi();
  // Open the setup network first so the mode is settled before the station
  // starts connecting (a mode change mid-association can abort the attempt).
  if (forcePortal || !_staWanted) openPortal(forcePortal ? 300000 : 0);
  if (_staWanted) kickSta(settings.wifiSsid, settings.wifiPass);
  else startScan();  // have the list ready when the phone arrives
}

void Net::loop() {
  uint32_t now = millis();
  if (_apUp) _dns.poll();

  if (_evGotIp) {
    _evGotIp = false;
    handleGotIp(now);
  }
  if (_evDisc) {
    _evDisc = false;
    uint8_t r = _evReason;
    // A disconnect that the core's auto-reconnect has already healed must not
    // mark the link as down after the GOT_IP that followed it.
    if (!staConnected()) handleDisconnect(now, r);
    else if (r != WIFI_REASON_ASSOC_LEAVE) _lastReason = r;
  }

  bool up = staConnected();
  if (up) {
    _downSince = 0;
    if (!_testing && _phase != NetPhase::Connected) _phase = NetPhase::Connected;
  } else if (_downSince == 0) {
    _downSince = now ? now : 1;
  }

  pollScan(now);
  if (_testing) testLoop(now, up);
  else supervise(now, up);
  apHousekeeping(now, up);
}

void Net::fillStatus(JsonObject o) {
  bool up = staConnected();
  o["sta"] = up;
  o["ssid"] = currentSsid();
  o["ip"] = up ? WiFi.localIP().toString() : "";
  o["rssi"] = rssi();
  o["rssiText"] = rssiText();
  o["hostname"] = settings.hostname;
  o["ap"] = _apUp;
  o["apSsid"] = AP_SSID;
  o["apIp"] = _apUp ? WiFi.softAPIP().toString() : "";
  o["apClients"] = apClients();
  o["apOpen"] = !_apSecured;
  o["apTimedOut"] = _apTimedOut;
  o["apTimeoutMin"] = settings.apTimeoutMin;
  o["hasWifi"] = settings.hasWifi();
  o["testing"] = _testing;
  o["testSeq"] = _testing ? _testSeq : _lastTestSeq;
  o["testDone"] = !_testing && _lastTestSeq != 0;
  o["testOk"] = _lastTestOk;
  o["testSsid"] = _lastTestSsid;
  o["testText"] = _lastTestText;
  o["phase"] = phaseName();
  o["phaseText"] = phaseText();
  o["reason"] = _failText;
  // Why the supervisor's last attempt failed (our own disconnects excluded).
  o["lastReason"] = (!up && _lastReason && _lastReason != WIFI_REASON_ASSOC_LEAVE) ? reasonText(_lastReason) : String("");
  o["upSec"] = upSeconds();
  o["downSec"] = downSeconds();
  o["attempts"] = _attempts;
  o["reconnects"] = _reconnects;
  o["txPower"] = settings.txPower;
  o["channel"] = up ? (int)WiFi.channel() : 0;
  o["mac"] = WiFi.macAddress();
}
