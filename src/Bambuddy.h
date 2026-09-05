#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Every call records exactly what happened so the web UI can show it.
struct ApiResult {
  bool ok = false;
  int status = 0;            // HTTP status, or a negative HTTPClient error code
  bool notAwaiting = false;  // clear-plate: Bambuddy said "nothing to clear" (HTTP 400)
  String body;               // response body (truncated)
  String detail;             // "detail"/"error"/"message" from an error body, else the body excerpt
  String error;              // human readable summary, empty when ok
  uint32_t ms = 0;           // duration, so slow calls are obvious
};

struct PrinterStatus {
  bool awaitingPlateClear = false;
  bool chamberLight = false;
  bool connected = false;
  String state;              // IDLE / RUNNING / PAUSE / FINISH / FAILED / ...
  bool valid = false;
};

class Bambuddy {
 public:
  // Uses the given host/key (connection test from the web UI) …
  ApiResult getPrinters(JsonDocument &doc, const String &host, const String &key);
  // … or the stored configuration.
  ApiResult getStatus(int printerId, PrinterStatus &out);
  ApiResult clearPlate(int printerId);

 private:
  // With streamDoc/filter set, a 2xx body is parsed straight from the socket
  // (only the filtered fields are kept), so the size of the document does not
  // matter. Without them the body is read into memory (bounded).
  ApiResult request(bool post, const String &path, String *bodyOut,
                    const String &host, const String &key, uint32_t timeoutMs,
                    JsonDocument *streamDoc = nullptr, JsonDocument *filter = nullptr);
  String resolveHost(const String &hostPort);
  String _mdnsName, _mdnsIp;
  uint32_t _mdnsAt = 0;
};

extern Bambuddy bambuddy;
