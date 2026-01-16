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

  /**
   * @brief Check for a debounced press event.
   * @return true exactly once per physical press (HIGH->LOW with pull-up).
   */
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

/**
 * @brief Debounced touch button with edge detection.
 *
 * Uses touchRead() and a threshold (pressed when reading is below threshold).
 */
class TouchButton {
public:
  TouchButton(uint16_t threshold, uint32_t debounceMs = 60)
    : _threshold(threshold), _debounceMs(debounceMs) {}

  /** @brief Initialize the touch button pin. */
  void begin(int pin) {
    _pin = pin;
    _lastStable = readTouch();
    _lastRead = _lastStable;
    _lastChangeMs = millis();
  }

  /** @brief Check for a debounced touch press event. */
  bool pressed() {
    bool r = readTouch();
    uint32_t now = millis();

    if (r != _lastRead) {
      _lastRead = r;
      _lastChangeMs = now;
    }

    if ((now - _lastChangeMs) > _debounceMs && _lastStable != _lastRead) {
      bool prev = _lastStable;
      _lastStable = _lastRead;
      if (prev == false && _lastStable == true) return true;
    }
    return false;
  }

private:
  bool readTouch() const { return touchRead(_pin) < _threshold; }

  int _pin = -1;
  uint16_t _threshold;
  uint32_t _debounceMs;
  bool _lastStable = false;
  bool _lastRead = false;
  uint32_t _lastChangeMs = 0;
};
