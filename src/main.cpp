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

static constexpr int PIN_BTN_UP        = 32;
static constexpr int PIN_BTN_DOWN      = 33;
static constexpr int PIN_BTN_CONFIRM   = 25;
static constexpr int PIN_SENSOR_CARRIAGE = 26;

static constexpr int PIN_NEOPIXEL = 27;
static constexpr int LED_COUNT    = 12;

static constexpr neoPixelType LED_TYPE = NEO_GRB + NEO_KHZ800;

// ============================================================
// ------------------------ GLOBALS -----------------------------
// ============================================================

WebServer server(80);
DNSServer dns;
Preferences prefs;

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

static void clampActiveRow() {
  if (cfg.activeRow < 0) cfg.activeRow = 0;
  if (cfg.activeRow >= pattern.h) cfg.activeRow = pattern.h - 1;
}

static void refreshOutputs() {
  clampActiveRow();
  oled.showKnitStatus(cfg.activeRow + 1, pattern.h, cfg.totalPulses);
  leds.showRow(pattern, cfg.activeRow, rowConfirmed[cfg.activeRow], cfg);
}

// ============================================================
// ------------------- KNITTING ACTIONS ------------------------
// ============================================================

static void doRowDelta(int delta) {
  cfg.warnBlinkActive = false;
  cfg.activeRow += delta;
  clampActiveRow();
  saveConfig(cfg);
  refreshOutputs();
}

static void doConfirm() {
  rowConfirmed[cfg.activeRow] = true;
  cfg.warnBlinkActive = false;

  if (cfg.autoAdvance && cfg.activeRow < pattern.h - 1) {
    cfg.activeRow++;
  }

  saveConfig(cfg);
  refreshOutputs();
}

static void onCarriagePulse() {
  cfg.totalPulses++;

  if (cfg.blinkWarning && !rowConfirmed[cfg.activeRow]) {
    cfg.warnBlinkActive = true;
  }

  cfg.activeRow++;
  clampActiveRow();
  saveConfig(cfg);
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

  // Load or create pattern
  if (!loadPatternFile(cfg.currentPatternFile, pattern)) {
    pattern.name = "default";
    pattern.w = 12;
    pattern.h = 24;
    savePatternFile("/patterns/default.json", pattern);
    cfg.currentPatternFile = "/patterns/default.json";
    saveConfig(cfg);
  }

  clampActiveRow();

  // LEDs
  leds.begin(cfg.brightness);

  // Buttons
  btnUp.begin(PIN_BTN_UP, true);
  btnDown.begin(PIN_BTN_DOWN, true);
  btnConfirm.begin(PIN_BTN_CONFIRM, true);
  btnCarriage.begin(PIN_SENSOR_CARRIAGE, true);

  // Try STA WiFi
  if (!wifiCreds.ssid.isEmpty() && wifiConnectSTA(wifiCreds, 12000)) {
    oled.showIp(WiFi.localIP().toString());
    startMainServer();
    delay(500);
    refreshOutputs();
    return;
  }

  // Fallback WiFi portal
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
      oled.showIp(ip.toString());

      wifiStopPortal(dns);

      server.stop();
      startMainServer();

      delay(500);
      refreshOutputs();
    }
  );
}

// ============================================================
// ---------------------------- LOOP ---------------------------
// ============================================================

void loop() {
  server.handleClient();
  dns.processNextRequest();

  // Hardware controls active only when connected
  if (WiFi.status() == WL_CONNECTED) {
    if (btnUp.pressed())        doRowDelta(+1);
    if (btnDown.pressed())      doRowDelta(-1);
    if (btnConfirm.pressed())   doConfirm();
    if (btnCarriage.pressed())  onCarriagePulse();
  }

  // Blink warning handling
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
