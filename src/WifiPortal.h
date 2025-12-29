#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>

struct WifiCreds {
  String ssid;
  String pass;
};

bool wifiConnectSTA(const WifiCreds& c, uint32_t timeoutMs);
void wifiStartPortal(WebServer& server, DNSServer& dns, const char* apSsid,
                     WifiCreds& creds,
                     void (*onCredsSaved)(const WifiCreds&),
                     void (*onConnected)(const IPAddress&));
