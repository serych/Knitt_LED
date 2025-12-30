/**
 * @file Buttons.h
 * @brief Debounced edge-detect button helper.
 *
 * Implements a small debouncer for INPUT_PULLUP buttons.
 * Call pressed() repeatedly; it returns true once per press.
 */

#pragma once
#include <Arduino.h>

/**
 * @brief Debounced button with edge detection.
 *
 * Designed for INPUT_PULLUP wiring (pressed = LOW).
 */
class EdgeButton {
public:
  explicit EdgeButton(uint32_t debounceMs = 60) : _debounceMs(debounceMs) {}

  /** @brief Initialize the button GPIO. */
    void begin(int pin, bool pullup = true) {
    _pin = pin;
    pinMode(_pin, pullup ? INPUT_PULLUP : INPUT);
    _lastStable = digitalRead(_pin);
    _lastRead = _lastStable;
    _lastChangeMs = millis();
  }

  // true on pressed edge (HIGH->LOW when using pull-up)
  /**
   *    * @brief Check for a debounced press event.
   *    * @return true exactly once per physical press (HIGH->LOW with pull-up).
   *    */
    bool pressed() {
    bool r = digitalRead(_pin);
    uint32_t now = millis();

    if (r != _lastRead) {
      _lastRead = r;
      _lastChangeMs = now;
    }

    if ((now - _lastChangeMs) > _debounceMs && _lastStable != _lastRead) {
      bool prev = _lastStable;
      _lastStable = _lastRead;
      if (prev == true && _lastStable == false) return true;
    }
    return false;
  }

private:
  int _pin = -1;
  uint32_t _debounceMs;
  bool _lastStable = true;
  bool _lastRead = true;
  uint32_t _lastChangeMs = 0;
};
