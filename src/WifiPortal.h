#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <functional>

struct WifiCreds {
  String ssid;
  String pass;
};

bool wifiConnectSTA(const WifiCreds& c, uint32_t timeoutMs);

void wifiStartPortal(
  WebServer& server,
  DNSServer& dns,
  const char* apSsid,
  WifiCreds& creds,
  std::function<void(const WifiCreds&)> onCredsSaved,
  std::function<void(const IPAddress&)> onConnected
);

void wifiStopPortal(DNSServer& dns);
