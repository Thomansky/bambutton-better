#include "Settings.h"
#include <Preferences.h>

Settings settings;
static Preferences prefs;
static const char *NS = "bambutton";

void Settings::load() {
  const int defLed[STATION_COUNT] = {3, 5};
  const int defBtn[STATION_COUNT] = {4, 6};
  prefs.begin(NS, true);
  wifiSsid = prefs.getString("ssid", "");
  wifiPass = prefs.getString("pass", "");
  hostname = prefs.getString("hostname", "bambutton");
  host = prefs.getString("bb_host", "");
  apiKey = prefs.getString("bb_key", "");
  apiEnabled = prefs.getBool("bb_on", true);
  pollIntervalMs = prefs.getUInt("poll_ms", 3000);
  httpTimeoutMs = prefs.getUInt("http_ms", 20000);
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
  if (hostname.length() == 0) hostname = "bambutton";
}

void Settings::save() {
  prefs.begin(NS, false);
  prefs.putString("ssid", wifiSsid);
  prefs.putString("pass", wifiPass);
  prefs.putString("hostname", hostname);
  prefs.putString("bb_host", host);
  prefs.putString("bb_key", apiKey);
  prefs.putBool("bb_on", apiEnabled);
  prefs.putUInt("poll_ms", pollIntervalMs);
  prefs.putUInt("http_ms", httpTimeoutMs);
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

// Accepts "1.2.3.4:8000", "http://1.2.3.4:8000" or a full ".../api/v1" URL
// and always returns a clean "http://host/api/v1".
String Settings::baseUrl() const {
  String h = host;
  h.trim();
  if (h.startsWith("http://")) h = h.substring(7);
  else if (h.startsWith("https://")) h = h.substring(8);
  while (h.endsWith("/")) h.remove(h.length() - 1);
  if (h.endsWith("/api/v1")) h = h.substring(0, h.length() - 7);
  while (h.endsWith("/")) h.remove(h.length() - 1);
  return "http://" + h + "/api/v1";
}
