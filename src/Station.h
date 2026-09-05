#pragma once
#include <Arduino.h>

// One physical button + LED, bound to one Bambuddy printer.
//
// Button: a short press (released within 1.5 s) clears the plate; holding
// for 5 s opens the setup network (reported via consumeLongPress). Presses
// between those two lengths do nothing.
//
// LED vocabulary (single colour, so patterns are kept clearly distinct):
//   identify   150 ms blink for a few seconds   "this is the one" (web UI, long press)
//   busy        80 ms blink                     clear-plate request in flight
//   feedback   2 slow blinks = accepted, 1.5 s flicker = rejected/failed
//   no link    short blip every 2 s             no Wi-Fi / Bambuddy unreachable
//   awaiting   400 ms blink                     plate needs clearing
//   idle       chamber light / on / off         (configurable)
class Station {
 public:
  void begin(int idx, int ledPin, int buttonPin, int printerId);
  void pollButton();               // call often — debounced edge detection
  void updateLed(uint32_t nowMs);  // call often — time based, no timers needed
  void identify(uint32_t durationMs);
  void feedback(bool ok);          // result of a clear-plate attempt
  bool consumePress();             // true exactly once per short press
  bool consumeLongPress();         // true exactly once per 5 s hold
  bool isPressed() const { return _stable; }

  int index = 0;
  int printerId = 0;
  bool active = false;
  uint8_t idleMode = 0;            // IdleLed from Settings

  // written by the worker task, read by the LED/web code
  volatile bool awaiting = false;
  volatile bool chamberLight = true;
  volatile bool busy = false;      // a clear-plate call is in flight
  volatile bool noLink = false;    // Wi-Fi down or Bambuddy not answering
  volatile bool online = true;     // printer connected to Bambuddy

 private:
  int _led = -1;
  int _btn = -1;
  bool _stable = false;
  uint32_t _changeStart = 0;
  uint32_t _pressedAt = 0;
  bool _longFired = false;
  volatile bool _pending = false;
  volatile bool _longPending = false;
  uint32_t _identifyUntil = 0;
  uint32_t _fbUntil = 0;
  bool _fbOk = false;
  bool _ledOn = false;
};
