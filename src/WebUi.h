/**
 * @file WebUi.h
 * @brief Web UI and REST-like API for editing and knitting modes.
 *
 * The web UI is served from the ESP32 and provides:
 * - Pattern editor (grid)
 * - Knitting mode (active row highlight, confirm/step)
 * - File management (LittleFS, no SD card)
 * - Device configuration (colors incl. inactive, brightness, behavior, row direction)
 *
 * Pattern files are stored under @c /patterns in LittleFS as JSON.
 */

#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>

#include "Pattern.h"
#include "AppConfig.h"

/**
 * @brief Dependency bundle injected into the Web UI module.
 *
 * The web server instance is owned by the application (main.cpp).
 * WebUi registers handlers and uses these pointers for reading/writing
 * application state.
 */
struct WebUiDeps {
  WebServer* server;     /**< @brief Web server instance (port 80). */
  Pattern* pattern;      /**< @brief Current in-memory pattern. */
  AppConfig* cfg;        /**< @brief Current configuration/state. */
  bool* rowConfirmed;    /**< @brief Confirmation flags array [MAX_H]. */
};

/**
 * @brief Register all routes for the web UI on the provided server.
 *
 * Routes:
 * - GET  @c /                : HTML UI
 * - GET  @c /api/files        : JSON list of pattern file paths
 * - GET  @c /api/pattern      : Load pattern (query param @c file)
 * - POST @c /api/pattern      : Save pattern (JSON body)
 * - POST @c /api/delete       : Delete file (JSON body)
 * - POST @c /api/row          : Step row (+1/-1) (JSON body)
 * - POST @c /api/confirm      : Confirm current row and optionally auto-advance
 * - GET  @c /api/state        : Current state for polling
 * - GET  @c /api/config       : Read config
 * - POST @c /api/config       : Update config
 * - POST @c /api/reset        : Reset row/total state
 * - GET  @c /download         : Download a pattern file
 * - POST @c /upload           : Upload a pattern file
 */
void webuiBegin(WebUiDeps deps);

/** @brief Return JSON array of stored pattern files (paths). */
String listPatternFilesJson();

/**
 * @brief Load a pattern from LittleFS.
 * @param path File path (either full path or just filename; will be normalized).
 * @param p Destination pattern.
 * @return true on success.
 */
bool loadPatternFile(const String& path, Pattern& p);

/**
 * @brief Save a pattern to LittleFS.
 * @param path File path (either full path or just filename; will be normalized).
 * @param p Pattern to save.
 * @return true on success.
 */
bool savePatternFile(const String& path, const Pattern& p);
