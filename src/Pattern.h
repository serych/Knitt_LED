#pragma once
#include <Arduino.h>

static constexpr int MAX_W = 12;
static constexpr int MAX_H = 24;

struct Pattern {
  String name = "default";
  int w = 12;
  int h = 24;
  bool px[MAX_H][MAX_W]{};
};

String patternToJson(const Pattern& p);
bool jsonToPattern(const String& json, Pattern& out);
