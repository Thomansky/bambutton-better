#pragma once
#include <Arduino.h>

#define STATION_COUNT 2
#define FW_VERSION "2.1.0"

struct StationCfg {
  int printerId = 0;  // 0 = no printer assigned
  int ledPin = 3;
  int buttonPin = 4;
};

// What the LED shows while nothing special is going on.
enum IdleLed : uint8_t { IDLE_FOLLOW_LIGHT = 0, IDLE_ON = 1, IDLE_OFF = 2 };

// UI language. Firmware-generated messages follow it too.
enum Lang : uint8_t { LANG_DE = 0, LANG_EN = 1 };

class Settings {
 public:
  String wifiSsid;
  String wifiPass;
  String hostname = "bambutton";
  String apPass;             // empty = open setup network, else WPA2 (8..63 chars)
  String host;               // "192.168.1.50:8000" — no scheme, no /api/v1
  String apiKey;
  bool apiEnabled = true;    // polling can be switched off without losing the config
  StationCfg stations[STATION_COUNT];
  uint32_t pollIntervalMs = 3000;
  // Clearing a plate makes Bambuddy talk to the printer, which can take
  // several seconds. A short timeout is the classic cause of "the button
  // stops blinking but nothing happens". Status polls use a shorter one.
  uint32_t httpTimeoutMs = 20000;
  // Wi-Fi transmit power in wifi_power_t units (quarter dBm). The ESP32-C3
  // Super Mini's antenna is badly matched; at full power (78 = 19.5 dBm) many
  // boards cannot connect at all. 34 = 8.5 dBm is the widely used fix.
  int8_t txPower = 34;
  uint8_t idleLed = IDLE_FOLLOW_LIGHT;
  uint8_t lang = LANG_DE;
  // The setup network closes by itself after this many minutes without a
  // client, even when the home network is unreachable (0 = never). A 5 s
  // press on any button, button A at boot, the web UI or the flash page
  // (USB) open it again.
  uint8_t apTimeoutMin = 15;

  void load();
  void save();
  bool hasWifi() const { return wifiSsid.length() > 0; }
  bool hasApi() const { return host.length() > 0 && apiKey.length() > 0; }
  String baseUrl() const { return baseUrlFor(host); }

  // Accepts "1.2.3.4:8000", "http://1.2.3.4:8000/" or a full ".../api/v1"
  // URL and returns a clean "http://host[:port]/api/v1".
  static String baseUrlFor(const String &host);
  // "Mein Knopf!" -> "mein-knopf"; empty -> "bambutton". RFC 1123 labels only.
  static String cleanHostname(const String &raw);
  static bool validTxPower(int v);

  // The web handlers (loop task) and the Bambuddy worker task both use the
  // String members. Copy what you need under the lock; never hold it across
  // a network call.
  void lock();
  void unlock();
};

extern Settings settings;

// Pick the text for the configured language.
inline const char *tr(const char *de, const char *en) { return settings.lang == LANG_EN ? en : de; }
