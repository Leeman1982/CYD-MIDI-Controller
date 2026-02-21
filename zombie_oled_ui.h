#ifndef ZOMBIE_OLED_UI_H
#define ZOMBIE_OLED_UI_H

#include <U8g2lib.h>
#include <Wire.h>

// ST7567S 128x64 I2C OLED Display
// I2C pins (default for ESP32):
// - SDA: GPIO 21
// - SCL: GPIO 22

// Display modes for OLED interface
enum OLEDMode {
  OLED_MENU,
  OLED_SYNTH,
  OLED_ARP,
  OLED_SEQ,
  OLED_PRESET
};

// Synth parameter pages
enum SynthPage {
  PAGE_OSC,      // Oscillator waveforms
  PAGE_FILTER,   // Filter cutoff, resonance, type
  PAGE_ENV_AMP,  // Amplitude envelope
  PAGE_ENV_FILT, // Filter envelope
  PAGE_LFO,      // LFO settings
  PAGE_MASTER    // Master volume, tuning
};

// Display instance for ST7567S
U8G2_ST7567_ENH_DG128064I_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

class ZombieOLEDUI {
private:
  OLEDMode currentMode;
  SynthPage currentPage;
  int menuSelection;
  int paramSelection;
  bool needsRedraw;

  // Menu items
  static const int menuItemCount = 4;
  const char* menuItems[menuItemCount] = {
    "SYNTH",
    "ARPEGGIATOR",
    "SEQUENCER",
    "PRESETS"
  };

  // Parameter names for each synth page
  static const int maxParamsPerPage = 4;
  const char* oscParams[4] = {"OSC1 WAVE", "OSC2 WAVE", "OSC2 DET", "OSC2 SEMI"};
  const char* filterParams[4] = {"CUTOFF", "RESONANCE", "TYPE", "ENV AMT"};
  const char* envAmpParams[4] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
  const char* envFiltParams[4] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
  const char* lfoParams[4] = {"RATE", "DEPTH", "TARGET", "WAVEFORM"};
  const char* masterParams[4] = {"VOLUME", "VOICES", "OCTAVE", "FINE TUNE"};

public:
  ZombieOLEDUI() : currentMode(OLED_MENU), currentPage(PAGE_OSC),
                   menuSelection(0), paramSelection(0), needsRedraw(true) {}

  void init() {
    u8g2.begin();
    u8g2.setContrast(255);
    u8g2.setFont(u8g2_font_6x10_tr);
    drawSplash();
  }

  void drawSplash() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_helvB12_tr);
    u8g2.drawStr(20, 25, "ZOMBI SS");
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(15, 40, "Synthesizer");
    u8g2.drawStr(25, 55, "Starting...");
    u8g2.sendBuffer();
  }

  void setMode(OLEDMode mode) {
    currentMode = mode;
    menuSelection = 0;
    paramSelection = 0;
    needsRedraw = true;
  }

  void setPage(SynthPage page) {
    currentPage = page;
    paramSelection = 0;
    needsRedraw = true;
  }

  void drawMenu() {
    u8g2.clearBuffer();

    // Header
    u8g2.drawBox(0, 0, 128, 12);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.drawStr(30, 9, "ZOMBI SS");
    u8g2.setDrawColor(1);

    // Menu items
    u8g2.setFont(u8g2_font_6x10_tr);
    int y = 20;
    for (int i = 0; i < menuItemCount; i++) {
      if (i == menuSelection) {
        u8g2.drawBox(2, y - 1, 124, 11);
        u8g2.setDrawColor(0);
      }

      u8g2.drawStr(10, y + 8, menuItems[i]);

      if (i == menuSelection) {
        u8g2.setDrawColor(1);
      }
      y += 12;
    }

    u8g2.sendBuffer();
  }

  void drawSynthMode(int activeVoices, float masterVol) {
    u8g2.clearBuffer();

    // Header with page name
    u8g2.drawBox(0, 0, 128, 10);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_6x10_tr);

    const char* pageName = "";
    switch (currentPage) {
      case PAGE_OSC: pageName = "OSCILLATOR"; break;
      case PAGE_FILTER: pageName = "FILTER"; break;
      case PAGE_ENV_AMP: pageName = "AMP ENV"; break;
      case PAGE_ENV_FILT: pageName = "FILT ENV"; break;
      case PAGE_LFO: pageName = "LFO"; break;
      case PAGE_MASTER: pageName = "MASTER"; break;
    }
    u8g2.drawStr(35, 8, pageName);
    u8g2.setDrawColor(1);

    // Status bar
    u8g2.setFont(u8g2_font_5x7_tr);
    char buf[16];
    sprintf(buf, "V:%d/8", activeVoices);
    u8g2.drawStr(2, 20, buf);
    sprintf(buf, "VOL:%d%%", (int)(masterVol * 100));
    u8g2.drawStr(70, 20, buf);

    // Get current page parameters
    const char** params = nullptr;
    switch (currentPage) {
      case PAGE_OSC: params = oscParams; break;
      case PAGE_FILTER: params = filterParams; break;
      case PAGE_ENV_AMP: params = envAmpParams; break;
      case PAGE_ENV_FILT: params = envFiltParams; break;
      case PAGE_LFO: params = lfoParams; break;
      case PAGE_MASTER: params = masterParams; break;
    }

    // Draw parameters (4 per page)
    u8g2.setFont(u8g2_font_6x10_tr);
    int y = 30;
    for (int i = 0; i < maxParamsPerPage; i++) {
      if (i == paramSelection) {
        u8g2.drawFrame(0, y - 1, 128, 11);
      }

      u8g2.drawStr(4, y + 8, params[i]);
      // Value will be drawn here (implementation in main sketch)
      y += 12;
    }

    // Footer instructions
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(2, 62, "MODE:Page ENC:Val");

    u8g2.sendBuffer();
  }

  void drawArpMode(bool playing, int bpm, int pattern) {
    u8g2.clearBuffer();

    // Header
    u8g2.drawBox(0, 0, 128, 10);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(20, 8, "ARPEGGIATOR");
    u8g2.setDrawColor(1);

    // Status
    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.drawStr(10, 25, playing ? "PLAYING" : "STOPPED");

    // Parameters
    u8g2.setFont(u8g2_font_6x10_tr);
    char buf[32];

    sprintf(buf, "BPM: %d", bpm);
    u8g2.drawStr(10, 38, buf);

    sprintf(buf, "PATTERN: %d", pattern);
    u8g2.drawStr(10, 50, buf);

    // Footer
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(2, 62, "PLAY:Toggle");

    u8g2.sendBuffer();
  }

  void drawSeqMode(bool playing, int bpm, int currentStep) {
    u8g2.clearBuffer();

    // Header
    u8g2.drawBox(0, 0, 128, 10);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(25, 8, "SEQUENCER");
    u8g2.setDrawColor(1);

    // Status
    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.drawStr(10, 22, playing ? "PLAYING" : "STOPPED");

    // BPM
    char buf[32];
    sprintf(buf, "BPM: %d", bpm);
    u8g2.drawStr(10, 35, buf);

    // Step display (16 steps, 8 pixels each)
    int stepY = 42;
    for (int i = 0; i < 16; i++) {
      int x = 2 + i * 8;
      if (i == currentStep && playing) {
        u8g2.drawBox(x, stepY, 6, 6);
      } else {
        u8g2.drawFrame(x, stepY, 6, 6);
      }
    }

    // Footer
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(2, 62, "PLAY:Start/Stop");

    u8g2.sendBuffer();
  }

  void drawPresetMode(int presetNum, const char* presetName) {
    u8g2.clearBuffer();

    // Header
    u8g2.drawBox(0, 0, 128, 10);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(35, 8, "PRESETS");
    u8g2.setDrawColor(1);

    // Preset number
    u8g2.setFont(u8g2_font_helvB14_tr);
    char buf[16];
    sprintf(buf, "#%d", presetNum);
    u8g2.drawStr(50, 30, buf);

    // Preset name
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(20, 45, presetName);

    // Footer
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(2, 62, "ENC:Select SW:Load");

    u8g2.sendBuffer();
  }

  void drawParameterValue(int y, float value, int min, int max, const char* suffix = "") {
    char buf[16];
    sprintf(buf, "%d%s", (int)map(value * 100, 0, 100, min, max), suffix);
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(90, y + 8, buf);
  }

  // Navigation
  void menuDown() {
    menuSelection = (menuSelection + 1) % menuItemCount;
    needsRedraw = true;
  }

  void menuUp() {
    menuSelection = (menuSelection - 1 + menuItemCount) % menuItemCount;
    needsRedraw = true;
  }

  void paramNext() {
    paramSelection = (paramSelection + 1) % maxParamsPerPage;
    needsRedraw = true;
  }

  void paramPrev() {
    paramSelection = (paramSelection - 1 + maxParamsPerPage) % maxParamsPerPage;
    needsRedraw = true;
  }

  void pageNext() {
    currentPage = (SynthPage)((currentPage + 1) % 6);
    paramSelection = 0;
    needsRedraw = true;
  }

  void pagePrev() {
    currentPage = (SynthPage)((currentPage - 1 + 6) % 6);
    paramSelection = 0;
    needsRedraw = true;
  }

  // Getters
  OLEDMode getMode() { return currentMode; }
  SynthPage getPage() { return currentPage; }
  int getMenuSelection() { return menuSelection; }
  int getParamSelection() { return paramSelection; }
  bool needsUpdate() { return needsRedraw; }
  void setNeedsRedraw(bool val) { needsRedraw = val; }
};

#endif
