//
// options.h
//
// Reads key=value options from cmdline.txt on the SD card, and from
// bmcoco.cfg, where the BMCoco menu saves its settings. A setting in
// bmcoco.cfg wins over the same option in cmdline.txt.
//
// This file is part of BMCoco, a bare metal Tandy Color Computer emulator for the
// Raspberry Pi. See LICENSE for licensing details.
//

#ifndef BMCOCO_OPTIONS_H_
#define BMCOCO_OPTIONS_H_

#include <circle/bcmpropertytags.h>

#define OPTIONS_MAX 32
#define SETTINGS_MAX 48
#define SETTING_NAME_MAX 24
#define SETTING_VALUE_MAX 72

class Options {
public:
  Options(void);

  // Value of option NAME, or DEFAULT_VALUE if it was not given.
  const char *Get(const char *name, const char *default_value = 0) const;
  int GetInt(const char *name, int default_value) const;
  double GetDouble(const char *name, double default_value) const;
  bool GetBool(const char *name, bool default_value) const;

  // Reads the settings file (key=value lines, # comments). Call once the SD
  // card is mounted.
  void LoadSettings(const char *path);
  // Changes a setting (in memory; SaveSettings writes them all out).
  bool Set(const char *name, const char *value);
  bool SaveSettings(void);

  static Options *Get(void);

private:
  int FindSetting(const char *name) const;

  char m_SettingsPath[64];
  int m_nSettings;
  char m_SettingName[SETTINGS_MAX][SETTING_NAME_MAX];
  char m_SettingValue[SETTINGS_MAX][SETTING_VALUE_MAX];

  TPropertyTagCommandLine m_TagCommandLine;
  int m_nCount;
  const char *m_pName[OPTIONS_MAX];
  const char *m_pValue[OPTIONS_MAX];

  static Options *s_pThis;
};

#endif
