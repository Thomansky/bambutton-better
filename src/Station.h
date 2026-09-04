#pragma once
#include <Arduino.h>

// One physical button + LED, bound to one Bambuddy printer.
class Station {
 public:
  void begin(int idx, int ledPin, int buttonPin, int printerId);
  void pollButton();               // call often — debounced edge detection
  void updateLed(uint32_t nowMs);  // call often — time based, no timers needed
  void identify(uint32_t durationMs);
  bool consumePress();             // true exactly once per press

  int index = 0;
  int printerId = 0;
  bool active = false;

  // written by the worker task, read by the LED/web code
  volatile bool awaiting = false;
  volatile bool chamberLight = true;
  volatile bool busy = false;  // a clear-plate call is in flight

 private:
  int _led = -1;
  int _btn = -1;
  bool _stable = false;
  uint32_t _changeStart = 0;
  volatile bool _pending = false;
  uint32_t _identifyUntil = 0;
  bool _ledOn = false;
};
