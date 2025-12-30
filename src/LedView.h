/**
 * @file LedView.h
 * @brief NeoPixel row renderer for the current knitting row.
 *
 * Maps pattern columns to LEDs so that LED0 is the rightmost needle (#1).
 * Only pixels set to 1 are illuminated.
 */

#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "Pattern.h"
#include "AppConfig.h"

/**
 * @brief NeoPixel strip renderer for a single pattern row.
 */
class LedView {
public:
  LedView(uint16_t ledCount, int pin, neoPixelType type)
    : _strip(ledCount, pin, type) {}

  void begin(uint8_t brightness) {
    _strip.begin();
    _strip.setBrightness(brightness);
    _strip.clear();
    _strip.show();
  }

  void setBrightness(uint8_t b) {
    _strip.setBrightness(b);
    _strip.show();
  }

  // LED0 is RIGHTMOST = needle #1.
  // Internal col 0 is LEFT, so mapping: col->led = (w-1-col)
  void showRow(const Pattern& p, int row, bool confirmed, const AppConfig& cfg) {
    _strip.clear();

    int w = p.w;
    int leds = _strip.numPixels();
    int use = min(w, leds);
    uint32_t col = confirmed ? cfg.colorConfirmed : cfg.colorActive;

    for (int c = 0; c < use; c++) {
      if (!p.px[row][c]) continue;
      int li = (w - 1) - c;
      if (li >= 0 && li < leds) _strip.setPixelColor(li, col);
    }
    _strip.show();
  }

  void blinkRow(const Pattern& p, int row, bool confirmed, const AppConfig& cfg, bool on) {
    if (on) showRow(p, row, confirmed, cfg);
    else {
      _strip.clear();
      _strip.show();
    }
  }

private:
  Adafruit_NeoPixel _strip;
};
