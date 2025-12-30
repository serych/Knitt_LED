/**
 * @file Pattern.cpp
 * @brief Implementation of Pattern JSON serialization/parsing.
 *
 * Parsing is intentionally lightweight to keep dependencies small.
 * If you later add ArduinoJson, this module can be simplified.
 */

#include "Pattern.h"

static String esc(const String& s) {
  String out;
  out.reserve(s.length());
  for (char c : s) {
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"':  out += "\\\""; break;
      default: out += c; break;
    }
  }
  return out;
}

String patternToJson(const Pattern& p) {
  String json;
  json.reserve(2200);
  json += "{";
  json += "\"name\":\"" + esc(p.name) + "\",";
  json += "\"w\":" + String(p.w) + ",";
  json += "\"h\":" + String(p.h) + ",";
  json += "\"pixels\":[";
  for (int r = 0; r < p.h; r++) {
    String row;
    row.reserve(p.w);
    for (int c = 0; c < p.w; c++) row += (p.px[r][c] ? '1' : '0');
    json += "\"";
    json += row;
    json += "\"";
    if (r != p.h - 1) json += ",";
  }
  json += "]}";
  return json;
}

bool jsonToPattern(const String& json, Pattern& out) {
  auto findInt = [&](const char* key, int& value) -> bool {
    String k = String("\"") + key + "\":";
    int i = json.indexOf(k);
    if (i < 0) return false;
    i += k.length();
    int j = i;
    while (j < (int)json.length() && (isdigit(json[j]) || json[j] == '-')) j++;
    value = json.substring(i, j).toInt();
    return true;
  };

  auto findString = [&](const char* key, String& value) -> bool {
    String k = String("\"") + key + "\":\"";
    int i = json.indexOf(k);
    if (i < 0) return false;
    i += k.length();
    int j = json.indexOf("\"", i);
    if (j < 0) return false;
    value = json.substring(i, j);
    return true;
  };

  int w = 0, h = 0;
  if (!findInt("w", w)) return false;
  if (!findInt("h", h)) return false;
  if (w < 1 || w > MAX_W) return false;
  if (h < 1 || h > MAX_H) return false;

  Pattern p;
  p.w = w;
  p.h = h;
  findString("name", p.name);

  int pixKey = json.indexOf("\"pixels\":[");
  if (pixKey < 0) return false;
  int i = json.indexOf("[", pixKey);
  int end = json.indexOf("]", i);
  if (i < 0 || end < 0) return false;

  int r = 0;
  int pos = i + 1;
  while (r < h) {
    int q1 = json.indexOf("\"", pos);
    if (q1 < 0 || q1 > end) break;
    int q2 = json.indexOf("\"", q1 + 1);
    if (q2 < 0 || q2 > end) break;

    String row = json.substring(q1 + 1, q2);
    if ((int)row.length() != w) return false;

    for (int c = 0; c < w; c++) p.px[r][c] = (row[c] == '1');

    r++;
    pos = q2 + 1;
  }

  if (r != h) return false;
  out = p;
  return true;
}
