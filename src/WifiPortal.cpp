#include "WifiPortal.h"
#include <WiFi.h>

static const byte DNS_PORT = 53;

bool wifiConnectSTA(const WifiCreds& c, uint32_t timeoutMs) {
  if (c.ssid.isEmpty()) return false;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(200);
  WiFi.begin(c.ssid.c_str(), c.pass.c_str());

  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) return true;
    delay(250);
  }
  return false;
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

void wifiStartPortal(WebServer& server, DNSServer& dns, const char* apSsid,
                     WifiCreds& creds,
                     void (*onCredsSaved)(const WifiCreds&),
                     void (*onConnected)(const IPAddress&)) {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apSsid);
  delay(200);

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
    for (int i=0;i<n;i++) html += "<option value='" + esc(WiFi.SSID(i)) + "'>" + esc(WiFi.SSID(i)) + "</option>";
    html += "</select>";
    html += "<label>Password</label><input name='pass' type='password' placeholder='(optional for open)'>";
    html += "<button type='submit'>Save & Connect</button></form></div></body></html>";
    server.send(200, "text/html; charset=utf-8", html);
  });

  server.on("/save", HTTP_POST, [&]() {
    creds.ssid = server.arg("ssid"); creds.ssid.trim();
    creds.pass = server.arg("pass");
    onCredsSaved(creds);

    bool ok = wifiConnectSTA(creds, 15000);
    if (ok) {
      onConnected(WiFi.localIP());
      server.sendHeader("Location", "http://" + WiFi.localIP().toString() + "/");
      server.send(302);
    } else {
      server.sendHeader("Location", "/");
      server.send(302);
    }
  });

  // captive portal helpers
  server.on("/generate_204", HTTP_GET, [&](){ server.sendHeader("Location","/"); server.send(302); });
  server.on("/hotspot-detect.html", HTTP_GET, [&](){ server.sendHeader("Location","/"); server.send(302); });
  server.on("/fwlink", HTTP_GET, [&](){ server.sendHeader("Location","/"); server.send(302); });

  server.onNotFound([&](){ server.sendHeader("Location","/"); server.send(302); });

  server.begin();
}
