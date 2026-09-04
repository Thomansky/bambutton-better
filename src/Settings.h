#pragma once
#include <Arduino.h>

#define STATION_COUNT 2

struct StationCfg {
  int printerId = 0;  // 0 = no printer assigned
  int ledPin = 3;
  int buttonPin = 4;
};

class Settings {
 public:
  String wifiSsid;
  String wifiPass;
  String hostname = "bambutton";
  String host;    // "192.168.1.50:8000" — no scheme, no /api/v1
  String apiKey;
  StationCfg stations[STATION_COUNT];
  uint32_t pollIntervalMs = 3000;
  // Clearing a plate makes Bambuddy talk to the printer, which can take
  // several seconds. A short timeout is the classic cause of "the button
  // stops blinking but nothing happens".
  uint32_t httpTimeoutMs = 20000;

  void load();
  void save();
  bool hasWifi() const { return wifiSsid.length() > 0; }
  bool hasApi() const { return host.length() > 0 && apiKey.length() > 0; }
  String baseUrl() const;
};

extern Settings settings;
