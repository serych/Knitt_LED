#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

// Create display object
// U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C is the most common 128x32 OLED
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(
  U8G2_R0,      // rotation
  U8X8_PIN_NONE // reset pin (not used)
);

void setup() {
  // Initialize I2C (ESP32 default pins)
  Wire.begin(21, 22);

  // Initialize display
  u8g2.begin();

  // Clear buffer
  u8g2.clearBuffer();

  // Select font
  u8g2.setFont(u8g2_font_8x13_tf);

  // Draw text
  u8g2.drawStr(0, 15, "Hello World!");

  // Send buffer to display
  u8g2.sendBuffer();
}

void loop() {
  // Nothing needed here for static text
}
