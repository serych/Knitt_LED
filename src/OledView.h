#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

class OledView {
public:
  explicit OledView(U8G2& d) : _d(d) {}

  void begin() { _d.begin(); }

  void showIp(const String& ip) {
    _d.clearBuffer();
    _d.setFont(u8g2_font_8x13_tf);
    _d.drawStr(0, 14, "Connected");
    _d.setFont(u8g2_font_6x12_tf);
    _d.drawStr(0, 30, ip.c_str());
    _d.sendBuffer();
  }

  // Big readable status line:
  // Row:07/24, Tot:53
  void showKnitStatus(int row1based, int rowsTotal, uint32_t tot) {
    char buf1[32];
    snprintf(buf1, sizeof(buf1), "Row:%02d/%02d", row1based, rowsTotal);

    char buf2[32];
    snprintf(buf2, sizeof(buf2), "Tot:%lu", (unsigned long)tot);

    _d.clearBuffer();
    // Big-ish font that fits 128x32 nicely:
    // 8x13 for first line, 9x18 for second would be tight;
    // This combo is very readable:
    _d.setFont(u8g2_font_8x13_tf);
    _d.drawStr(0, 14, buf1);

    _d.setFont(u8g2_font_8x13B_tf);
    _d.drawStr(0, 30, buf2);

    _d.sendBuffer();
  }

private:
  U8G2& _d;
};
