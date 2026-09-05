#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <vector>

#define AP_SSID "Bambutton-Setup"

// Minimal DNS responder for the captive portal: every A query gets the AP
// address. The core's DNSServer rejects queries that carry an EDNS0 OPT
// record (ARCOUNT != 0), which modern phones and Windows send, so those
// devices would never see the portal page.
class CaptiveDns {
 public:
  void begin(IPAddress ip);
  void stop();
  void poll();

 private:
  WiFiUDP _udp;
  IPAddress _ip;
  bool _up = false;
  uint8_t _buf[512];
};

// Wi-Fi supervisor. Drives station mode and the setup access point as one
// state machine from loop(); Wi-Fi events only set flags.
//
//  * Boot with credentials: connect as station. If that has not worked after
//    30 s (router still booting after a power cut, wrong password, moved to a
//    new network …) the setup network is opened *in addition* and the station
//    keeps retrying in the background. As soon as the station connects, the
//    setup network closes again by itself.
//  * Boot without credentials (or button A held): setup network only.
//  * Portal "connect": credentials are tried live while the phone stays on the
//    setup network; only a successful connection is stored. The result (IP,
//    hostname, signal) is shown before the setup network goes away.
//  * Any later loss of the connection is retried forever; after 60 s the setup
//    network appears as a fallback, after 30 min without any link and without
//    anyone on the setup network the board reboots as a last resort.
enum class NetPhase : uint8_t { Idle, Connecting, GotLink, Connected, Failed };

struct ScanEntry {
  String ssid;
  int rssi;
  int channel;
  bool secure;
};

class Net {
 public:
  void begin(bool forcePortal);
  void loop();

  // ---- setup portal actions
  bool startTest(const String &ssid, const String &pass, uint32_t seq);  // false while another test runs
  bool openPortal(uint32_t holdMs);                         // open the setup network on demand
  bool closePortal();                                       // only allowed while the station is connected
  void closePortalSoon(uint32_t inMs);                      // same, but after the HTTP reply went out
  void forgetWifi();                                        // erase credentials (caller reboots)
  bool startScan();
  bool scanning();
  bool scanFailed() const { return _scanFailed; }
  bool hasScan() const { return _scanDone; }
  void fillScan(JsonArray arr);

  // ---- state
  bool staConnected() const { return WiFi.status() == WL_CONNECTED; }
  bool apActive() const { return _apUp; }
  bool testing() const { return _testing; }
  bool hasCredentials() const;
  NetPhase phase() const { return _phase; }
  const char *phaseName() const;
  String phaseText() const;          // for humans, in German
  String failReason() const { return _failText; }
  String currentSsid() const;
  IPAddress apIp() const { return WiFi.softAPIP(); }
  int apClients() const { return _apUp ? WiFi.softAPgetStationNum() : 0; }
  uint32_t upSeconds() const;
  uint32_t downSeconds() const;
  uint32_t attempts() const { return _attempts; }
  uint32_t reconnects() const { return _reconnects; }
  int rssi() const { return staConnected() ? WiFi.RSSI() : 0; }
  String rssiText() const;
  void applyTxPower();
  void applyHostname();              // after the hostname changed (DHCP name + mDNS)
  void fillStatus(JsonObject o);     // everything the UI wants to know

 private:
  static void onEvent(WiFiEvent_t ev, WiFiEventInfo_t info);
  void kickSta(const String &ssid, const String &pass);
  void openAp();
  void closeAp();
  void handleGotIp(uint32_t now);
  void handleDisconnect(uint32_t now, uint8_t reason);
  void testLoop(uint32_t now, bool up);
  void supervise(uint32_t now, bool up);
  void apHousekeeping(uint32_t now, bool up);
  void pollScan(uint32_t now);
  void finishTest(uint32_t now, bool ok, const String &why);
  String reasonText(uint8_t reason) const;
  void startMdns();

  CaptiveDns _dns;
  std::vector<ScanEntry> _scan;
  String _testSsid, _testPass, _failText, _lastTestSsid, _lastTestText;
  NetPhase _phase = NetPhase::Idle;

  bool _apUp = false, _apSecured = false, _staWanted = false, _testing = false, _inProgress = false;
  bool _mdnsUp = false, _scanRunning = false, _scanDone = false, _scanFailed = false;
  bool _restoreAfterTest = false, _testGotIp = false, _lastTestOk = false;
  uint32_t _bootAt = 0, _connectedAt = 0, _downSince = 0, _lastKick = 0, _lastDiscAt = 0;
  uint32_t _testStart = 0, _apOpenedAt = 0, _apHoldUntil = 0, _closeAt = 0, _scanStartedAt = 0;
  uint32_t _attempts = 0, _reconnects = 0, _testSeq = 0, _lastTestSeq = 0;
  uint8_t _lastReason = 0, _testAuthFails = 0, _testDiscs = 0;

  // written by the Wi-Fi event task, consumed in loop()
  volatile bool _evGotIp = false, _evDisc = false, _evLink = false;
  volatile uint8_t _evReason = 0;
};

extern Net net;
