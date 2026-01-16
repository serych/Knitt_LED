/**
 * @file LedView.h
 * @brief NeoPixel row renderer for the current knitting row.
 *
 * LED0 is reserved for status; pattern columns map to LEDs 1..N.
 * Inactive pixels are rendered using a configurable dim color.
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
    : _strip(ledCount, pin, type), _statusColor(0), _rowBrightness(255) {}

  void begin(uint8_t brightness) {
    _strip.begin();
    _strip.setBrightness(255);
    _rowBrightness = brightness;
    _strip.clear();
    _strip.show();
  }

  void setBrightness(uint8_t b) {
    _rowBrightness = b;
  }

  uint32_t color(uint8_t r, uint8_t g, uint8_t b) {
    return _strip.Color(r, g, b);
  }

  uint32_t dimColor(uint32_t packed, uint8_t brightness) {
    uint8_t a = (packed >> 16) & 0xFF;
    uint8_t b = (packed >> 8) & 0xFF;
    uint8_t c = packed & 0xFF;
    a = (uint8_t)((uint16_t)a * brightness / 255);
    b = (uint8_t)((uint16_t)b * brightness / 255);
    c = (uint8_t)((uint16_t)c * brightness / 255);
    return ((uint32_t)a << 16) | ((uint32_t)b << 8) | c;
  }

  void setStatusColor(uint32_t c) {
    _statusColor = c;
    _strip.setPixelColor(0, _statusColor);
    _strip.show();
  }

  // LED1 is RIGHTMOST = needle #1.
  // Internal col 0 is LEFT, so mapping: col->led = 1 + (w-1-col)
  void showRow(const Pattern& p, int row, bool confirmed, const AppConfig& cfg) {
    _strip.clear();

    int w = p.w;
    int leds = _strip.numPixels();
    int rowLeds = max(0, leds - 1);
    int use = min(w, rowLeds);
    uint32_t baseOn = confirmed ? cfg.colorConfirmed : cfg.colorActive;
    uint32_t baseOff = cfg.colorInactive;
    uint32_t colOn = dimColor(baseOn, _rowBrightness);
    uint32_t colOff = dimColor(baseOff, _rowBrightness);

    for (int c = 0; c < use; c++) {
      int li = 1 + (w - 1) - c;
      if (li >= 1 && li < leds) {
        _strip.setPixelColor(li, p.px[row][c] ? colOn : colOff);
      }
    }
    _strip.setPixelColor(0, _statusColor);
    _strip.show();
  }

  void blinkRow(const Pattern& p, int row, bool confirmed, const AppConfig& cfg, bool on) {
    if (on) showRow(p, row, confirmed, cfg);
    else {
      _strip.clear();
      _strip.setPixelColor(0, _statusColor);
      _strip.show();
    }
  }

private:
  Adafruit_NeoPixel _strip;
  uint32_t _statusColor;
  uint8_t _rowBrightness;
};
