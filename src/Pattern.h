/**
 * @file Pattern.h
 * @brief Pattern data model and JSON serialization.
 *
 * Patterns are stored as a compact JSON file in LittleFS.
 * The file format is project-specific (not an industry standard).
 */

#pragma once
#include <Arduino.h>

static constexpr int MAX_W = 12;
static constexpr int MAX_H = 24;

/**
 * @brief Knitting pattern grid.
 *
 * Row index 0 is the top row in storage.
 * Columns are stored left-to-right.
 */
struct Pattern {
  String name = "default";
  int w = 12;
  int h = 24;
  bool px[MAX_H][MAX_W]{};
};

/** @brief Serialize pattern to JSON string. */
String patternToJson(const Pattern& p);
/**
 *  * @brief Parse pattern JSON into @p out.
 *  * @return true on success, false if JSON is invalid or out of bounds.
 *  */
bool jsonToPattern(const String& json, Pattern& out);
