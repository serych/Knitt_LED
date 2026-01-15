/**
 * @file WifiPortal.h
 * @brief Wi-Fi provisioning portal (fallback AP + captive DNS).
 *
 * If STA connection fails, the device starts an AP (SSID KnittLED) and serves a simple setup page.
 * After successful connection, the app reboots into STA mode for a clean server state.
 */

#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <functional>

/**
 * @brief Stored Wi-Fi credentials.
 */
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

// Call regularly from loop() while the portal is active.
void wifiPortalLoop();

void wifiStopPortal(DNSServer& dns);
