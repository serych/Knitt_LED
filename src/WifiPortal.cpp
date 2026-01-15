/**
 * @file WifiPortal.cpp
 * @brief Implementation of Wi-Fi provisioning portal.
 *
 * Uses WebServer + DNSServer to behave as a captive portal.
 * Network scanning is performed on demand when the portal root page is requested.
 */

#include "WifiPortal.h"
#include <WiFi.h>

static const byte DNS_PORT = 53;
static WifiCreds* portalCreds = nullptr;
static std::function<void(const IPAddress&)> onConnectedFn;
static std::function<void(const WifiCreds&)> onCredsSavedFn;
static bool saveRequested = false;
static bool connectRequested = false;

bool wifiConnectSTA(const WifiCreds& c, uint32_t timeoutMs) {
  if (c.ssid.isEmpty()) return false;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setAutoConnect(true);
  WiFi.disconnect(true, true);
  delay(200);
  Serial.printf("WiFi connect: ssid=%s\n", c.ssid.c_str());
  WiFi.begin(c.ssid.c_str(), c.pass.c_str());

  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) return true;
    delay(250);
  }
  Serial.printf("WiFi connect failed: status=%d\n", (int)WiFi.status());
  return false;
}

void wifiStopPortal(DNSServer& dns) {
  dns.stop();
  WiFi.softAPdisconnect(true);
}

static String esc(const String& s) {
  String out;
  out.reserve(s.length());
  for (char c : s) {
    switch (c) {
      case '&': out += F("&amp;"); break;
      case '<': out += F("&lt;"); break;
      case '>': out += F("&gt;"); break;
      case '"': out += F("&quot;"); break;
      case '\'': out += F("&#39;"); break;
      default: out += c; break;
    }
  }
  return out;
}

void wifiStartPortal(
  WebServer& server,
  DNSServer& dns,
  const char* apSsid,
  WifiCreds& creds,
  std::function<void(const WifiCreds&)> onCredsSaved,
  std::function<void(const IPAddress&)> onConnected
) {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apSsid);
  delay(200);

  portalCreds = &creds;
  onConnectedFn = onConnected;
  onCredsSavedFn = onCredsSaved;
  saveRequested = false;
  connectRequested = false;

  IPAddress apIP = WiFi.softAPIP();
  dns.start(DNS_PORT, "*", apIP);

  server.on("/", HTTP_GET, [&]() {
    int n = WiFi.scanNetworks(false, true);
    String html = "<!doctype html><html><head><meta charset='utf-8'>"
                  "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                  "<title>KnittLED setup</title>"
                  "<style>body{font-family:system-ui,Arial;margin:20px;max-width:680px}"
                  ".card{padding:14px;border:1px solid #ddd;border-radius:12px;margin:12px 0}"
                  "label{display:block;margin:10px 0 4px} input,select{width:100%;padding:10px;border-radius:10px;border:1px solid #ccc}"
                  "button{padding:12px 14px;border:0;border-radius:12px;background:#111;color:#fff;font-weight:600;width:100%;margin-top:12px}"
                  ".small{color:#555;font-size:13px}</style></head><body>";
    html += "<h1>KnittLED Wi-Fi Setup</h1>";
    html += "<div class='card small'>Connect to AP <b>KnittLED</b>, choose Wi-Fi, enter password.</div>";
    html += "<div class='card'><form method='POST' action='/save'>";
    html += "<label>SSID</label><select name='ssid' required><option value=''>-- Select --</option>";
    for (int i = 0; i < n; i++) {
      html += "<option value='" + esc(WiFi.SSID(i)) + "'>" + esc(WiFi.SSID(i)) + "</option>";
    }
    html += "</select>";
    html += "<label>Password</label><input name='pass' type='password' placeholder='(optional for open)'>";
    html += "<button type='submit'>Save & Connect</button></form></div></body></html>";
    server.send(200, "text/html; charset=utf-8", html);
  });

  server.on("/save", HTTP_POST, [&]() {
    creds.ssid = server.arg("ssid");
    creds.ssid.trim();
    creds.pass = server.arg("pass");

    Serial.printf("Portal save: ssid_len=%u pass_len=%u\n",
                  (unsigned)creds.ssid.length(),
                  (unsigned)creds.pass.length());
    Serial.println("Portal save: scheduling commit");
    Serial.flush();
    saveRequested = true;

    server.sendHeader("Location", "/");
    server.send(302);
  });

  // Captive portal helpers
  server.on("/generate_204", HTTP_GET, [&]() { server.sendHeader("Location", "/"); server.send(302); });
  server.on("/hotspot-detect.html", HTTP_GET, [&]() { server.sendHeader("Location", "/"); server.send(302); });
  server.on("/fwlink", HTTP_GET, [&]() { server.sendHeader("Location", "/"); server.send(302); });

  server.onNotFound([&]() { server.sendHeader("Location", "/"); server.send(302); });

  server.begin();
}

void wifiPortalLoop() {
  if (saveRequested && portalCreds != nullptr) {
    saveRequested = false;
    Serial.println("Portal save: committing");
    Serial.flush();
    if (onCredsSavedFn) {
      onCredsSavedFn(*portalCreds);
    }
    Serial.println("Portal save: committed");
    Serial.flush();
    connectRequested = true;
  }

  if (!connectRequested || portalCreds == nullptr) return;

  connectRequested = false;
  Serial.println("Portal connect: attempting STA");
  Serial.flush();

  bool ok = wifiConnectSTA(*portalCreds, 20000);
  if (ok) {
    Serial.println("Portal connect: STA ok");
    Serial.flush();
    if (onConnectedFn) {
      onConnectedFn(WiFi.localIP());
    }
  } else {
    Serial.println("Portal connect: STA failed");
    Serial.flush();
  }
}
