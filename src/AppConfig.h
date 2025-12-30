/**
 * @file AppConfig.h
 * @brief Persistent runtime configuration for KnittLED.
 *
 * This header defines @ref AppConfig which holds user-configurable settings
 * (LED colors/brightness, behavior toggles, row direction) and runtime state
 * that may be persisted (active pattern file, active row).
 */

#pragma once
#include <Arduino.h>

/**
 * @brief Application configuration and runtime state.
 *
 * Most fields are persisted using Preferences (see AppConfig.cpp).
 * Some fields (like counters) are runtime-only unless you decide to persist them.
 */
struct AppConfig {
  /** @name LED rendering settings */
  ///@{
  /** @brief Color used for the active (unconfirmed) row, 0xRRGGBB. */
  uint32_t colorActive = 0x00FF00;     // green

  /** @brief Color used for the confirmed row, 0xRRGGBB. */
  uint32_t colorConfirmed = 0x0000FF;  // blue

  /** @brief LED brightness (0..255). */
  uint8_t brightness = 64;
  ///@}

  /** @name Row counting direction */
  ///@{
  /**
   * @brief If true, "Row 1" is the bottom row (count from bottom).
   *
   * Internal storage keeps row index 0 at the top.
   * This flag only affects stepping direction and UI numbering.
   */
  bool rowFromBottom = false;
  ///@}

  /** @name Behavior toggles */
  ///@{
  /** @brief If true, confirming a row auto-advances to the next row. */
  bool autoAdvance = true;

  /** @brief If true, LEDs blink when carriage moves without row confirmation. */
  bool blinkWarning = true;
  ///@}

  /** @name Persisted selection */
  ///@{
  /** @brief Path to the currently selected pattern file in LittleFS. */
  String currentPatternFile = "/patterns/default.json";

  /** @brief Active row index (0-based, internal top-origin indexing). */
  int activeRow = 0;
  ///@}

  /** @name Runtime counters/state */
  ///@{
  /** @brief Total carriage sensor pulses since boot (or persisted if desired). */
  uint32_t totalPulses = 0;

  /** @brief True when warning blink state is active. */
  bool warnBlinkActive = false;
  ///@}
};
