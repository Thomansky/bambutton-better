#pragma once
#include <Arduino.h>
#include <vector>

// Improv Wi-Fi over serial (https://www.improv-wifi.com/serial/).
//
// ESP Web Tools speaks this on the same USB port it flashes through: right
// after installing, and any time the board is plugged in, the flash page can
// offer "Connect to Wi-Fi", let the user pick a network from a scan, send
// the credentials and show a link to the device. The credentials are tried
// through Net::startTest(), exactly like the captive portal does, so only a
// working connection is stored.
//
// The parser looks for the "IMPROV" header in the incoming byte stream, so
// the board's ordinary log output on the same port does not disturb it.
class ImprovSerial {
 public:
  void begin(Stream &s);
  void loop();

 private:
  enum : uint8_t { TYPE_STATE = 0x01, TYPE_ERROR = 0x02, TYPE_RPC = 0x03, TYPE_RESULT = 0x04 };
  enum : uint8_t { STATE_READY = 0x02, STATE_PROVISIONING = 0x03, STATE_PROVISIONED = 0x04 };
  enum : uint8_t { ERR_NONE = 0x00, ERR_INVALID_RPC = 0x01, ERR_UNKNOWN_CMD = 0x02, ERR_CONNECT = 0x03, ERR_UNKNOWN = 0xFF };
  enum : uint8_t { CMD_WIFI = 0x01, CMD_STATE = 0x02, CMD_INFO = 0x03, CMD_SCAN = 0x04 };

  void feed(uint8_t b);
  void handlePacket(uint8_t type, const uint8_t *data, uint8_t len);
  void handleRpc(const uint8_t *data, uint8_t len);
  void sendState(uint8_t state);
  void sendError(uint8_t err);
  void sendResult(uint8_t cmd, const std::vector<String> &strings);
  void sendPacket(uint8_t type, const uint8_t *data, size_t len);
  std::vector<String> deviceUrls();

  Stream *_s = nullptr;
  uint8_t _buf[300];
  size_t _pos = 0;
  uint32_t _lastByte = 0;
  bool _provisioning = false;
  uint32_t _seq = 0;
  uint32_t _n = 0;
  bool _scanPending = false;
  uint32_t _scanRequestedAt = 0;
};

extern ImprovSerial improv;
