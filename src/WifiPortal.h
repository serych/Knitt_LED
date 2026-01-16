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

/** @brief Connect to a Wi-Fi network in STA mode. */
bool wifiConnectSTA(const WifiCreds& c, uint32_t timeoutMs);

/**
 * @brief Start the captive portal for provisioning and call back on save/connect.
 */
void wifiStartPortal(
  WebServer& server,
  DNSServer& dns,
  const char* apSsid,
  WifiCreds& creds,
  std::function<void(const WifiCreds&)> onCredsSaved,
  std::function<void(const IPAddress&)> onConnected
);

/** @brief Call regularly from loop() while the portal is active. */
void wifiPortalLoop();

/** @brief Stop the captive portal and AP. */
void wifiStopPortal(DNSServer& dns);
