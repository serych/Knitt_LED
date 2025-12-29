#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "Pattern.h"
#include "AppConfig.h"

struct WebUiDeps {
  WebServer* server;
  Pattern* pattern;
  AppConfig* cfg;
  bool* rowConfirmed;        // array [MAX_H]
};

void webuiBegin(WebUiDeps deps);
String listPatternFilesJson();
bool loadPatternFile(const String& path, Pattern& p);
bool savePatternFile(const String& path, const Pattern& p);
