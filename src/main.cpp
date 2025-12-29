#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <Wire.h>
#include <U8g2lib.h>

#include "AppConfig.h"
#include "Pattern.h"
#include "LedView.h"
#include "OledView.h"
#include "Buttons.h"
#include "WebUi.h"
#include "WifiPortal.h"

// ------------------- YOU will choose pins later -------------------
// For now: pick safe defaults (ESP32 DevKit typical)
// You can change these once you decide your PCB/wiring.
static constexpr int PIN_SDA = 21;
static constexpr int PIN_SCL = 22;

static constexpr int PIN_BTN_UP = 32;
static constexpr int PIN_BTN_DOWN = 33;
static constexpr int PIN_BTN_CONFIRM = 25;
static constexpr int PIN_SENSOR_CARRIAGE = 26;

static constexpr int PIN_NEOPIXEL = 27;
static constexpr int LED_COUNT = 12; // physical LEDs on strip (default 12)

// LED type (most common)
static constexpr neoPixelType LED_TYPE = NEO_GRB + NEO_KHZ800;

// ------------------- Globals -------------------
WebServer server(80);
DNSServer dns;
Preferences prefs;

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
OledView oled(u8g2);

LedView leds(LED_COUNT, PIN_NEOPIXEL, LED_TYPE);

EdgeButton btnUp(60), btnDown(60), btnConfirm(60), btnCarriage(40);

AppConfig cfg;
Pattern pattern;
bool rowConfirmed[MAX_H]{};

WifiCreds wifiCreds;

// Declarations from AppConfig.cpp
void loadConfig(AppConfig& cfg);
void saveConfig(const AppConfig& cfg);

// WiFi creds persist (in same namespace "knittled")
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
    // nothing better to do here; you’ll see it on Serial
    Serial.println("LittleFS mount failed");
  }
  if (!LittleFS.exists("/patterns")) LittleFS.mkdir("/patterns");
}

// Keep activeRow in range
static void clampRow() {
  if (cfg.activeRow < 0) cfg.activeRow = 0;
  if (cfg.activeRow >= pattern.h) cfg.activeRow = pattern.h - 1;
}

// Apply LEDs + OLED for current state
static void refreshOutputs() {
  clampRow();
  oled.showKnitStatus(cfg.activeRow + 1, pattern.h, cfg.totalPulses);

  // blinking is handled in loop; here we show steady
  leds.showRow(pattern, cfg.activeRow, rowConfirmed[cfg.activeRow], cfg);
}

// Called when carriage pulse occurs (sensor)
static void onCarriagePulse() {
  cfg.totalPulses++;

  // If current row not confirmed and blinkWarning enabled => start warning blink
  if (cfg.blinkWarning && !rowConfirmed[cfg.activeRow]) {
    cfg.warnBlinkActive = true;
  }

  // Acts like "UP"
  cfg.activeRow++;
  clampRow();
  saveConfig(cfg);
  refreshOutputs();
}

// Confirm button
static void doConfirm() {
  rowConfirmed[cfg.activeRow] = true;
  cfg.warnBlinkActive = false;

  if (cfg.autoAdvance && cfg.activeRow < pattern.h - 1) cfg.activeRow++;
  saveConfig(cfg);
  refreshOutputs();
}

// Row +/- buttons
static void doRowDelta(int delta) {
  cfg.warnBlinkActive = false;
  cfg.activeRow += delta;
  clampRow();
  saveConfig(cfg);
  refreshOutputs();
}

static void startMainServer() {
  WebUiDeps deps;
  deps.server = &server;
  deps.pattern = &pattern;
  deps.cfg = &cfg;
  deps.rowConfirmed = rowConfirmed;

  webuiBegin(deps);
  server.begin();
}

void setup() {
  Serial.begin(115200);

  // OLED
  Wire.begin(PIN_SDA, PIN_SCL);
  oled.begin();

  // FS
  ensureFS();

  // Load config & wifi creds
  loadConfig(cfg);
  loadWifiCreds();

  // Load or create default pattern
  if (!loadPatternFile(cfg.currentPatternFile, pattern)) {
    pattern.name = "default";
    pattern.w = 12; pattern.h = 24;
    savePatternFile("/patterns/default.json", pattern);
    cfg.currentPatternFile = "/patterns/default.json";
    saveConfig(cfg);
  }
  clampRow();

  // LEDs
  leds.begin(cfg.brightness);

  // Buttons
  btnUp.begin(PIN_BTN_UP, true);
  btnDown.begin(PIN_BTN_DOWN, true);
  btnConfirm.begin(PIN_BTN_CONFIRM, true);
  btnCarriage.begin(PIN_SENSOR_CARRIAGE, true);

  // Try STA
  if (!wifiCreds.ssid.isEmpty() && wifiConnectSTA(wifiCreds, 12000)) {
    oled.showIp(WiFi.localIP().toString());
    startMainServer();
    delay(600);
    refreshOutputs();
    return;
  }

  // Fallback provisioning portal
  oled.showIp("AP: KnittLED");
  wifiStartPortal(
    server, dns, "KnittLED", wifiCreds,
    [](const WifiCreds& c){ saveWifiCreds(c); },
    [&](const IPAddress& ip){
      // when connected
      oled.showIp(ip.toString());
      // swap to main server
      server.stop(); // stop portal routes (simple approach)
      server = WebServer(80); // re-init server instance
      startMainServer();
      delay(600);
      refreshOutputs();
    }
  );
}

void loop() {
  server.handleClient();
  dns.processNextRequest();

  // ---- Hardware buttons ----
  if (WiFi.status() == WL_CONNECTED) {
    if (btnUp.pressed()) doRowDelta(+1);
    if (btnDown.pressed()) doRowDelta(-1);
    if (btnConfirm.pressed()) doConfirm();
    if (btnCarriage.pressed()) onCarriagePulse();
  }

  // ---- Warning blink ----
  static uint32_t lastBlink = 0;
  static bool blinkOn = true;

  if (cfg.warnBlinkActive && cfg.blinkWarning) {
    uint32_t now = millis();
    if (now - lastBlink > 300) {
      lastBlink = now;
      blinkOn = !blinkOn;
      leds.blinkRow(pattern, cfg.activeRow, rowConfirmed[cfg.activeRow], cfg, blinkOn);
    }
  } else {
    // Keep steady output (avoid calling too often)
  }

  delay(5);
}
