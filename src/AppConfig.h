#pragma once
#include <Arduino.h>

struct AppConfig {
  // LED settings
  uint32_t colorActive = 0x00FF00;     // green
  uint32_t colorConfirmed = 0x0000FF;  // blue
  uint8_t brightness = 64;            // 0..255

  // behavior
  bool autoAdvance = true;
  bool blinkWarning = true;

  // persistence
  String currentPatternFile = "/patterns/default.json";
  int activeRow = 0;

  // counters
  uint32_t totalPulses = 0;            // total carriage sensor pulses since boot (or persist if you want)

  // warning state
  bool warnBlinkActive = false;
};
