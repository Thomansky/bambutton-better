#include "Station.h"
#include "Settings.h"

static const uint32_t SHORT_PRESS_MS = 1500;  // released within this: clear plate
static const uint32_t LONG_PRESS_MS = 5000;   // held this long: open the setup network

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
      _changeStart = now ? now : 1;
    } else if (now - _changeStart >= 40) {  // 40 ms debounce
      _stable = raw;
      _changeStart = 0;
      if (raw) {
        _pressedAt = now ? now : 1;  // pressed: decide on release (or at 5 s)
        _longFired = false;
      } else if (_pressedAt && !_longFired && now - _pressedAt < SHORT_PRESS_MS) {
        _pending = true;             // released quickly: a normal press
      }
    }
  } else {
    _changeStart = 0;
  }
  // Still held after 5 s: fire the long press once while the finger is
  // still on the button, so the LED can acknowledge it right away.
  if (_stable && _pressedAt && !_longFired && now - _pressedAt >= LONG_PRESS_MS) {
    _longFired = true;
    _longPending = true;
  }
}

bool Station::consumePress() {
  if (!_pending) return false;
  _pending = false;
  return true;
}

bool Station::consumeLongPress() {
  if (!_longPending) return false;
  _longPending = false;
  return true;
}

void Station::identify(uint32_t durationMs) {
  _identifyUntil = millis() + durationMs;
  if (_identifyUntil == 0) _identifyUntil = 1;
}

void Station::feedback(bool ok) {
  _fbOk = ok;
  _fbUntil = millis() + (ok ? 900 : 1500);
  if (_fbUntil == 0) _fbUntil = 1;
}

void Station::updateLed(uint32_t now) {
  if (_led < 0) return;
  bool on;
  if (_identifyUntil != 0 && (int32_t)(_identifyUntil - now) > 0) {
    on = ((now / 150) % 2) == 0;          // fast blink: "this is the one"
  } else if (_fbUntil != 0 && (int32_t)(_fbUntil - now) > 0) {
    on = _fbOk ? ((now / 220) % 2) == 0   // two calm blinks: accepted
               : ((now / 45) % 2) == 0;   // flicker: rejected / failed
  } else if (!active) {
    on = false;                           // no printer assigned -> dark
  } else if (busy) {
    on = ((now / 80) % 2) == 0;           // very fast: request in flight
  } else if (noLink) {
    on = (now % 2000) < 70;               // heartbeat blip: no connection
  } else if (awaiting) {
    on = ((now / 400) % 2) == 0;          // slow blink: plate needs clearing
  } else if (idleMode == IDLE_ON) {
    on = true;
  } else if (idleMode == IDLE_OFF) {
    on = false;
  } else {
    on = chamberLight;                    // follow the chamber light
  }
  if (on != _ledOn) {
    _ledOn = on;
    digitalWrite(_led, on ? HIGH : LOW);
  }
}
