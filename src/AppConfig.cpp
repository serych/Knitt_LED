#include "AppConfig.h"
#include <Preferences.h>

static Preferences prefs;

void loadConfig(AppConfig& cfg) {
  prefs.begin("knittled", true);
  cfg.colorActive = (uint32_t)prefs.getUInt("cA", (unsigned int)cfg.colorActive);
  cfg.colorConfirmed = (uint32_t)prefs.getUInt("cC", (unsigned int)cfg.colorConfirmed);
  cfg.brightness = (uint8_t)prefs.getUChar("br", cfg.brightness);

  cfg.autoAdvance = prefs.getBool("aa", cfg.autoAdvance);
  cfg.blinkWarning = prefs.getBool("bw", cfg.blinkWarning);

  cfg.currentPatternFile = prefs.getString("file", cfg.currentPatternFile);
  cfg.activeRow = prefs.getInt("row", cfg.activeRow);
  cfg.rowFromBottom = prefs.getBool("rb", cfg.rowFromBottom);
  prefs.end();
}

void saveConfig(const AppConfig& cfg) {
  prefs.begin("knittled", false);
  prefs.putUInt("cA", (unsigned int)cfg.colorActive);
  prefs.putUInt("cC", (unsigned int)cfg.colorConfirmed);
  prefs.putUChar("br", cfg.brightness);

  prefs.putBool("aa", cfg.autoAdvance);
  prefs.putBool("bw", cfg.blinkWarning);

  prefs.putString("file", cfg.currentPatternFile);
  prefs.putInt("row", cfg.activeRow);
  prefs.putBool("rb", cfg.rowFromBottom);
  prefs.end();
}
