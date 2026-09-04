#include "Station.h"

void Station::begin(int idx, int ledPin, int buttonPin, int printerId_) {
  index = idx;
  _led = ledPin;
  _btn = buttonPin;
  printerId = printerId_;
  active = printerId > 0;
  pinMode(_led, OUTPUT);
  digitalWrite(_led, LOW);
  pinMode(_btn, INPUT_PULLDOWN);  // button pulls the pin to 3V3 when pressed
  _stable = (digitalRead(_btn) == HIGH);
}

void Station::pollButton() {
  if (_btn < 0) return;
  bool raw = (digitalRead(_btn) == HIGH);
  uint32_t now = millis();
  if (raw != _stable) {
    if (_changeStart == 0) {
      _changeStart = now;
    } else if (now - _changeStart >= 40) {  // 40 ms debounce
      _stable = raw;
      _changeStart = 0;
      if (raw) _pending = true;  // rising edge = pressed
    }
  } else {
    _changeStart = 0;
  }
}

bool Station::consumePress() {
  if (!_pending) return false;
  _pending = false;
  return true;
}

void Station::identify(uint32_t durationMs) {
  _identifyUntil = millis() + durationMs;
  if (_identifyUntil == 0) _identifyUntil = 1;
}

void Station::updateLed(uint32_t now) {
  if (_led < 0) return;
  bool on;
  if (_identifyUntil != 0 && (int32_t)(_identifyUntil - now) > 0) {
    on = ((now / 150) % 2) == 0;  // fast blink: "this is the one"
  } else if (!active) {
    on = false;                   // no printer assigned -> dark
  } else if (busy) {
    on = ((now / 80) % 2) == 0;   // very fast: request in flight
  } else if (awaiting) {
    on = ((now / 400) % 2) == 0;  // slow blink: plate needs clearing
  } else {
    on = chamberLight;            // otherwise follow the chamber light
  }
  if (on != _ledOn) {
    _ledOn = on;
    digitalWrite(_led, on ? HIGH : LOW);
  }
}
