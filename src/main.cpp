// Bambutton — physical plate-clear button for Bambuddy (ESP32-C3)
//
// Design notes:
//  * All Bambuddy traffic runs in a worker task, so the web UI stays
//    responsive even while a slow clear-plate call is in flight.
//  * Wi-Fi power save is disabled; with it enabled the board becomes
//    unreachable after a few minutes of idling.
//  * Every API failure is kept in memory and shown in the web UI instead of
//    only being printed to a serial console nobody is watching.
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Update.h>
#include <ArduinoJson.h>

#include "Settings.h"
#include "Bambuddy.h"
#include "Station.h"
#include "page.h"

#define FW_VERSION "1.2.0"
#define AP_SSID "Bambutton-Setup"
// Setup AP is intentionally open: it only runs while the board is
// unconfigured (or the button is held at boot) and never exposes the Wi-Fi
// password or API key over the air.

WebServer server(80);
DNSServer dnsServer;
Station stations[STATION_COUNT];

static bool apMode = false;
static volatile bool pressPending[STATION_COUNT] = {false, false};

// Heartbeats: both the main loop and the worker stamp these. A separate
// watchdog task reboots the board if either stops moving — without this the
// board stays dead until someone unplugs it.
static volatile uint32_t hbLoop = 0;
static volatile uint32_t hbWorker = 0;
static SemaphoreHandle_t mux = nullptr;

struct Diag {
  char lastError[200];
  char lastClear[200];
  uint32_t polls;
  uint32_t errors;
};
static Diag diag = {"", "", 0, 0};

static void lockedCopy(char *dst, size_t n, const String &s) {
  xSemaphoreTake(mux, portMAX_DELAY);
  strncpy(dst, s.c_str(), n - 1);
  dst[n - 1] = 0;
  xSemaphoreGive(mux);
}

// ---------------------------------------------------------------- networking

static bool connectWifi() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // critical: keeps the board reachable
  WiFi.setHostname(settings.hostname.c_str());
  WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPass.c_str());
  Serial.printf("Verbinde mit WLAN '%s' ...\n", settings.wifiSsid.c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) delay(200);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WLAN fehlgeschlagen");
    return false;
  }
  WiFi.setSleep(false);
  Serial.print("Verbunden, IP: ");
  Serial.println(WiFi.localIP());
  Serial.printf("Hostname: %s\n", settings.hostname.c_str());
  return true;
}

static void startAp() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  delay(200);
  dnsServer.start(53, "*", WiFi.softAPIP());
  Serial.print("Setup-AP aktiv: " AP_SSID " -> http://");
  Serial.println(WiFi.softAPIP());
}

// ------------------------------------------------------------- worker thread

static void recordClear(int idx, const ApiResult &r) {
  String s = "Knopf " + String(idx == 0 ? "A" : "B") + ": ";
  s += r.ok ? "OK" : "FEHLER";
  s += " HTTP " + String(r.status) + " nach " + String(r.ms) + " ms";
  if (!r.ok && r.error.length()) s += " — " + r.error;
  lockedCopy(diag.lastClear, sizeof(diag.lastClear), s);
  if (!r.ok) lockedCopy(diag.lastError, sizeof(diag.lastError), s);
  Serial.println(s);
}

// A stalled task must never mean "dead until power-cycled".
static void watchdogTask(void *) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(5000));
    uint32_t now = millis();
    // The worker may legitimately block for a while on slow Bambuddy calls,
    // so it gets a much longer leash than the main loop.
    bool loopStalled = (now - hbLoop) > 60000;
    bool workerStalled = (now - hbWorker) > 180000;
    if (loopStalled || workerStalled) {
      Serial.printf("WATCHDOG: %s haengt -> Neustart\n",
                    loopStalled ? "Hauptschleife" : "Bambuddy-Task");
      Serial.flush();
      delay(200);
      ESP.restart();
    }
  }
}

static void workerTask(void *) {
  uint32_t lastPoll = 0;
  for (;;) {
    hbWorker = millis();
    if (!apMode && WiFi.status() == WL_CONNECTED && settings.hasApi() && settings.apiEnabled) {
      // Button presses first — that is what someone is standing there waiting for.
      for (int i = 0; i < STATION_COUNT; i++) {
        if (!pressPending[i]) continue;
        pressPending[i] = false;
        Station &st = stations[i];
        if (!st.active) continue;
        st.busy = true;
        ApiResult r = bambuddy.clearPlate(st.printerId);
        st.busy = false;
        recordClear(i, r);
        if (r.ok) {
          st.awaiting = false;
          lastPoll = 0;  // re-poll immediately so the LED reflects reality
        }
      }
      if (millis() - lastPoll >= settings.pollIntervalMs) {
        lastPoll = millis();
        for (int i = 0; i < STATION_COUNT; i++) {
          Station &st = stations[i];
          if (!st.active) continue;
          PrinterStatus ps;
          ApiResult r = bambuddy.getStatus(st.printerId, ps);
          diag.polls++;
          if (r.ok && ps.valid) {
            st.awaiting = ps.awaitingPlateClear;
            st.chamberLight = ps.chamberLight;
          } else {
            diag.errors++;
            lockedCopy(diag.lastError, sizeof(diag.lastError),
                       "Status Knopf " + String(i == 0 ? "A" : "B") + ": " + r.error);
          }
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ------------------------------------------------------------- web endpoints

static void sendJson(JsonDocument &doc, int code = 200) {
  String out;
  serializeJson(doc, out);
  server.send(code, "application/json", out);
}

static bool bodyJson(JsonDocument &doc) {
  if (!server.hasArg("plain")) return false;
  return deserializeJson(doc, server.arg("plain")) == DeserializationError::Ok;
}

static void handleRoot() { server.send_P(200, "text/html", PAGE_HTML); }

static void handleStatus() {
  JsonDocument doc;
  doc["version"] = FW_VERSION;
  doc["mode"] = apMode ? "setup" : "normal";
  doc["ip"] = apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  doc["hostname"] = settings.hostname;
  doc["ssid"] = settings.wifiSsid;
  doc["host"] = settings.host;
  doc["apiEnabled"] = settings.apiEnabled;
  doc["polls"] = diag.polls;
  doc["errors"] = diag.errors;
  xSemaphoreTake(mux, portMAX_DELAY);
  doc["lastError"] = String(diag.lastError);
  doc["lastClear"] = String(diag.lastClear);
  xSemaphoreGive(mux);
  JsonArray arr = doc["stations"].to<JsonArray>();
  for (int i = 0; i < STATION_COUNT; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["printerId"] = stations[i].printerId;
    o["awaiting"] = (bool)stations[i].awaiting;
    o["light"] = (bool)stations[i].chamberLight;
  }
  sendJson(doc);
}

static void handleScan() {
  JsonDocument doc;
  JsonArray arr = doc["networks"].to<JsonArray>();
  int n = WiFi.scanNetworks();
  String seen = "\n";
  for (int i = 0; i < n && i < 25; i++) {
    String s = WiFi.SSID(i);
    if (s.length() == 0) continue;
    String probe = s + "\n";
    if (seen.indexOf("\n" + probe) >= 0) continue;  // already listed
    seen += probe;
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = s;
    o["rssi"] = WiFi.RSSI(i);
  }
  WiFi.scanDelete();
  sendJson(doc);
}

static void handleWifiSave() {
  JsonDocument in;
  JsonDocument out;
  if (!bodyJson(in)) {
    out["ok"] = false;
    out["error"] = "Ungueltige Anfrage";
    sendJson(out, 400);
    return;
  }
  String ssid = in["ssid"] | "";
  if (ssid.length() == 0) {
    out["ok"] = false;
    out["error"] = "Kein WLAN-Name angegeben";
    sendJson(out, 400);
    return;
  }
  settings.wifiSsid = ssid;
  String pass = in["pass"] | "";
  if (pass.length() > 0) settings.wifiPass = pass;  // blank keeps the old one
  String hn = in["hostname"] | "";
  if (hn.length() > 0) settings.hostname = hn;
  settings.save();
  out["ok"] = true;
  sendJson(out);
  delay(400);
  ESP.restart();
}

static void handlePrinters() {
  JsonDocument in;
  JsonDocument out;
  if (bodyJson(in)) {
    String h = in["host"] | "";
    String k = in["key"] | "";
    if (h.length()) settings.host = h;
    if (k.length()) settings.apiKey = k;
  }
  JsonDocument list;
  ApiResult r = bambuddy.getPrinters(list);
  if (!r.ok) {
    out["ok"] = false;
    out["error"] = r.error;
    out["status"] = r.status;
    sendJson(out);
    return;
  }
  JsonArray src;
  if (list.is<JsonArray>()) src = list.as<JsonArray>();
  else if (list["printers"].is<JsonArray>()) src = list["printers"].as<JsonArray>();
  else if (list["results"].is<JsonArray>()) src = list["results"].as<JsonArray>();
  else if (list["items"].is<JsonArray>()) src = list["items"].as<JsonArray>();

  out["ok"] = true;
  JsonArray arr = out["printers"].to<JsonArray>();
  for (JsonVariant p : src) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = p["id"].as<int>();
    const char *name = p["friendly_name"].as<const char *>();
    if (!name) name = p["name"].as<const char *>();
    if (!name) name = p["display_name"].as<const char *>();
    o["name"] = name ? name : "Drucker";
  }
  sendJson(out);
}

static void applyStations() {
  for (int i = 0; i < STATION_COUNT; i++) {
    stations[i].printerId = settings.stations[i].printerId;
    stations[i].active = settings.stations[i].printerId > 0;
    if (!stations[i].active) {
      stations[i].awaiting = false;
      stations[i].chamberLight = false;
    }
  }
}

static void handleSave() {
  JsonDocument in;
  JsonDocument out;
  if (!bodyJson(in)) {
    out["ok"] = false;
    out["error"] = "Ungueltige Anfrage";
    sendJson(out, 400);
    return;
  }
  String h = in["host"] | "";
  String k = in["key"] | "";
  if (h.length()) settings.host = h;
  if (k.length()) settings.apiKey = k;
  if (!in["apiEnabled"].isNull()) settings.apiEnabled = in["apiEnabled"].as<bool>();
  settings.stations[0].printerId = in["p0"] | 0;
  settings.stations[1].printerId = in["p1"] | 0;
  settings.save();
  applyStations();
  out["ok"] = true;
  sendJson(out);
}

static int argIndex() {
  int i = server.hasArg("i") ? server.arg("i").toInt() : -1;
  return (i >= 0 && i < STATION_COUNT) ? i : -1;
}

static void handleIdentify() {
  JsonDocument out;
  int i = argIndex();
  if (i < 0) {
    out["ok"] = false;
    out["error"] = "Ungueltiger Knopf";
    sendJson(out, 400);
    return;
  }
  stations[i].identify(4000);
  out["ok"] = true;
  sendJson(out);
}

// Fires the exact same call a real button press makes, and reports the raw
// result — this is the tool that makes a failing clear-plate diagnosable.
static void handleTestClear() {
  JsonDocument out;
  int i = argIndex();
  if (i < 0) {
    out["ok"] = false;
    out["error"] = "Ungueltiger Knopf";
    sendJson(out, 400);
    return;
  }
  if (!stations[i].active) {
    out["ok"] = false;
    out["status"] = 0;
    out["ms"] = 0;
    out["error"] = "Diesem Knopf ist kein Drucker zugeordnet";
    sendJson(out);
    return;
  }
  stations[i].busy = true;
  ApiResult r = bambuddy.clearPlate(stations[i].printerId);
  stations[i].busy = false;
  recordClear(i, r);
  out["ok"] = r.ok;
  out["status"] = r.status;
  out["ms"] = r.ms;
  out["error"] = r.error;
  out["body"] = r.body;
  sendJson(out);
}

static void handleReboot() {
  JsonDocument out;
  out["ok"] = true;
  sendJson(out);
  delay(300);
  ESP.restart();
}

static void handleOtaDone() {
  JsonDocument out;
  out["ok"] = !Update.hasError();
  if (Update.hasError()) out["error"] = "Update fehlgeschlagen";
  server.sendHeader("Connection", "close");
  sendJson(out);
  delay(600);
  ESP.restart();
}

static void handleOtaUpload() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    Serial.printf("OTA start: %s\n", up.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_END) {
    if (Update.end(true)) Serial.printf("OTA fertig: %u Bytes\n", (unsigned)up.totalSize);
    else Update.printError(Serial);
  }
}

static void setupRoutes() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/wifi", HTTP_POST, handleWifiSave);
  server.on("/api/printers", HTTP_POST, handlePrinters);
  server.on("/api/save", HTTP_POST, handleSave);
  server.on("/api/identify", HTTP_POST, handleIdentify);
  server.on("/api/testclear", HTTP_POST, handleTestClear);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/ota", HTTP_POST, handleOtaDone, handleOtaUpload);
  server.onNotFound(handleRoot);  // captive portal: anything shows the page
}

// -------------------------------------------------------------------- sketch

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nBambutton " FW_VERSION);

  mux = xSemaphoreCreateMutex();
  settings.load();

  for (int i = 0; i < STATION_COUNT; i++) {
    stations[i].begin(i, settings.stations[i].ledPin, settings.stations[i].buttonPin,
                      settings.stations[i].printerId);
  }

  // Holding button A during boot forces the setup portal.
  delay(60);
  bool forceSetup = digitalRead(settings.stations[0].buttonPin) == HIGH;
  if (forceSetup) Serial.println("Config-Taste gehalten -> Setup-Modus");

  bool connected = false;
  if (!forceSetup && settings.hasWifi()) connected = connectWifi();
  if (!connected) startAp();

  setupRoutes();
  server.begin();
  Serial.printf("Weboberflaeche: http://%s/\n",
                (apMode ? WiFi.softAPIP() : WiFi.localIP()).toString().c_str());

  hbLoop = hbWorker = millis();
  // 12 KB: the 8 KB original was tight once Bambuddy responses got parsed.
  if (!apMode) xTaskCreate(workerTask, "bambuddy", 12288, nullptr, 1, nullptr);
  xTaskCreate(watchdogTask, "watchdog", 2560, nullptr, 3, nullptr);
}

void loop() {
  server.handleClient();
  if (apMode) dnsServer.processNextRequest();

  uint32_t now = millis();
  hbLoop = now;

  // Periodic health line: a falling heap points at a leak, a falling RSSI at
  // reception. Both are the usual suspects behind "it just disappears".
  static uint32_t lastLog = 0;
  if (now - lastLog > 30000) {
    lastLog = now;
    Serial.printf("[%lus] Heap %u B (min %u) | RSSI %d dBm | WiFi %d | Abfragen %u, Fehler %u\n",
                  (unsigned long)(now / 1000), (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getMinFreeHeap(), (int)WiFi.RSSI(),
                  (int)WiFi.status(), (unsigned)diag.polls, (unsigned)diag.errors);
  }

  for (int i = 0; i < STATION_COUNT; i++) {
    stations[i].pollButton();
    if (stations[i].consumePress()) pressPending[i] = true;
    stations[i].updateLed(now);
  }

  // Keep the station alive if the access point drops out.
  static uint32_t lastCheck = 0;
  if (!apMode && now - lastCheck > 15000) {
    lastCheck = now;
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WLAN weg — verbinde neu");
      WiFi.disconnect();
      WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPass.c_str());
      WiFi.setSleep(false);
    }
  }
  delay(2);
}
