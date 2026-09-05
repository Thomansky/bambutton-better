#include "Bambuddy.h"
#include "Settings.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <ESPmDNS.h>

Bambuddy bambuddy;

// "bambuddy.local:8000" -> "192.168.1.50:8000". Plain LwIP DNS cannot resolve
// .local names; they need an mDNS query. Resolved addresses are cached.
String Bambuddy::resolveHost(const String &hostPort) {
  String h = hostPort;
  String port;
  int colon = h.lastIndexOf(':');
  if (colon > 0) {
    port = h.substring(colon);
    h = h.substring(0, colon);
  }
  if (!h.endsWith(".local")) return hostPort;
  String name = h.substring(0, h.length() - 6);
  if (name == _mdnsName && _mdnsIp.length() && millis() - _mdnsAt < 300000) return _mdnsIp + port;
  IPAddress ip = MDNS.queryHost(name, 3000);
  if ((uint32_t)ip == 0) return hostPort;  // let the HTTP layer report the failure
  _mdnsName = name;
  _mdnsIp = ip.toString();
  _mdnsAt = millis();
  return _mdnsIp + port;
}

static String detailFrom(const String &body) {
  // Bambuddy answers errors as {"detail": "..."}; show that instead of raw JSON.
  JsonDocument d;
  if (deserializeJson(d, body) == DeserializationError::Ok) {
    const char *s = d["detail"].as<const char *>();
    if (!s) s = d["error"].as<const char *>();
    if (!s) s = d["message"].as<const char *>();
    if (s) return String(s);
  }
  return body;
}

ApiResult Bambuddy::request(bool post, const String &path, String *bodyOut,
                            const String &host, const String &key, uint32_t timeoutMs) {
  ApiResult r;
  uint32_t t0 = millis();

  if (host.length() == 0 || key.length() == 0) {
    r.error = "Bambuddy ist nicht konfiguriert (Adresse und API-Key eintragen)";
    return r;
  }
  if (WiFi.status() != WL_CONNECTED) {
    r.error = "Kein WLAN";
    return r;
  }

  String base = Settings::baseUrlFor(host);           // http://host:port/api/v1
  String hostPort = base.substring(7, base.length() - 7);
  String url = "http://" + resolveHost(hostPort) + "/api/v1" + path;

  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(timeoutMs);
  http.setReuse(false);
  http.setUserAgent("Bambutton/2.0");
  // Bambuddy defines some routes with and some without a trailing slash;
  // follow the 307 instead of failing on it.
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (!http.begin(client, url)) {
    r.error = "Ungueltige Adresse: " + url;
    r.ms = millis() - t0;
    return r;
  }
  http.addHeader("X-API-Key", key);
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
    if (!r.ok) {
      String detail = detailFrom(r.body);
      switch (code) {
        case 401:
        case 403:
          r.error = "API-Key abgelehnt (HTTP " + String(code) +
                    "). Key pruefen; er braucht die Rechte printers:read und printers:clear_plate.";
          break;
        case 404:
          r.error = "Nicht gefunden (HTTP 404): Adresse oder Drucker-ID pruefen. " + detail;
          break;
        case 429:
          r.error = "Bambuddy drosselt die Anfragen (HTTP 429): Abfrageintervall erhoehen.";
          break;
        default:
          if (code >= 500) r.error = "Bambuddy-Serverfehler (HTTP " + String(code) + "): " + detail;
          else r.error = "HTTP " + String(code) + ": " + detail;
      }
    }
  } else {
    String why = HTTPClient::errorToString(code);
    if (code == HTTPC_ERROR_CONNECTION_REFUSED)
      r.error = "Keine Verbindung zu " + hostPort + " (abgelehnt oder nicht erreichbar): Adresse, Port und Firewall pruefen";
    else if (code == HTTPC_ERROR_READ_TIMEOUT)
      r.error = "Zeitueberschreitung: " + hostPort + " antwortet nicht innerhalb von " + String(timeoutMs / 1000) + " s";
    else
      r.error = "Verbindungsfehler zu " + hostPort + ": " + why;
  }

  http.end();
  r.ms = millis() - t0;
  return r;
}

ApiResult Bambuddy::getPrinters(JsonDocument &doc, const String &host, const String &key) {
  String body;
  ApiResult r = request(false, "/printers/", &body, host, key, 10000);
  if (!r.ok) return r;
  DeserializationError e = deserializeJson(doc, body);
  if (e) {
    r.ok = false;
    r.error = String("Antwort ist kein JSON (") + e.c_str() + "). Zeigt die Adresse wirklich auf Bambuddy?";
  }
  return r;
}

ApiResult Bambuddy::getStatus(int printerId, PrinterStatus &out) {
  settings.lock();
  String host = settings.host, key = settings.apiKey;
  uint32_t timeout = settings.httpTimeoutMs;
  settings.unlock();

  String body;
  ApiResult r = request(false, "/printers/" + String(printerId) + "/status", &body, host, key, timeout);
  if (!r.ok) return r;

  // The status document is large (temperatures, AMS trays, HMS errors …).
  // Only keep the handful of fields the button needs.
  JsonDocument filter;
  filter["awaiting_plate_clear"] = true;
  filter["chamber_light"] = true;
  filter["connected"] = true;
  filter["state"] = true;
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, body, DeserializationOption::Filter(filter));
  if (e) {
    r.ok = false;
    r.error = String("Status ist kein JSON: ") + e.c_str();
    return r;
  }
  out.awaitingPlateClear = doc["awaiting_plate_clear"] | false;
  out.chamberLight = doc["chamber_light"] | false;
  out.connected = doc["connected"] | false;
  const char *st = doc["state"].as<const char *>();
  out.state = st ? st : "";
  out.valid = true;
  return r;
}

ApiResult Bambuddy::clearPlate(int printerId) {
  settings.lock();
  String host = settings.host, key = settings.apiKey;
  uint32_t timeout = settings.httpTimeoutMs;
  settings.unlock();

  ApiResult r = request(true, "/printers/" + String(printerId) + "/clear-plate", nullptr, host, key, timeout);
  if (!r.ok && r.status == 400) {
    // Bambuddy: "Printer is not awaiting plate-clear acknowledgment". That is
    // not a connection problem; the printer simply has nothing to clear.
    r.notAwaiting = true;
    r.error = "Drucker wartet gerade nicht auf eine Plattenfreigabe (" + detailFrom(r.body) + ")";
  }
  return r;
}
