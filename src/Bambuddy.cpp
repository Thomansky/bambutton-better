#include "Bambuddy.h"
#include "Settings.h"
#include <HTTPClient.h>
#include <WiFi.h>

Bambuddy bambuddy;

ApiResult Bambuddy::request(bool post, const String &path, String *bodyOut) {
  ApiResult r;
  uint32_t t0 = millis();

  if (!settings.hasApi()) {
    r.error = "Bambuddy ist nicht konfiguriert";
    return r;
  }
  if (WiFi.status() != WL_CONNECTED) {
    r.error = "Kein WLAN";
    return r;
  }

  WiFiClient client;
  HTTPClient http;
  String url = settings.baseUrl() + path;

  http.setConnectTimeout(6000);
  http.setTimeout(settings.httpTimeoutMs);
  http.setReuse(false);

  if (!http.begin(client, url)) {
    r.error = "Ungueltige URL: " + url;
    r.ms = millis() - t0;
    return r;
  }
  http.addHeader("X-API-Key", settings.apiKey);
  http.addHeader("Accept", "application/json");

  // clear-plate takes no request body. Send an explicit zero-length body so
  // Content-Length: 0 is present and the server never waits for data.
  int code = post ? http.POST(String("")) : http.GET();
  r.status = code;

  if (code > 0) {
    String b = http.getString();
    if (bodyOut) *bodyOut = b;
    r.body = b.length() > 300 ? b.substring(0, 300) : b;
    r.ok = (code >= 200 && code < 300);
    if (!r.ok) r.error = "HTTP " + String(code) + " " + r.body;
  } else {
    r.error = String("Verbindungsfehler: ") + HTTPClient::errorToString(code);
  }

  http.end();
  r.ms = millis() - t0;
  return r;
}

ApiResult Bambuddy::getPrinters(JsonDocument &doc) {
  String body;
  ApiResult r = request(false, "/printers/", &body);
  if (!r.ok) return r;
  DeserializationError e = deserializeJson(doc, body);
  if (e) {
    r.ok = false;
    r.error = String("JSON-Fehler: ") + e.c_str();
  }
  return r;
}

ApiResult Bambuddy::getStatus(int printerId, PrinterStatus &out) {
  String body;
  ApiResult r = request(false, "/printers/" + String(printerId) + "/status", &body);
  if (!r.ok) return r;
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, body);
  if (e) {
    r.ok = false;
    r.error = String("JSON-Fehler: ") + e.c_str();
    return r;
  }
  out.awaitingPlateClear = doc["awaiting_plate_clear"].as<bool>();
  out.chamberLight = doc["chamber_light"].as<bool>();
  out.valid = true;
  return r;
}

ApiResult Bambuddy::clearPlate(int printerId) {
  return request(true, "/printers/" + String(printerId) + "/clear-plate", nullptr);
}
