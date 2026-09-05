#include "Bambuddy.h"
#include "Settings.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <ESPmDNS.h>

Bambuddy bambuddy;

static const size_t BODY_CAP = 24576;  // a status document is a few KB; never let it eat the heap

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

// HTTPClient::getString() loops for as long as the socket looks connected, so
// a peer that vanished mid-response (half-open TCP) would hang the worker
// until the watchdog reboots the board. Read with a deadline instead.
static String readBody(HTTPClient &http, WiFiClient &client, uint32_t timeoutMs) {
  if (http.header("Transfer-Encoding").equalsIgnoreCase("chunked")) return http.getString();
  int len = http.getSize();  // -1 = unknown (read until the server closes)
  String out;
  if (len > 0) out.reserve((size_t)len < BODY_CAP ? len : BODY_CAP);
  uint32_t deadline = millis() + timeoutMs;
  uint8_t buf[256];
  while ((len < 0 || (int)out.length() < len) && out.length() < BODY_CAP) {
    if ((int32_t)(deadline - millis()) <= 0) break;
    int avail = client.available();
    if (avail > 0) {
      int n = client.read(buf, avail > (int)sizeof(buf) ? sizeof(buf) : avail);
      if (n > 0) {
        out.concat((const char *)buf, n);
        continue;
      }
    }
    if (!client.connected()) break;
    delay(5);
  }
  return out;
}

ApiResult Bambuddy::request(bool post, const String &path, String *bodyOut,
                            const String &host, const String &key, uint32_t timeoutMs) {
  ApiResult r;
  uint32_t t0 = millis();

  if (host.length() == 0 || key.length() == 0) {
    r.error = tr("Bambuddy ist nicht konfiguriert (Adresse und API-Key eintragen)",
                 "Bambuddy is not configured (enter address and API key)");
    return r;
  }
  if (WiFi.status() != WL_CONNECTED) {
    r.error = tr("Kein WLAN", "No Wi-Fi");
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
  // Our paths match Bambuddy's routes exactly; this only covers a GET being
  // redirected by FastAPI's trailing-slash handling after a route change.
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  const char *keep[] = {"Transfer-Encoding"};
  http.collectHeaders(keep, 1);

  if (!http.begin(client, url)) {
    r.error = String(tr("Ungueltige Adresse: ", "Invalid address: ")) + url;
    r.ms = millis() - t0;
    return r;
  }
  http.addHeader("X-API-Key", key);
  http.addHeader("Accept", "application/json");

  // clear-plate takes no request body. The core omits Content-Length for an
  // empty payload, so add it ourselves; the server must never wait for data.
  if (post) http.addHeader("Content-Length", "0");
  int code = post ? http.POST(String("")) : http.GET();
  r.status = code;

  if (code > 0) {
    String b = readBody(http, client, timeoutMs);
    r.body = b.length() > 300 ? b.substring(0, 300) : b;
    if (bodyOut) *bodyOut = std::move(b);
    r.ok = (code >= 200 && code < 300);
    if (!r.ok) {
      String detail = detailFrom(r.body);
      switch (code) {
        case 401:
        case 403:
          r.error = String(tr("API-Key abgelehnt (HTTP ", "API key rejected (HTTP ")) + String(code) +
                    tr("). Key pruefen; er braucht die Rechte printers:read und printers:clear_plate.",
                       "). Check the key; it needs the permissions printers:read and printers:clear_plate.");
          break;
        case 404:
          r.error = String(tr("Nicht gefunden (HTTP 404): Adresse oder Drucker-ID pruefen. ",
                              "Not found (HTTP 404): check the address or printer ID. ")) + detail;
          break;
        case 429:
          r.error = tr("Bambuddy drosselt die Anfragen (HTTP 429): Abfrageintervall erhoehen.",
                       "Bambuddy is rate limiting (HTTP 429): increase the poll interval.");
          break;
        default:
          if (code >= 500)
            r.error = String(tr("Bambuddy-Serverfehler (HTTP ", "Bambuddy server error (HTTP ")) + String(code) + "): " + detail;
          else
            r.error = "HTTP " + String(code) + ": " + detail;
      }
    }
  } else {
    String why = HTTPClient::errorToString(code);
    if (code == HTTPC_ERROR_CONNECTION_REFUSED)
      r.error = String(tr("Keine Verbindung zu ", "Cannot reach ")) + hostPort +
                tr(" (abgelehnt oder nicht erreichbar): Adresse, Port und Firewall pruefen",
                   " (refused or unreachable): check address, port and firewall");
    else if (code == HTTPC_ERROR_READ_TIMEOUT)
      r.error = String(tr("Zeitueberschreitung: ", "Timeout: ")) + hostPort +
                tr(" antwortet nicht innerhalb von ", " did not answer within ") + String(timeoutMs / 1000) + " s";
    else
      r.error = String(tr("Verbindungsfehler zu ", "Connection error to ")) + hostPort + ": " + why;
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
    r.error = String(tr("Antwort ist kein JSON (", "Response is not JSON (")) + e.c_str() +
              tr("). Zeigt die Adresse wirklich auf Bambuddy?", "). Does the address really point at Bambuddy?");
  }
  return r;
}

ApiResult Bambuddy::getStatus(int printerId, PrinterStatus &out) {
  settings.lock();
  String host = settings.host, key = settings.apiKey;
  uint32_t timeout = settings.httpTimeoutMs;
  settings.unlock();
  // Status is served from Bambuddy's memory and answers fast; a long timeout
  // here would only delay button presses queued behind a poll.
  if (timeout > 6000) timeout = 6000;

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
    r.error = String(tr("Status ist kein JSON: ", "Status is not JSON: ")) + e.c_str();
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
    r.error = String(tr("Drucker wartet gerade nicht auf eine Plattenfreigabe (",
                        "Printer is not waiting for a plate clear right now (")) + detailFrom(r.body) + ")";
  }
  return r;
}
