/**
 * @file main.cpp
 * @brief KnittLED application entry point.
 *
 * Initializes hardware (OLED, NeoPixels, buttons), Wi-Fi, file system, and web UI.
 * Implements knitting logic: stepping rows, confirmation, carriage sensor handling, and warning blink.
 * Row stepping wraps around and respects row counting direction (rowFromBottom).
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <Wire.h>
#include <U8g2lib.h>

// Project modules
#include "AppConfig.h"
#include "Pattern.h"
#include "LedView.h"
#include "OledView.h"
#include "Buttons.h"
#include "WebUi.h"
#include "WifiPortal.h"

// ============================================================
// ---------------------- PIN DEFINITIONS ----------------------
// ============================================================

// You can change these later
static constexpr int PIN_SDA = 21;
static constexpr int PIN_SCL = 22;

static constexpr int PIN_BTN_UP          = 14; // touch
static constexpr int PIN_BTN_DOWN        = 27; // touch
static constexpr int PIN_BTN_CONFIRM     = 13; // touch
static constexpr int PIN_SENSOR_CARRIAGE = 26;

static constexpr int PIN_NEOPIXEL = 2;
static constexpr int LED_COUNT    = 13;

static constexpr neoPixelType LED_TYPE = NEO_GRB + NEO_KHZ800;

// ============================================================
// ------------------------ GLOBALS -----------------------------
// ============================================================

WebServer server(80);
DNSServer dns;
Preferences prefs;

static bool portalActive = false;

// OLED
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
OledView oled(u8g2);

// LEDs
LedView leds(LED_COUNT, PIN_NEOPIXEL, LED_TYPE);

// Buttons
EdgeButton btnUp(60);
EdgeButton btnDown(60);
EdgeButton btnConfirm(60);
EdgeButton btnCarriage(40);

// App state
AppConfig cfg;
Pattern pattern;
bool rowConfirmed[MAX_H] = { false };

// WiFi
WifiCreds wifiCreds;

// ============================================================
// ------------------- FORWARD DECLARATIONS --------------------
// ============================================================

void loadConfig(AppConfig& cfg);
void saveConfig(const AppConfig& cfg);

// ============================================================
// -------------------- UTILITY FUNCTIONS ----------------------
// ============================================================

static void loadWifiCreds() {
  prefs.begin("knittled", true);
  wifiCreds.ssid = prefs.getString("ssid", "");
  wifiCreds.pass = prefs.getString("pass", "");
  prefs.end();
}

static void saveWifiCreds(const WifiCreds& c) {
  prefs.begin("knittled", false);
  prefs.putString("ssid", c.ssid);
  prefs.putString("pass", c.pass);
  prefs.end();
}

static void ensureFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
  }
  if (!LittleFS.exists("/patterns")) {
    LittleFS.mkdir("/patterns");
  }
}

// Wrap row index 0..h-1
static int wrapRow(int r) {
  if (pattern.h <= 0) return 0;
  while (r < 0) r += pattern.h;
  while (r >= pattern.h) r -= pattern.h;
  return r;
}

// Map "step +1/-1" to internal row index change based on direction
// rowFromBottom=false: +1 means go down (row index +1)
// rowFromBottom=true:  +1 means go up in index (row index -1) because counting starts from bottom
static void stepRow(int step) {
  cfg.warnBlinkActive = false;

  int dir = cfg.rowFromBottom ? -1 : +1;
  cfg.activeRow = wrapRow(cfg.activeRow + step * dir);

  saveConfig(cfg);
}

static int shownRowNumber1based() {
  // OLED/web should show "Row 1" at the "start" according to direction
  // activeRow is internal index where 0 is top.
  return cfg.rowFromBottom ? (pattern.h - cfg.activeRow) : (cfg.activeRow + 1);
}

static void refreshOutputs() {
  // OLED
  oled.showKnitStatus(shownRowNumber1based(), pattern.h, cfg.totalPulses);

  // LEDs (active row)
  leds.showRow(pattern, cfg.activeRow, rowConfirmed[cfg.activeRow], cfg);
}

// ============================================================
// ------------------- KNITTING ACTIONS ------------------------
// ============================================================

static void doConfirm() {
  rowConfirmed[cfg.activeRow] = true;
  cfg.warnBlinkActive = false;

  if (cfg.autoAdvance) {
    stepRow(+1);          // wraps + respects direction
  } else {
    saveConfig(cfg);
  }

  refreshOutputs();
}

static void onCarriagePulse() {
  cfg.totalPulses++;

  // Blink warning if carriage moved but current row not confirmed
  if (cfg.blinkWarning && !rowConfirmed[cfg.activeRow]) {
    cfg.warnBlinkActive = true;
  }

  stepRow(+1);            // carriage acts like "UP" (next row in chosen direction)
  refreshOutputs();
}

// ============================================================
// ------------------- MAIN WEB SERVER -------------------------
// ============================================================

static void startMainServer() {
  WebUiDeps deps;
  deps.server = &server;
  deps.pattern = &pattern;
  deps.cfg = &cfg;
  deps.rowConfirmed = rowConfirmed;

  webuiBegin(deps);
  server.begin();
}

// ============================================================
// --------------------------- SETUP ---------------------------
// ============================================================

void setup() {
  Serial.begin(115200);

  // OLED
  Wire.begin(PIN_SDA, PIN_SCL);
  oled.begin();

  // File system
  ensureFS();

  // Load config and WiFi credentials
  loadConfig(cfg);
  loadWifiCreds();

  // Load or create default pattern
  if (!loadPatternFile(cfg.currentPatternFile, pattern)) {
    pattern.name = "default";
    pattern.w = 12;
    pattern.h = 24;
    savePatternFile("/patterns/default.json", pattern);
    cfg.currentPatternFile = "/patterns/default.json";
    saveConfig(cfg);
  }

  cfg.activeRow = wrapRow(cfg.activeRow);

  // LEDs
  leds.begin(cfg.brightness);

  // Buttons
  btnUp.begin(PIN_BTN_UP, true);
  btnDown.begin(PIN_BTN_DOWN, true);
  btnConfirm.begin(PIN_BTN_CONFIRM, true);
  btnCarriage.begin(PIN_SENSOR_CARRIAGE, true);

  // ---- Try STA WiFi first ----
  if (!wifiCreds.ssid.isEmpty() && wifiConnectSTA(wifiCreds, 12000)) {
    oled.showIp(WiFi.localIP().toString());
    startMainServer();
    delay(350);
    refreshOutputs();
    return;
  }

  // ---- Fallback provisioning portal ----
  portalActive = true;
  oled.showIp("AP: KnittLED");

  wifiStartPortal(
    server,
    dns,
    "KnittLED",
    wifiCreds,
    [](const WifiCreds& c) {
      saveWifiCreds(c);
    },
    [&](const IPAddress& ip) {
      portalActive = false;

      oled.showIp(ip.toString());

      wifiStopPortal(dns);
      server.stop();

      delay(800);     // show IP briefly
      ESP.restart();  // reboot to clean STA mode
    }
  );
}

// ============================================================
// ---------------------------- LOOP ---------------------------
// ============================================================

void loop() {
  server.handleClient();
  if (portalActive) {
    dns.processNextRequest();
  }

  // Hardware controls active only when connected
  if (WiFi.status() == WL_CONNECTED) {
    if (btnUp.pressed()) {
      stepRow(+1);
      refreshOutputs();
    }
    if (btnDown.pressed()) {
      stepRow(-1);
      refreshOutputs();
    }
    if (btnConfirm.pressed()) {
      doConfirm();
    }
    if (btnCarriage.pressed()) {
      onCarriagePulse();
    }
  }

  // ---- Detect changes made by Web UI and refresh outputs ----
  // Web UI updates cfg.activeRow via /api/row and changes cfg via /api/config.
  // This observer makes OLED + LEDs follow those changes.
  static int lastRow = -999;
  static uint32_t lastTot = 0;
  static bool lastWarn = false;
  static uint8_t lastBright = 255;
  static uint32_t lastCA = 0;
  static uint32_t lastCC = 0;
  static bool lastRB = false;
  static int lastH = -1;
  static int lastW = -1;

  bool changed =
    (cfg.activeRow != lastRow) ||
    (cfg.totalPulses != lastTot) ||
    (cfg.warnBlinkActive != lastWarn) ||
    (cfg.brightness != lastBright) ||
    (cfg.colorActive != lastCA) ||
    (cfg.colorConfirmed != lastCC) ||
    (cfg.rowFromBottom != lastRB) ||
    (pattern.h != lastH) ||
    (pattern.w != lastW);

  if (changed) {
    // Apply brightness change immediately
    leds.setBrightness(cfg.brightness);

    refreshOutputs();

    lastRow = cfg.activeRow;
    lastTot = cfg.totalPulses;
    lastWarn = cfg.warnBlinkActive;
    lastBright = cfg.brightness;
    lastCA = cfg.colorActive;
    lastCC = cfg.colorConfirmed;
    lastRB = cfg.rowFromBottom;
    lastH = pattern.h;
    lastW = pattern.w;
  }

  // ---- Blink warning handling ----
  static uint32_t lastBlinkMs = 0;
  static bool blinkOn = true;

  if (cfg.warnBlinkActive && cfg.blinkWarning) {
    uint32_t now = millis();
    if (now - lastBlinkMs > 300) {
      lastBlinkMs = now;
      blinkOn = !blinkOn;
      leds.blinkRow(pattern, cfg.activeRow, rowConfirmed[cfg.activeRow], cfg, blinkOn);
    }
  }

  delay(5);
}
