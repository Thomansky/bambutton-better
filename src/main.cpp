// Bambutton — physical plate-clear button for Bambuddy (ESP32-C3)
//
// Design notes:
//  * Wi-Fi lives in Net (src/Net.cpp): station mode is retried forever, the
//    setup network only appears while there is no connection and closes
//    again by itself, and the portal tests credentials live before storing
//    them. Tx power is capped because the ESP32-C3 Super Mini's antenna
//    cannot cope with full power.
//  * All Bambuddy traffic runs in a worker task, so the web UI, the captive
//    portal and the buttons stay responsive even while a slow call is in
//    flight. UI-triggered calls (connection test, "press now") are queued to
//    that worker as jobs and polled by the page.
//  * Every failure is kept in memory and shown in the web UI instead of only
//    being printed to a serial console nobody is watching.
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <ArduinoJson.h>

#include "Settings.h"
#include "Net.h"
#include "Bambuddy.h"
#include "Station.h"
#include "page.h"

#define FW_VERSION "2.0.0"

WebServer server(80);
Station stations[STATION_COUNT];

// A press is handed to the worker with its timestamp. If it cannot be
// delivered within a few seconds it is dropped (with a visible "failed"
// flicker) instead of firing minutes later when the link comes back.
static volatile uint32_t pressAt[STATION_COUNT] = {0, 0};
static const uint32_t PRESS_MAX_AGE_MS = 10000;

// Heartbeats: both the main loop and the worker stamp these. A separate
// watchdog task reboots the board if either stops moving — without this the
// board stays dead until someone unplugs it.
static volatile uint32_t hbLoop = 0;
static volatile uint32_t hbWorker = 0;
static SemaphoreHandle_t mux = nullptr;
static volatile uint32_t rebootAt = 0;      // deferred so the HTTP response gets out first
static volatile uint32_t backoffUntil = 0;  // after HTTP 429: poll slower for a while

struct StationDiag {
  char lastError[160];
  char state[16];
  uint32_t lastOkAt;
  uint32_t fails;
};
struct Diag {
  char lastError[200];
  char lastClear[200];
  uint32_t polls;
  uint32_t errors;
  uint32_t lastOkAt;
  StationDiag st[STATION_COUNT];
};
static Diag diag = {};

static void lockedCopy(char *dst, size_t n, const String &s) {
  xSemaphoreTake(mux, portMAX_DELAY);
  strncpy(dst, s.c_str(), n - 1);
  dst[n - 1] = 0;
  xSemaphoreGive(mux);
}

// UI-triggered Bambuddy calls run on the worker as jobs so the web server,
// buttons and captive portal never block for the duration of an HTTP call.
enum JobKind : uint8_t { JOB_NONE = 0, JOB_PRINTERS, JOB_TESTCLEAR };
struct Job {
  volatile uint8_t kind = JOB_NONE;
  volatile bool done = false;
  uint32_t id = 0;
  int station = 0;
  String host, key;
  ApiResult result;
  String printersJson;
};
static Job job;
static uint32_t jobSeq = 0;

static const char *stationName(int i) { return i == 0 ? "A" : "B"; }

// ------------------------------------------------------------- worker thread

static void recordClear(int idx, const ApiResult &r) {
  String s = "Knopf " + String(stationName(idx)) + ": ";
  if (r.ok) s += "OK, HTTP " + String(r.status) + " nach " + String(r.ms) + " ms";
  else if (r.notAwaiting) s += "nichts zu raeumen (HTTP 400) nach " + String(r.ms) + " ms";
  else s += "FEHLER nach " + String(r.ms) + " ms: " + r.error;
  lockedCopy(diag.lastClear, sizeof(diag.lastClear), s);
  if (!r.ok && !r.notAwaiting) lockedCopy(diag.lastError, sizeof(diag.lastError), s);
  Serial.println("[bambuddy] " + s);
}

static void pollStation(int i) {
  Station &st = stations[i];
  PrinterStatus ps;
  ApiResult r = bambuddy.getStatus(st.printerId, ps);
  diag.polls++;
  if (r.ok && ps.valid) {
    st.awaiting = ps.awaitingPlateClear;
    st.chamberLight = ps.chamberLight;
    st.online = ps.connected;
    st.noLink = false;
    diag.st[i].fails = 0;
    diag.st[i].lastOkAt = millis();
    diag.lastOkAt = millis();
    lockedCopy(diag.st[i].state, sizeof(diag.st[i].state), ps.state);
    lockedCopy(diag.st[i].lastError, sizeof(diag.st[i].lastError), "");
  } else {
    diag.errors++;
    diag.st[i].fails++;
    if (r.status == 429) backoffUntil = millis() + 120000;
    lockedCopy(diag.st[i].lastError, sizeof(diag.st[i].lastError), r.error);
    lockedCopy(diag.lastError, sizeof(diag.lastError),
               "Status Knopf " + String(stationName(i)) + ": " + r.error);
    if (diag.st[i].fails >= 3) st.noLink = true;
    Serial.printf("[bambuddy] Status Knopf %s: %s\n", stationName(i), r.error.c_str());
  }
}

static void runJob() {
  if (job.kind == JOB_PRINTERS) {
    JsonDocument list;
    ApiResult r = bambuddy.getPrinters(list, job.host, job.key);
    String out = "[";
    if (r.ok) {
      JsonArray src;
      if (list.is<JsonArray>()) src = list.as<JsonArray>();
      else if (list["printers"].is<JsonArray>()) src = list["printers"].as<JsonArray>();
      else if (list["results"].is<JsonArray>()) src = list["results"].as<JsonArray>();
      else if (list["items"].is<JsonArray>()) src = list["items"].as<JsonArray>();
      JsonDocument arr;
      JsonArray a = arr.to<JsonArray>();
      for (JsonVariant p : src) {
        JsonObject o = a.add<JsonObject>();
        o["id"] = p["id"].as<int>();
        const char *name = p["friendly_name"].as<const char *>();
        if (!name) name = p["name"].as<const char *>();
        if (!name) name = p["display_name"].as<const char *>();
        o["name"] = name ? name : "Drucker";
      }
      out = "";
      serializeJson(arr, out);
    }
    xSemaphoreTake(mux, portMAX_DELAY);
    job.result = r;
    job.printersJson = r.ok ? out : "[]";
    xSemaphoreGive(mux);
  } else if (job.kind == JOB_TESTCLEAR) {
    Station &st = stations[job.station];
    st.busy = true;
    ApiResult r = bambuddy.clearPlate(st.printerId);
    st.busy = false;
    recordClear(job.station, r);
    st.feedback(r.ok);
    if (r.ok) st.awaiting = false;
    xSemaphoreTake(mux, portMAX_DELAY);
    job.result = r;
    xSemaphoreGive(mux);
  }
  job.done = true;
}

// A stalled task must never mean "dead until power-cycled".
static void watchdogTask(void *) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(5000));
    uint32_t now = millis();
    // The worker may legitimately block for a while on slow Bambuddy calls,
    // so it gets a much longer leash than the main loop.
    bool loopStalled = (now - hbLoop) > 120000;
    bool workerStalled = (now - hbWorker) > 240000;
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
    if (job.kind != JOB_NONE && !job.done) runJob();

    bool link = net.staConnected();
    settings.lock();
    bool configured = settings.hasApi();
    bool enabled = settings.apiEnabled;
    uint32_t interval = settings.pollIntervalMs;
    settings.unlock();
    bool canTalk = link && configured && enabled;

    // Button presses first — that is what someone is standing there waiting for.
    for (int i = 0; i < STATION_COUNT; i++) {
      uint32_t at = pressAt[i];
      if (!at) continue;
      pressAt[i] = 0;
      Station &st = stations[i];
      if (!st.active) continue;
      if (millis() - at > PRESS_MAX_AGE_MS) {
        st.feedback(false);
        continue;
      }
      if (!canTalk) {
        st.feedback(false);
        ApiResult r;
        r.error = !link ? "kein WLAN" : (!configured ? "Bambuddy nicht konfiguriert" : "Bambuddy-Abfrage abgeschaltet");
        recordClear(i, r);
        continue;
      }
      st.busy = true;
      ApiResult r = bambuddy.clearPlate(st.printerId);
      st.busy = false;
      hbWorker = millis();
      recordClear(i, r);
      st.feedback(r.ok);
      if (r.ok) {
        st.awaiting = false;
        lastPoll = 0;  // re-poll immediately so the LED reflects reality
      }
    }

    if (canTalk) {
      uint32_t iv = interval;
      if ((int32_t)(backoffUntil - millis()) > 0 && iv < 15000) iv = 15000;
      if (millis() - lastPoll >= iv) {
        lastPoll = millis();
        for (int i = 0; i < STATION_COUNT; i++) {
          if (!stations[i].active) continue;
          if (pressAt[0] || pressAt[1]) break;  // a press is waiting: serve it first
          pollStation(i);
          hbWorker = millis();
        }
      }
    } else {
      for (int i = 0; i < STATION_COUNT; i++) {
        // Deliberately switched off: plain idle LED. Otherwise: "no link".
        stations[i].noLink = !(configured && !enabled);
        stations[i].awaiting = false;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ------------------------------------------------------------- web endpoints

static void sendJson(JsonDocument &doc, int code = 200) {
  String out;
  serializeJson(doc, out);
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", out);
}

static void sendError(const String &msg, int code = 400) {
  JsonDocument out;
  out["ok"] = false;
  out["error"] = msg;
  sendJson(out, code);
}

static bool bodyJson(JsonDocument &doc) {
  if (!server.hasArg("plain")) return false;
  return deserializeJson(doc, server.arg("plain")) == DeserializationError::Ok;
}

static void scheduleReboot(uint32_t inMs) {
  uint32_t t = millis() + inMs;
  rebootAt = t ? t : 1;
}

// Captive portal: a phone on the setup network probing some other host name
// (connectivitycheck.gstatic.com, captive.apple.com, msftconnecttest.com …)
// is redirected to the portal address, which makes the OS pop the page up.
static bool captiveRedirect() {
  if (!net.apActive()) return false;
  if (!net.isApClient(server.client().remoteIP())) return false;
  String host = server.hostHeader();
  String ap = net.apIp().toString();
  if (host == ap || host == ap + ":80") return false;
  server.sendHeader("Location", "http://" + ap + "/", true);
  server.sendHeader("Cache-Control", "no-store");
  server.send(302, "text/plain", "");
  return true;
}

static void sendPage() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html", PAGE_HTML);
}

static void handleRoot() {
  if (captiveRedirect()) return;
  sendPage();
}

static void handleNotFound() {
  if (captiveRedirect()) return;
  if (net.apActive() && net.isApClient(server.client().remoteIP())) {
    sendPage();  // anything a captive-portal browser asks for shows the page
    return;
  }
  server.send(404, "text/plain", "Not found");
}

static void handleStatus() {
  JsonDocument doc;
  doc["version"] = FW_VERSION;
  doc["uptime"] = millis() / 1000;
  doc["heap"] = ESP.getFreeHeap();
  JsonObject n = doc["net"].to<JsonObject>();
  net.fillStatus(n);
  settings.lock();
  doc["host"] = settings.host;
  doc["hasKey"] = settings.apiKey.length() > 0;
  doc["apiEnabled"] = settings.apiEnabled;
  doc["pollMs"] = settings.pollIntervalMs;
  doc["idleLed"] = settings.idleLed;
  doc["apPassSet"] = settings.apPass.length() > 0;
  settings.unlock();
  doc["polls"] = diag.polls;
  doc["errors"] = diag.errors;
  doc["lastOkAgo"] = diag.lastOkAt ? (int)((millis() - diag.lastOkAt) / 1000) : -1;
  xSemaphoreTake(mux, portMAX_DELAY);
  doc["lastError"] = String(diag.lastError);
  doc["lastClear"] = String(diag.lastClear);
  JsonArray arr = doc["stations"].to<JsonArray>();
  for (int i = 0; i < STATION_COUNT; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["printerId"] = stations[i].printerId;
    o["awaiting"] = (bool)stations[i].awaiting;
    o["light"] = (bool)stations[i].chamberLight;
    o["online"] = (bool)stations[i].online;
    o["noLink"] = (bool)stations[i].noLink;
    o["state"] = String(diag.st[i].state);
    o["error"] = String(diag.st[i].lastError);
    o["fails"] = diag.st[i].fails;
    o["okAgo"] = diag.st[i].lastOkAt ? (int)((millis() - diag.st[i].lastOkAt) / 1000) : -1;
  }
  xSemaphoreGive(mux);
  sendJson(doc);
}

static void handleWifiState() {
  JsonDocument doc;
  JsonObject n = doc.to<JsonObject>();
  net.fillStatus(n);
  sendJson(doc);
}

static void handleScan() {
  bool fresh = server.hasArg("fresh");
  if (fresh || (!net.hasScan() && !net.scanning())) net.startScan();
  JsonDocument doc;
  doc["scanning"] = net.scanning();
  doc["failed"] = net.scanFailed() && !net.scanning();
  JsonArray arr = doc["networks"].to<JsonArray>();
  if (!net.scanning()) net.fillScan(arr);
  sendJson(doc);
}

// Reads ssid/pass/hostname from the body, validates, stores the hostname.
static bool readWifiBody(String &ssid, String &pass) {
  JsonDocument in;
  if (!bodyJson(in)) {
    sendError("Ungueltige Anfrage");
    return false;
  }
  ssid = in["ssid"] | "";
  ssid.trim();
  pass = in["pass"] | "";
  String hn = in["hostname"] | "";
  if (ssid.length() == 0) {
    sendError("Kein WLAN-Name angegeben");
    return false;
  }
  if (ssid.length() > 32) {
    sendError("WLAN-Name zu lang (max. 32 Zeichen)");
    return false;
  }
  settings.lock();
  // Same network, empty password field: keep the stored password.
  if (pass.length() == 0 && ssid == settings.wifiSsid) pass = settings.wifiPass;
  if (hn.length()) {
    String clean = Settings::cleanHostname(hn);
    if (clean != settings.hostname) {
      settings.hostname = clean;
      settings.save();
      net.renameMdns();
    }
  }
  settings.unlock();
  if (pass.length() > 0 && pass.length() < 8) {
    sendError("Das WLAN-Passwort hat mindestens 8 Zeichen (leer lassen = offenes Netz)");
    return false;
  }
  if (pass.length() > 63) {
    sendError("WLAN-Passwort zu lang (max. 63 Zeichen)");
    return false;
  }
  return true;
}

static void handleWifiConnect() {
  String ssid, pass;
  if (!readWifiBody(ssid, pass)) return;
  if (!net.startTest(ssid, pass)) {
    sendError("Es laeuft bereits ein Verbindungsversuch — bitte warten");
    return;
  }
  JsonDocument out;
  out["ok"] = true;
  sendJson(out);
}

static void handleWifiSave() {
  String ssid, pass;
  if (!readWifiBody(ssid, pass)) return;
  settings.lock();
  settings.wifiSsid = ssid;
  settings.wifiPass = pass;
  settings.save();
  settings.unlock();
  JsonDocument out;
  out["ok"] = true;
  sendJson(out);
  scheduleReboot(800);
}

static void handleWifiFinish() {
  if (!net.closePortal()) {
    sendError("Das Setup-Netz schliesst erst, wenn die WLAN-Verbindung steht");
    return;
  }
  JsonDocument out;
  out["ok"] = true;
  sendJson(out);
}

static void handleWifiPortal() {
  net.openPortal(10UL * 60UL * 1000UL);
  JsonDocument out;
  out["ok"] = true;
  out["apSsid"] = AP_SSID;
  sendJson(out);
}

static void handleWifiForget() {
  net.forgetWifi();
  JsonDocument out;
  out["ok"] = true;
  sendJson(out);
  scheduleReboot(800);
}

static bool startJob(uint8_t kind, int station, const String &host, const String &key, uint32_t &idOut) {
  if (job.kind != JOB_NONE && !job.done) return false;
  job.kind = JOB_NONE;
  job.done = false;
  job.station = station;
  job.host = host;
  job.key = key;
  job.id = ++jobSeq;
  idOut = job.id;
  job.kind = kind;  // last: the worker picks it up from here
  return true;
}

static void handlePrinters() {
  JsonDocument in;
  String h, k;
  if (bodyJson(in)) {
    h = in["host"] | "";
    k = in["key"] | "";
  }
  h.trim();
  settings.lock();
  if (h.length() == 0) h = settings.host;
  if (k.length() == 0) k = settings.apiKey;
  settings.unlock();
  if (h.length() == 0 || k.length() == 0) {
    sendError("Bitte Bambuddy-Adresse und API-Key eintragen");
    return;
  }
  if (h.startsWith("https://")) {
    sendError("https wird nicht unterstuetzt — bitte die Adresse als IP:Port (http) angeben");
    return;
  }
  if (!net.staConnected()) {
    sendError("Kein WLAN — Bambuddy kann erst nach der WLAN-Verbindung erreicht werden");
    return;
  }
  uint32_t id;
  if (!startJob(JOB_PRINTERS, 0, h, k, id)) {
    sendError("Bitte warten — eine andere Anfrage laeuft noch");
    return;
  }
  JsonDocument out;
  out["ok"] = true;
  out["job"] = id;
  sendJson(out);
}

static void handleJob() {
  uint32_t id = server.hasArg("id") ? (uint32_t)server.arg("id").toInt() : 0;
  JsonDocument out;
  if (id != job.id || job.kind == JOB_NONE) {
    out["ok"] = false;
    out["error"] = "Unbekannte Anfrage";
    sendJson(out);
    return;
  }
  if (!job.done) {
    out["pending"] = true;
    sendJson(out);
    return;
  }
  xSemaphoreTake(mux, portMAX_DELAY);
  out["ok"] = job.result.ok;
  out["status"] = job.result.status;
  out["ms"] = job.result.ms;
  out["error"] = job.result.error;
  out["notAwaiting"] = job.result.notAwaiting;
  if (job.kind == JOB_PRINTERS) {
    JsonDocument p;
    deserializeJson(p, job.printersJson);
    out["printers"] = p.as<JsonArray>();
  } else {
    out["body"] = job.result.body;
  }
  xSemaphoreGive(mux);
  sendJson(out);
}

static void applyStations() {
  for (int i = 0; i < STATION_COUNT; i++) {
    stations[i].printerId = settings.stations[i].printerId;
    stations[i].active = settings.stations[i].printerId > 0;
    stations[i].idleMode = settings.idleLed;
    if (!stations[i].active) {
      stations[i].awaiting = false;
      stations[i].chamberLight = false;
    }
  }
}

static void handleSave() {
  JsonDocument in;
  if (!bodyJson(in)) {
    sendError("Ungueltige Anfrage");
    return;
  }
  String h = in["host"] | "";
  h.trim();
  if (h.startsWith("https://")) {
    sendError("https wird nicht unterstuetzt — bitte die Adresse als IP:Port (http) angeben");
    return;
  }
  String k = in["key"] | "";
  String ap = in["apPass"] | "";
  if (!in["apPass"].isNull() && ap.length() > 0 && (ap.length() < 8 || ap.length() > 63)) {
    sendError("Das Setup-Netz-Passwort hat 8 bis 63 Zeichen (leer = offenes Setup-Netz)");
    return;
  }
  int tx = in["txPower"] | -1;
  if (tx >= 0 && !Settings::validTxPower(tx)) {
    sendError("Ungueltige Sendeleistung");
    return;
  }

  settings.lock();
  if (h.length()) settings.host = h;
  if (k.length()) settings.apiKey = k;
  if (!in["apiEnabled"].isNull()) settings.apiEnabled = in["apiEnabled"].as<bool>();
  if (!in["p0"].isNull()) settings.stations[0].printerId = in["p0"] | 0;
  if (!in["p1"].isNull()) settings.stations[1].printerId = in["p1"] | 0;
  if (!in["pollMs"].isNull()) {
    uint32_t p = in["pollMs"] | 3000;
    settings.pollIntervalMs = p < 1500 ? 1500 : (p > 60000 ? 60000 : p);
  }
  if (!in["idleLed"].isNull()) {
    int m = in["idleLed"] | 0;
    settings.idleLed = (m < 0 || m > IDLE_OFF) ? IDLE_FOLLOW_LIGHT : (uint8_t)m;
  }
  if (tx >= 0) settings.txPower = (int8_t)tx;
  if (!in["apPass"].isNull()) settings.apPass = ap;
  settings.save();
  settings.unlock();

  applyStations();
  net.applyTxPower();
  backoffUntil = 0;
  JsonDocument out;
  out["ok"] = true;
  sendJson(out);
}

static int argIndex() {
  int i = server.hasArg("i") ? server.arg("i").toInt() : -1;
  return (i >= 0 && i < STATION_COUNT) ? i : -1;
}

static void handleIdentify() {
  int i = argIndex();
  if (i < 0) {
    sendError("Ungueltiger Knopf");
    return;
  }
  stations[i].identify(4000);
  JsonDocument out;
  out["ok"] = true;
  sendJson(out);
}

// Fires the exact same call a real button press makes, via the worker, and
// reports the raw result — this is what makes a failing clear-plate diagnosable.
static void handleTestClear() {
  int i = argIndex();
  if (i < 0) {
    sendError("Ungueltiger Knopf");
    return;
  }
  if (!stations[i].active) {
    sendError("Diesem Knopf ist kein Drucker zugeordnet");
    return;
  }
  if (!net.staConnected()) {
    sendError("Kein WLAN");
    return;
  }
  settings.lock();
  bool configured = settings.hasApi();
  settings.unlock();
  if (!configured) {
    sendError("Bambuddy ist nicht konfiguriert");
    return;
  }
  uint32_t id;
  if (!startJob(JOB_TESTCLEAR, i, "", "", id)) {
    sendError("Bitte warten — eine andere Anfrage laeuft noch");
    return;
  }
  JsonDocument out;
  out["ok"] = true;
  out["job"] = id;
  sendJson(out);
}

static void handleReboot() {
  JsonDocument out;
  out["ok"] = true;
  sendJson(out);
  scheduleReboot(500);
}

static String otaError;

static void handleOtaDone() {
  JsonDocument out;
  bool ok = otaError.length() == 0 && !Update.hasError();
  out["ok"] = ok;
  if (!ok) out["error"] = otaError.length() ? otaError : String("Update fehlgeschlagen: ") + Update.errorString();
  server.sendHeader("Connection", "close");
  sendJson(out);
  if (ok) scheduleReboot(800);
}

static void handleOtaUpload() {
  HTTPUpload &up = server.upload();
  hbLoop = millis();  // a slow upload must not look like a stalled loop
  if (up.status == UPLOAD_FILE_START) {
    otaError = "";
    Serial.printf("OTA start: %s\n", up.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) otaError = String("Update kann nicht starten: ") + Update.errorString();
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (otaError.length()) return;
    if (up.totalSize == 0 && up.currentSize > 0 && up.buf[0] != 0xE9) {
      otaError = "Das ist kein ESP32-Firmware-Image (firmware.bin erwartet)";
      Update.abort();
      return;
    }
    if (Update.write(up.buf, up.currentSize) != up.currentSize) {
      otaError = String("Schreibfehler: ") + Update.errorString();
      Update.abort();
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (otaError.length()) return;
    if (up.totalSize < 262144) {
      otaError = "Datei zu klein fuer eine Firmware — bootloader.bin/partitions.bin gehoeren nicht hierher";
      Update.abort();
      return;
    }
    if (Update.end(true)) Serial.printf("OTA fertig: %u Bytes\n", (unsigned)up.totalSize);
    else otaError = String("Update unvollstaendig: ") + Update.errorString();
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    otaError = "Upload abgebrochen";
    Update.abort();
  }
}

static void setupRoutes() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/wifi/state", HTTP_GET, handleWifiState);
  server.on("/api/wifi/connect", HTTP_POST, handleWifiConnect);
  server.on("/api/wifi/save", HTTP_POST, handleWifiSave);
  server.on("/api/wifi/finish", HTTP_POST, handleWifiFinish);
  server.on("/api/wifi/portal", HTTP_POST, handleWifiPortal);
  server.on("/api/wifi/forget", HTTP_POST, handleWifiForget);
  server.on("/api/printers", HTTP_POST, handlePrinters);
  server.on("/api/job", HTTP_GET, handleJob);
  server.on("/api/save", HTTP_POST, handleSave);
  server.on("/api/identify", HTTP_POST, handleIdentify);
  server.on("/api/testclear", HTTP_POST, handleTestClear);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/ota", HTTP_POST, handleOtaDone, handleOtaUpload);
  server.onNotFound(handleNotFound);
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
    stations[i].idleMode = settings.idleLed;
    stations[i].noLink = true;  // until the first successful poll
  }

  // Holding button A during boot opens the setup network (the stored
  // network is still tried in the background).
  int high = 0;
  for (int i = 0; i < 6; i++) {
    if (digitalRead(settings.stations[0].buttonPin) == HIGH) high++;
    delay(15);
  }
  bool forceSetup = high >= 5;
  if (forceSetup) Serial.println("Config-Taste gehalten -> Setup-Netz");

  net.begin(forceSetup);

  setupRoutes();
  server.begin();

  hbLoop = hbWorker = millis();
  // 12 KB: the 8 KB original was tight once Bambuddy responses got parsed.
  xTaskCreate(workerTask, "bambuddy", 12288, nullptr, 1, nullptr);
  xTaskCreate(watchdogTask, "watchdog", 2560, nullptr, 3, nullptr);
}

void loop() {
  server.handleClient();
  net.loop();

  uint32_t now = millis();
  hbLoop = now;

  // Periodic health line: a falling heap points at a leak, a falling RSSI at
  // reception. Both are the usual suspects behind "it just disappears".
  static uint32_t lastLog = 0;
  if (now - lastLog > 30000) {
    lastLog = now;
    Serial.printf("[%lus] Heap %u B (min %u) | WLAN %s RSSI %d dBm (%s) | Setup-Netz %s (%d) | Abfragen %u, Fehler %u\n",
                  (unsigned long)(now / 1000), (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getMinFreeHeap(), net.staConnected() ? "ok" : "AUS",
                  net.rssi(), net.rssiText().c_str(), net.apActive() ? "an" : "aus",
                  net.apClients(), (unsigned)diag.polls, (unsigned)diag.errors);
  }

  for (int i = 0; i < STATION_COUNT; i++) {
    stations[i].pollButton();
    if (stations[i].consumePress()) pressAt[i] = now ? now : 1;
    stations[i].updateLed(now);
  }

  if (rebootAt && (int32_t)(now - rebootAt) > 0) {
    Serial.println("Neustart");
    Serial.flush();
    ESP.restart();
  }
  delay(2);
}
