#include "Settings.h"
#include <Preferences.h>

Settings settings;
static Preferences prefs;
static const char *NS = "bambutton";
static SemaphoreHandle_t cfgMux = nullptr;

void Settings::lock() {
  if (cfgMux) xSemaphoreTake(cfgMux, portMAX_DELAY);
}

void Settings::unlock() {
  if (cfgMux) xSemaphoreGive(cfgMux);
}

bool Settings::validTxPower(int v) {
  switch (v) {
    case 78: case 68: case 60: case 52: case 44: case 34: case 28: case 20: case 8:
      return true;
    default:
      return false;
  }
}

String Settings::cleanHostname(const String &raw) {
  String out;
  for (size_t i = 0; i < raw.length() && out.length() < 24; i++) {
    char c = raw[i];
    if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    if (ok) out += c;
    else if ((c == '-' || c == ' ' || c == '_' || c == '.') && out.length() && out[out.length() - 1] != '-') out += '-';
  }
  while (out.length() && out[out.length() - 1] == '-') out.remove(out.length() - 1);
  if (out.length() == 0) out = "bambutton";
  return out;
}

void Settings::load() {
  if (!cfgMux) cfgMux = xSemaphoreCreateMutex();
  const int defLed[STATION_COUNT] = {3, 5};
  const int defBtn[STATION_COUNT] = {4, 6};
  prefs.begin(NS, true);
  wifiSsid = prefs.getString("ssid", "");
  wifiPass = prefs.getString("pass", "");
  hostname = prefs.getString("hostname", "bambutton");
  apPass = prefs.getString("ap_pass", "");
  host = prefs.getString("bb_host", "");
  apiKey = prefs.getString("bb_key", "");
  apiEnabled = prefs.getBool("bb_on", true);
  pollIntervalMs = prefs.getUInt("poll_ms", 3000);
  httpTimeoutMs = prefs.getUInt("http_ms", 20000);
  txPower = (int8_t)prefs.getInt("txpwr", 34);
  idleLed = (uint8_t)prefs.getUChar("idleled", IDLE_FOLLOW_LIGHT);
  lang = (uint8_t)prefs.getUChar("lang", LANG_DE);
  for (int i = 0; i < STATION_COUNT; i++) {
    char k[8];
    snprintf(k, sizeof(k), "p%d", i);
    stations[i].printerId = prefs.getInt(k, 0);
    snprintf(k, sizeof(k), "l%d", i);
    stations[i].ledPin = prefs.getInt(k, defLed[i]);
    snprintf(k, sizeof(k), "b%d", i);
    stations[i].buttonPin = prefs.getInt(k, defBtn[i]);
  }
  prefs.end();

  // Sanity: a corrupted or hand-edited value must never brick the board.
  hostname = cleanHostname(hostname);
  if (pollIntervalMs < 1500) pollIntervalMs = 1500;
  if (pollIntervalMs > 60000) pollIntervalMs = 60000;
  if (httpTimeoutMs < 3000) httpTimeoutMs = 3000;
  if (httpTimeoutMs > 60000) httpTimeoutMs = 60000;
  if (!validTxPower(txPower)) txPower = 34;
  if (idleLed > IDLE_OFF) idleLed = IDLE_FOLLOW_LIGHT;
  if (lang > LANG_EN) lang = LANG_DE;
  if (apPass.length() > 0 && apPass.length() < 8) apPass = "";
  if (apPass.length() > 63) apPass = apPass.substring(0, 63);
}

void Settings::save() {
  prefs.begin(NS, false);
  prefs.putString("ssid", wifiSsid);
  prefs.putString("pass", wifiPass);
  prefs.putString("hostname", hostname);
  prefs.putString("ap_pass", apPass);
  prefs.putString("bb_host", host);
  prefs.putString("bb_key", apiKey);
  prefs.putBool("bb_on", apiEnabled);
  prefs.putUInt("poll_ms", pollIntervalMs);
  prefs.putUInt("http_ms", httpTimeoutMs);
  prefs.putInt("txpwr", txPower);
  prefs.putUChar("idleled", idleLed);
  prefs.putUChar("lang", lang);
  for (int i = 0; i < STATION_COUNT; i++) {
    char k[8];
    snprintf(k, sizeof(k), "p%d", i);
    prefs.putInt(k, stations[i].printerId);
    snprintf(k, sizeof(k), "l%d", i);
    prefs.putInt(k, stations[i].ledPin);
    snprintf(k, sizeof(k), "b%d", i);
    prefs.putInt(k, stations[i].buttonPin);
  }
  prefs.end();
}

String Settings::baseUrlFor(const String &host) {
  String h = host;
  h.trim();
  if (h.startsWith("http://")) h = h.substring(7);
  else if (h.startsWith("https://")) h = h.substring(8);
  while (h.endsWith("/")) h.remove(h.length() - 1);
  if (h.endsWith("/api/v1")) h = h.substring(0, h.length() - 7);
  while (h.endsWith("/")) h.remove(h.length() - 1);
  return "http://" + h + "/api/v1";
}
