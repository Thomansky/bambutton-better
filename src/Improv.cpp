#include "Improv.h"
#include "Net.h"
#include "Settings.h"
#include <WiFi.h>

ImprovSerial improv;

static const char HDR[6] = {'I', 'M', 'P', 'R', 'O', 'V'};
static const uint32_t SCAN_WAIT_MS = 15000;

void ImprovSerial::begin(Stream &s) { _s = &s; }

// ------------------------------------------------------------------ outgoing

void ImprovSerial::sendPacket(uint8_t type, const uint8_t *data, size_t len) {
  if (!_s || len > 255) return;
  uint8_t out[6 + 3 + 255 + 1];
  size_t o = 0;
  memcpy(out, HDR, 6);
  o += 6;
  out[o++] = 1;  // protocol version
  out[o++] = type;
  out[o++] = (uint8_t)len;
  memcpy(out + o, data, len);
  o += len;
  uint8_t sum = 0;
  for (size_t i = 0; i < o; i++) sum += out[i];
  out[o++] = sum;
  _s->write(out, o);
}

void ImprovSerial::sendState(uint8_t state) { sendPacket(TYPE_STATE, &state, 1); }

void ImprovSerial::sendError(uint8_t err) { sendPacket(TYPE_ERROR, &err, 1); }

// RPC result: [command][length of the string block][len][string]...
void ImprovSerial::sendResult(uint8_t cmd, const std::vector<String> &strings) {
  uint8_t data[255];
  size_t o = 2;
  for (const String &s : strings) {
    size_t n = s.length();
    if (n > 200) n = 200;
    if (o + 1 + n > sizeof(data)) break;
    data[o++] = (uint8_t)n;
    memcpy(data + o, s.c_str(), n);
    o += n;
  }
  data[0] = cmd;
  data[1] = (uint8_t)(o - 2);
  sendPacket(TYPE_RESULT, data, o);
}

std::vector<String> ImprovSerial::deviceUrls() {
  std::vector<String> urls;
  if (net.staConnected()) urls.push_back("http://" + WiFi.localIP().toString() + "/");
  urls.push_back("http://" + settings.hostname + ".local/");
  return urls;
}

// ------------------------------------------------------------------ incoming

void ImprovSerial::feed(uint8_t b) {
  uint32_t now = millis();
  if (_pos && now - _lastByte > 1000) _pos = 0;  // long pause: resynchronise
  _lastByte = now;

  if (_pos < 6) {
    // Hunting for the header inside whatever else arrives on the port.
    if (b == (uint8_t)HDR[_pos]) {
      _buf[_pos++] = b;
    } else if (b == (uint8_t)HDR[0]) {
      _buf[0] = b;
      _pos = 1;
    } else {
      _pos = 0;
    }
    return;
  }
  if (_pos >= sizeof(_buf)) {
    _pos = 0;
    return;
  }
  _buf[_pos++] = b;
  if (_pos < 9) return;  // header(6) + version + type + length
  size_t total = 10 + _buf[8];
  if (total > sizeof(_buf)) {
    _pos = 0;
    return;
  }
  if (_pos < total) return;

  uint8_t sum = 0;
  for (size_t i = 0; i < total - 1; i++) sum += _buf[i];
  if (sum == _buf[total - 1] && _buf[6] == 1) handlePacket(_buf[7], _buf + 9, _buf[8]);
  else sendError(ERR_INVALID_RPC);
  _pos = 0;
}

void ImprovSerial::handlePacket(uint8_t type, const uint8_t *data, uint8_t len) {
  if (type == TYPE_RPC) handleRpc(data, len);
  // State/error/result packets only travel from the device to the host.
}

void ImprovSerial::handleRpc(const uint8_t *data, uint8_t len) {
  if (len < 2 || data[1] + 2 > len) {
    sendError(ERR_INVALID_RPC);
    return;
  }
  uint8_t cmd = data[0];
  const uint8_t *p = data + 2;
  uint8_t plen = data[1];

  switch (cmd) {
    case CMD_WIFI: {
      if (plen < 1 || p[0] + 1 > plen) { sendError(ERR_INVALID_RPC); return; }
      uint8_t sl = p[0];
      String ssid((const char *)p + 1, sl);
      if (1 + sl >= plen) { sendError(ERR_INVALID_RPC); return; }
      uint8_t pl = p[1 + sl];
      if (2 + sl + pl > plen) { sendError(ERR_INVALID_RPC); return; }
      String pass((const char *)p + 2 + sl, pl);
      _seq = 3000000000UL + (++_n);
      Serial.printf("[improv] WLAN-Zugangsdaten fuer '%s' erhalten\n", ssid.c_str());
      if (!net.startTest(ssid, pass, _seq)) {
        sendError(ERR_UNKNOWN);  // another attempt is running
        return;
      }
      _provisioning = true;
      sendError(ERR_NONE);
      sendState(STATE_PROVISIONING);
      break;
    }
    case CMD_STATE:
      sendError(ERR_NONE);
      if (_provisioning) {
        sendState(STATE_PROVISIONING);
      } else if (net.staConnected()) {
        sendState(STATE_PROVISIONED);
        sendResult(CMD_STATE, deviceUrls());
      } else {
        sendState(STATE_READY);
      }
      break;
    case CMD_INFO: {
      std::vector<String> info = {"Bambutton", FW_VERSION, "ESP32-C3", settings.hostname};
      sendResult(CMD_INFO, info);
      break;
    }
    case CMD_SCAN:
      _scanPending = true;
      _scanRequestedAt = millis();
      net.startScan();  // refused during a test: cached results (if any) are sent below
      break;
    default:
      sendError(ERR_UNKNOWN_CMD);
  }
}

// ------------------------------------------------------------------ loop

void ImprovSerial::loop() {
  if (!_s) return;
  for (int i = 0; i < 64 && _s->available(); i++) feed((uint8_t)_s->read());

  if (_provisioning && !net.testing() && net.lastTestSeq() == _seq) {
    _provisioning = false;
    if (net.lastTestOk()) {
      sendState(STATE_PROVISIONED);
      sendResult(CMD_WIFI, deviceUrls());
      Serial.println("[improv] Verbunden, Zugangsdaten gespeichert");
    } else {
      sendError(ERR_CONNECT);
      sendState(STATE_READY);
      Serial.printf("[improv] Verbindung fehlgeschlagen: %s\n", net.lastTestText().c_str());
    }
  }

  if (_scanPending && (!net.scanning() || millis() - _scanRequestedAt > SCAN_WAIT_MS)) {
    _scanPending = false;
    if (!net.scanning()) {
      for (const ScanEntry &e : net.scanResults()) {
        std::vector<String> row = {e.ssid, String(e.rssi), e.secure ? "YES" : "NO"};
        sendResult(CMD_SCAN, row);
      }
    }
    sendResult(CMD_SCAN, {});  // empty result terminates the list
  }
}
