#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Every call records exactly what happened so the web UI can show it.
// The MicroPython predecessor only printed errors to the serial console,
// which made failures invisible in normal use.
struct ApiResult {
  bool ok = false;
  int status = 0;   // HTTP status, or a negative HTTPClient error code
  String body;      // response body (truncated)
  String error;     // human readable summary, empty when ok
  uint32_t ms = 0;  // duration, so slow calls are obvious
};

struct PrinterStatus {
  bool awaitingPlateClear = false;
  bool chamberLight = false;
  bool valid = false;
};

class Bambuddy {
 public:
  ApiResult getPrinters(JsonDocument &doc);
  ApiResult getStatus(int printerId, PrinterStatus &out);
  ApiResult clearPlate(int printerId);

 private:
  ApiResult request(bool post, const String &path, String *bodyOut);
};

extern Bambuddy bambuddy;
