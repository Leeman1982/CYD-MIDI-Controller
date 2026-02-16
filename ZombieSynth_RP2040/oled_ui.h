#ifndef OLED_UI_H
#define OLED_UI_H

// ═══════════════════════════════════════════════════════════════════════════════
// ZOMBIE SS – OLED Menu UI (RP2040 Port)
// ═══════════════════════════════════════════════════════════════════════════════
//
// Complete menu-driven UI for 128×64 OLED display with 8-button input.
// Replaces the CYD touchscreen interface with hierarchical menus.
//
// Navigation:
//   UP/DOWN     – Navigate menu items / adjust values
//   LEFT/RIGHT  – Cycle options / switch pages / fine adjust
//   CENTER      – Select / Enter submenu / Toggle
//   A (BACK)    – Go back to parent menu
//   B (FN)      – Hold for secondary functions (shift)
//   C (PLAY)    – Quick play/stop for arp/seq
//
// Display layout (128×64, using 6×8 font = 21 chars × 8 lines):
//   Line 0: Mode title + status (inverse)
//   Line 1-6: Menu items (6 visible, cursor ►)
//   Line 7: Status bar / button hints
// ═══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "config.h"
#include "button_input.h"

// Forward declarations for globals defined in .ino
extern SynthEngine     synth;
extern ZombieEffects   fx;
extern Arpeggiator     arp;
extern ZombieSequencer seq;
extern LFOEngine       globalLFO;
extern PresetManager   presetMgr;
extern MIDIOutput      midiOut;
extern ButtonInput     buttons;
extern int             lastPlayedMidiNote;
extern AppMode         currentMode;
extern void            exitToMenu();
extern void            loadPresetToSynth(int slot);
extern void            saveCurrentToPreset(int slot);

// ── OLED Display Object ─────────────────────────────────────────────────────
// SH1106 128×64 I2C – change constructor if using SSD1306 or different size
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// ── UI State ────────────────────────────────────────────────────────────────
static int  uiCursor      = 0;      // Current menu cursor position
static int  uiScrollTop   = 0;      // First visible item index
static int  uiItemCount   = 0;      // Total items in current view
static bool uiNeedsRedraw = true;
static int  uiSubPage     = 0;      // Sub-page within a mode

// Synth sub-pages
enum SynthPage {
  SPAGE_OSC = 0,
  SPAGE_FILTER,
  SPAGE_AMP_ENV,
  SPAGE_FILT_ENV,
  SPAGE_LFO,
  SPAGE_FX_CHORUS,
  SPAGE_FX_DELAY,
  SPAGE_FX_REVERB,
  SPAGE_COUNT
};

static const char* synthPageNames[] = {
  "OSC", "FILTER", "AMP ENV", "FLT ENV", "LFO",
  "CHORUS", "DELAY", "REVERB"
};

// Seq sub-pages
enum SeqPage {
  SEQP_GRID = 0,
  SEQP_STEP_EDIT,
  SEQP_TRACK_CFG,
  SEQP_SCALE,
  SEQP_COUNT
};

// Chord state
static int  chordRoot      = 0;
static int  chordType      = 0;
static int  chordOctave    = 4;
static bool chordHold      = false;
static int  chordActiveNotes[5];
static int  chordActiveCount = 0;

// Chord type definitions
struct ChordTypeDef {
  const char* name;
  int intervals[5];
  int numNotes;
};

static const ChordTypeDef chordTypes[] = {
  {"MAJ",  {0, 4, 7, -1, -1}, 3},
  {"MIN",  {0, 3, 7, -1, -1}, 3},
  {"7",    {0, 4, 7, 10, -1}, 4},
  {"MAJ7", {0, 4, 7, 11, -1}, 4},
  {"MIN7", {0, 3, 7, 10, -1}, 4},
  {"DIM",  {0, 3, 6, -1, -1}, 3},
  {"AUG",  {0, 4, 8, -1, -1}, 3},
  {"SUS4", {0, 5, 7, -1, -1}, 3},
};
static const int NUM_CHORD_TYPES = 8;

// Seq step cursor
static int seqStepCursor = 0;

// Preset text entry
static char presetNameBuf[15];
static int  presetNamePos = 0;
static bool presetNaming  = false;

// ── Helpers ─────────────────────────────────────────────────────────────────
#define VISIBLE_LINES 6
#define CHAR_W        6
#define CHAR_H        8
#define SCREEN_W      128
#define SCREEN_H      64

static void uiClampScroll() {
  if (uiCursor < 0) uiCursor = 0;
  if (uiCursor >= uiItemCount) uiCursor = uiItemCount - 1;
  if (uiCursor < 0) uiCursor = 0;
  if (uiCursor < uiScrollTop) uiScrollTop = uiCursor;
  if (uiCursor >= uiScrollTop + VISIBLE_LINES) uiScrollTop = uiCursor - VISIBLE_LINES + 1;
  if (uiScrollTop < 0) uiScrollTop = 0;
}

static void uiResetCursor() {
  uiCursor = 0;
  uiScrollTop = 0;
  uiNeedsRedraw = true;
}

// Draw inverse header bar
static void drawHeader(const char* title) {
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, SCREEN_W, 10);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(2, 8, title);

  // Show last played note (top-right)
  if (lastPlayedMidiNote >= 0) {
    char nb[6];
    snprintf(nb, sizeof(nb), "%s%d", midiNoteName(lastPlayedMidiNote), midiNoteOctave(lastPlayedMidiNote));
    int w = u8g2.getStrWidth(nb);
    u8g2.drawStr(SCREEN_W - w - 2, 8, nb);
  }
  u8g2.setDrawColor(1);
}

// Draw status bar at bottom
static void drawStatus(const char* text) {
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(2, 63, text);
}

// Draw a menu item line
static void drawMenuItem(int screenLine, int itemIdx, const char* label, const char* value, bool selected) {
  int y = 11 + screenLine * 9;
  if (selected) {
    u8g2.drawBox(0, y - 1, SCREEN_W, 9);
    u8g2.setDrawColor(0);
  }
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(2, y + 6, label);
  if (value && value[0]) {
    int vw = u8g2.getStrWidth(value);
    u8g2.drawStr(SCREEN_W - vw - 2, y + 6, value);
  }
  if (selected) u8g2.setDrawColor(1);
}

// Draw a horizontal bar graph
static void drawBar(int x, int y, int w, int h, float value) {
  u8g2.drawFrame(x, y, w, h);
  int fill = (int)(value * (w - 2));
  if (fill > 0) u8g2.drawBox(x + 1, y + 1, fill, h - 2);
}

// Adjust a float value with LEFT/RIGHT
static bool adjustFloat(float& val, float minV, float maxV, float step, float fineStep) {
  bool fn = buttons.isDown(BTN_B);
  float s = fn ? fineStep : step;
  if (buttons.pressOrRepeat(BTN_RIGHT)) { val = constrain(val + s, minV, maxV); uiNeedsRedraw = true; return true; }
  if (buttons.pressOrRepeat(BTN_LEFT))  { val = constrain(val - s, minV, maxV); uiNeedsRedraw = true; return true; }
  return false;
}

// Adjust an int value with LEFT/RIGHT
static bool adjustInt(int& val, int minV, int maxV, int step) {
  if (buttons.pressOrRepeat(BTN_RIGHT)) { val = constrain(val + step, minV, maxV); uiNeedsRedraw = true; return true; }
  if (buttons.pressOrRepeat(BTN_LEFT))  { val = constrain(val - step, minV, maxV); uiNeedsRedraw = true; return true; }
  return false;
}

// Cycle an int value with LEFT/RIGHT (wraps around)
static bool cycleInt(int& val, int count) {
  if (buttons.pressed(BTN_RIGHT)) { val = (val + 1) % count; uiNeedsRedraw = true; return true; }
  if (buttons.pressed(BTN_LEFT))  { val = (val - 1 + count) % count; uiNeedsRedraw = true; return true; }
  return false;
}

// ═════════════════════════════════════════════════════════════════════════════
// MAIN MENU
// ═════════════════════════════════════════════════════════════════════════════
static void drawMenuMode() {
  u8g2.clearBuffer();
  drawHeader("ZOMBIE SS PROPHET");

  static const char* menuItems[] = {"SYNTH", "ARP", "SEQ", "PRESETS", "CHORDS"};
  static const AppMode menuModes[] = {MODE_SYNTH, MODE_ARP, MODE_SEQ, MODE_PRESETS, MODE_CHORD};
  uiItemCount = 5;
  uiClampScroll();

  for (int i = 0; i < VISIBLE_LINES && (uiScrollTop + i) < uiItemCount; i++) {
    int idx = uiScrollTop + i;
    char val[8];
    switch (idx) {
      case 0: snprintf(val, sizeof(val), "%dV", synth.getActiveVoiceCount()); break;
      case 1: snprintf(val, sizeof(val), "%s", arp.getIsPlaying() ? "PLAY" : ""); break;
      case 2: snprintf(val, sizeof(val), "%s", seq.getIsPlaying() ? "PLAY" : ""); break;
      default: val[0] = 0; break;
    }
    drawMenuItem(i, idx, menuItems[idx], val, idx == uiCursor);
  }

  drawStatus("[SEL]Enter [C]Play");
  u8g2.sendBuffer();
}

static void handleMenuInput() {
  static const AppMode menuModes[] = {MODE_SYNTH, MODE_ARP, MODE_SEQ, MODE_PRESETS, MODE_CHORD};

  if (buttons.pressOrRepeat(BTN_DOWN)) { uiCursor++; uiNeedsRedraw = true; }
  if (buttons.pressOrRepeat(BTN_UP))   { uiCursor--; uiNeedsRedraw = true; }

  if (buttons.pressed(BTN_CENTER)) {
    if (uiCursor >= 0 && uiCursor < 5) {
      currentMode = menuModes[uiCursor];
      uiResetCursor();
      uiSubPage = 0;
    }
  }
}

// ═════════════════════════════════════════════════════════════════════════════
// SYNTH MODE – 8 parameter pages
// ═════════════════════════════════════════════════════════════════════════════
static void drawSynthMode() {
  u8g2.clearBuffer();

  char hdr[22];
  snprintf(hdr, sizeof(hdr), "SYNTH>%s", synthPageNames[uiSubPage]);
  drawHeader(hdr);

  char val[16];

  switch ((SynthPage)uiSubPage) {
    case SPAGE_OSC: {
      uiItemCount = 6;
      uiClampScroll();
      const char* labels[] = {"O1 Wave", "O1 Level", "O2 Wave", "O2 Level", "Detune", "Semi"};
      for (int i = 0; i < VISIBLE_LINES && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "%s", waveformNames[synth.getOsc1Wave()]); break;
          case 1: snprintf(val, sizeof(val), "%d%%", (int)(synth.getOsc1Level() * 100)); break;
          case 2: snprintf(val, sizeof(val), "%s", waveformNames[synth.getOsc2Wave()]); break;
          case 3: snprintf(val, sizeof(val), "%d%%", (int)(synth.getOsc2Level() * 100)); break;
          case 4: snprintf(val, sizeof(val), "%d%%", (int)(synth.getOsc2Detune() / 0.02f * 100)); break;
          case 5: snprintf(val, sizeof(val), "%+d", (int)synth.getOsc2Semitones()); break;
        }
        drawMenuItem(i, idx, labels[idx], val, idx == uiCursor);
      }
      break;
    }

    case SPAGE_FILTER: {
      uiItemCount = 4;
      uiClampScroll();
      const char* labels[] = {"Type", "Cutoff", "Reso", "Env Amt"};
      for (int i = 0; i < VISIBLE_LINES && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "%s", filterTypeNames[synth.getFilterType()]); break;
          case 1: snprintf(val, sizeof(val), "%d%%", (int)(synth.getFilterCutoff() * 100)); break;
          case 2: snprintf(val, sizeof(val), "%d%%", (int)(synth.getFilterResonance() * 100)); break;
          case 3: snprintf(val, sizeof(val), "%d%%", (int)(synth.getFilterEnvAmount() * 100)); break;
        }
        drawMenuItem(i, idx, labels[idx], val, idx == uiCursor);
      }
      break;
    }

    case SPAGE_AMP_ENV: {
      uiItemCount = 4;
      uiClampScroll();
      const char* labels[] = {"Attack", "Decay", "Sustain", "Release"};
      for (int i = 0; i < VISIBLE_LINES && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "%dms", (int)(synth.getAmpAttack() * 1000)); break;
          case 1: snprintf(val, sizeof(val), "%dms", (int)(synth.getAmpDecay() * 1000)); break;
          case 2: snprintf(val, sizeof(val), "%d%%", (int)(synth.getAmpSustain() * 100)); break;
          case 3: snprintf(val, sizeof(val), "%dms", (int)(synth.getAmpRelease() * 1000)); break;
        }
        drawMenuItem(i, idx, labels[idx], val, idx == uiCursor);
      }
      break;
    }

    case SPAGE_FILT_ENV: {
      uiItemCount = 4;
      uiClampScroll();
      const char* labels[] = {"Attack", "Decay", "Sustain", "Release"};
      for (int i = 0; i < VISIBLE_LINES && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "%dms", (int)(synth.getFilterAttack() * 1000)); break;
          case 1: snprintf(val, sizeof(val), "%dms", (int)(synth.getFilterDecay() * 1000)); break;
          case 2: snprintf(val, sizeof(val), "%d%%", (int)(synth.getFilterSustain() * 100)); break;
          case 3: snprintf(val, sizeof(val), "%dms", (int)(synth.getFilterRelease() * 1000)); break;
        }
        drawMenuItem(i, idx, labels[idx], val, idx == uiCursor);
      }
      break;
    }

    case SPAGE_LFO: {
      uiItemCount = 5;
      uiClampScroll();
      const char* labels[] = {"Enable", "Wave", "Target", "Rate", "Depth"};
      for (int i = 0; i < VISIBLE_LINES && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "%s", globalLFO.enabled ? "ON" : "OFF"); break;
          case 1: snprintf(val, sizeof(val), "%s", lfoWaveNames[globalLFO.wave]); break;
          case 2: snprintf(val, sizeof(val), "%s", lfoTargetNames[globalLFO.target]); break;
          case 3: snprintf(val, sizeof(val), "%.1fHz", globalLFO.rate); break;
          case 4: snprintf(val, sizeof(val), "%d%%", (int)(globalLFO.depth * 100)); break;
        }
        drawMenuItem(i, idx, labels[idx], val, idx == uiCursor);
      }
      break;
    }

    case SPAGE_FX_CHORUS: {
      uiItemCount = 4;
      uiClampScroll();
      const char* labels[] = {"Enable", "Rate", "Depth", "Mix"};
      for (int i = 0; i < VISIBLE_LINES && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "%s", fx.chorus.enabled ? "ON" : "OFF"); break;
          case 1: snprintf(val, sizeof(val), "%.1fHz", fx.chorus.rate); break;
          case 2: snprintf(val, sizeof(val), "%d%%", (int)(fx.chorus.depth / 0.01f * 100)); break;
          case 3: snprintf(val, sizeof(val), "%d%%", (int)(fx.chorus.mix * 100)); break;
        }
        drawMenuItem(i, idx, labels[idx], val, idx == uiCursor);
      }
      break;
    }

    case SPAGE_FX_DELAY: {
      uiItemCount = 4;
      uiClampScroll();
      const char* labels[] = {"Enable", "Time", "Feedback", "Mix"};
      for (int i = 0; i < VISIBLE_LINES && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "%s", fx.delay.enabled ? "ON" : "OFF"); break;
          case 1: snprintf(val, sizeof(val), "%dms", (int)fx.delay.delayMs); break;
          case 2: snprintf(val, sizeof(val), "%d%%", (int)(fx.delay.feedback * 100)); break;
          case 3: snprintf(val, sizeof(val), "%d%%", (int)(fx.delay.mix * 100)); break;
        }
        drawMenuItem(i, idx, labels[idx], val, idx == uiCursor);
      }
      break;
    }

    case SPAGE_FX_REVERB: {
      uiItemCount = 4;
      uiClampScroll();
      const char* labels[] = {"Enable", "Room", "Damping", "Mix"};
      for (int i = 0; i < VISIBLE_LINES && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "%s", fx.reverb.enabled ? "ON" : "OFF"); break;
          case 1: snprintf(val, sizeof(val), "%d%%", (int)(fx.reverb.roomSize * 100)); break;
          case 2: snprintf(val, sizeof(val), "%d%%", (int)(fx.reverb.damping * 100)); break;
          case 3: snprintf(val, sizeof(val), "%d%%", (int)(fx.reverb.mix * 100)); break;
        }
        drawMenuItem(i, idx, labels[idx], val, idx == uiCursor);
      }
      break;
    }

    default: break;
  }

  // Page indicator at bottom
  char pgInfo[22];
  snprintf(pgInfo, sizeof(pgInfo), "[<>]Page %d/%d", uiSubPage + 1, SPAGE_COUNT);
  drawStatus(pgInfo);

  u8g2.sendBuffer();
}

static void handleSynthInput() {
  // Page switching with FN+LEFT/RIGHT or when no item selected for L/R
  if (buttons.isDown(BTN_B)) {
    if (buttons.pressed(BTN_RIGHT)) {
      uiSubPage = (uiSubPage + 1) % SPAGE_COUNT;
      uiResetCursor();
      return;
    }
    if (buttons.pressed(BTN_LEFT)) {
      uiSubPage = (uiSubPage - 1 + SPAGE_COUNT) % SPAGE_COUNT;
      uiResetCursor();
      return;
    }
  }

  // BACK
  if (buttons.pressed(BTN_A)) { exitToMenu(); return; }

  // UP/DOWN navigation
  if (buttons.pressOrRepeat(BTN_DOWN)) { uiCursor++; uiNeedsRedraw = true; }
  if (buttons.pressOrRepeat(BTN_UP))   { uiCursor--; uiNeedsRedraw = true; }

  // Parameter adjustment based on current page and cursor
  float fv;
  int iv;

  switch ((SynthPage)uiSubPage) {
    case SPAGE_OSC:
      switch (uiCursor) {
        case 0: { // OSC1 wave
          iv = (int)synth.getOsc1Wave();
          if (cycleInt(iv, 7)) synth.setOsc1Waveform((WaveformType)iv);
          break;
        }
        case 1: { fv = synth.getOsc1Level(); if (adjustFloat(fv, 0, 1, 0.05f, 0.01f)) synth.setOsc1Level(fv); break; }
        case 2: { // OSC2 wave
          iv = (int)synth.getOsc2Wave();
          if (cycleInt(iv, 7)) synth.setOsc2Waveform((WaveformType)iv);
          break;
        }
        case 3: { fv = synth.getOsc2Level(); if (adjustFloat(fv, 0, 1, 0.05f, 0.01f)) synth.setOsc2Level(fv); break; }
        case 4: { fv = synth.getOsc2Detune(); if (adjustFloat(fv, 0, 0.02f, 0.001f, 0.0002f)) synth.setOsc2Detune(fv); break; }
        case 5: { fv = synth.getOsc2Semitones(); if (adjustFloat(fv, -12, 12, 1.0f, 1.0f)) synth.setOsc2Semitones(fv); break; }
      }
      break;

    case SPAGE_FILTER:
      switch (uiCursor) {
        case 0: {
          iv = (int)synth.getFilterType();
          if (cycleInt(iv, 4)) synth.setFilterType((FilterType)iv);
          break;
        }
        case 1: { fv = synth.getFilterCutoff(); if (adjustFloat(fv, 0, 1, 0.05f, 0.01f)) synth.setFilterCutoff(fv); break; }
        case 2: { fv = synth.getFilterResonance(); if (adjustFloat(fv, 0, 1, 0.05f, 0.01f)) synth.setFilterResonance(fv); break; }
        case 3: { fv = synth.getFilterEnvAmount(); if (adjustFloat(fv, 0, 1, 0.05f, 0.01f)) synth.setFilterEnvAmount(fv); break; }
      }
      break;

    case SPAGE_AMP_ENV: {
      float a = synth.getAmpAttack(), d = synth.getAmpDecay();
      float s = synth.getAmpSustain(), r = synth.getAmpRelease();
      bool changed = false;
      switch (uiCursor) {
        case 0: changed = adjustFloat(a, 0.001f, 2.0f, 0.05f, 0.01f); break;
        case 1: changed = adjustFloat(d, 0.001f, 2.0f, 0.05f, 0.01f); break;
        case 2: changed = adjustFloat(s, 0.0f, 1.0f, 0.05f, 0.01f); break;
        case 3: changed = adjustFloat(r, 0.001f, 2.0f, 0.05f, 0.01f); break;
      }
      if (changed) synth.setAmpEnvelope(a, d, s, r);
      break;
    }

    case SPAGE_FILT_ENV: {
      float a = synth.getFilterAttack(), d = synth.getFilterDecay();
      float s = synth.getFilterSustain(), r = synth.getFilterRelease();
      bool changed = false;
      switch (uiCursor) {
        case 0: changed = adjustFloat(a, 0.001f, 2.0f, 0.05f, 0.01f); break;
        case 1: changed = adjustFloat(d, 0.001f, 2.0f, 0.05f, 0.01f); break;
        case 2: changed = adjustFloat(s, 0.0f, 1.0f, 0.05f, 0.01f); break;
        case 3: changed = adjustFloat(r, 0.001f, 2.0f, 0.05f, 0.01f); break;
      }
      if (changed) synth.setFilterEnvelope(a, d, s, r);
      break;
    }

    case SPAGE_LFO:
      switch (uiCursor) {
        case 0: if (buttons.pressed(BTN_CENTER) || buttons.pressed(BTN_RIGHT) || buttons.pressed(BTN_LEFT)) {
          globalLFO.enabled = !globalLFO.enabled; uiNeedsRedraw = true;
        } break;
        case 1: { iv = (int)globalLFO.wave; if (cycleInt(iv, 5)) globalLFO.wave = (LFOWave)iv; break; }
        case 2: { iv = (int)globalLFO.target; if (cycleInt(iv, 6)) globalLFO.target = (LFOTarget)iv; break; }
        case 3: { fv = globalLFO.rate; if (adjustFloat(fv, 0.1f, 20.0f, 0.5f, 0.1f)) globalLFO.rate = fv; break; }
        case 4: { fv = globalLFO.depth; if (adjustFloat(fv, 0, 1, 0.05f, 0.01f)) globalLFO.depth = fv; break; }
      }
      break;

    case SPAGE_FX_CHORUS:
      switch (uiCursor) {
        case 0: if (buttons.pressed(BTN_CENTER) || buttons.pressed(BTN_RIGHT) || buttons.pressed(BTN_LEFT)) {
          fx.chorus.enabled = !fx.chorus.enabled; uiNeedsRedraw = true;
        } break;
        case 1: { fv = fx.chorus.rate; if (adjustFloat(fv, 0.1f, 10.0f, 0.5f, 0.1f)) fx.chorus.rate = fv; break; }
        case 2: { fv = fx.chorus.depth; if (adjustFloat(fv, 0.001f, 0.01f, 0.001f, 0.0005f)) fx.chorus.depth = fv; break; }
        case 3: { fv = fx.chorus.mix; if (adjustFloat(fv, 0, 1, 0.05f, 0.01f)) fx.chorus.mix = fv; break; }
      }
      break;

    case SPAGE_FX_DELAY:
      switch (uiCursor) {
        case 0: if (buttons.pressed(BTN_CENTER) || buttons.pressed(BTN_RIGHT) || buttons.pressed(BTN_LEFT)) {
          fx.delay.enabled = !fx.delay.enabled; uiNeedsRedraw = true;
        } break;
        case 1: { fv = fx.delay.delayMs; if (adjustFloat(fv, 10, 250, 10.0f, 5.0f)) fx.delay.delayMs = fv; break; }
        case 2: { fv = fx.delay.feedback; if (adjustFloat(fv, 0, 0.9f, 0.05f, 0.01f)) fx.delay.feedback = fv; break; }
        case 3: { fv = fx.delay.mix; if (adjustFloat(fv, 0, 1, 0.05f, 0.01f)) fx.delay.mix = fv; break; }
      }
      break;

    case SPAGE_FX_REVERB:
      switch (uiCursor) {
        case 0: if (buttons.pressed(BTN_CENTER) || buttons.pressed(BTN_RIGHT) || buttons.pressed(BTN_LEFT)) {
          fx.reverb.enabled = !fx.reverb.enabled; uiNeedsRedraw = true;
        } break;
        case 1: { fv = fx.reverb.roomSize; if (adjustFloat(fv, 0, 1, 0.05f, 0.01f)) { fx.reverb.roomSize = fv; fx.reverb.updateParams(); } break; }
        case 2: { fv = fx.reverb.damping; if (adjustFloat(fv, 0, 1, 0.05f, 0.01f)) { fx.reverb.damping = fv; fx.reverb.updateParams(); } break; }
        case 3: { fv = fx.reverb.mix; if (adjustFloat(fv, 0, 1, 0.05f, 0.01f)) fx.reverb.mix = fv; break; }
      }
      break;

    default: break;
  }
}

// ═════════════════════════════════════════════════════════════════════════════
// ARP MODE
// ═════════════════════════════════════════════════════════════════════════════
static void drawArpMode() {
  u8g2.clearBuffer();

  char hdr[22];
  snprintf(hdr, sizeof(hdr), "ARP %s %dBPM",
    arp.getIsPlaying() ? "PLAY" : "STOP", (int)arp.getBPM());
  drawHeader(hdr);

  uiItemCount = 5;
  uiClampScroll();

  char val[16];
  const char* labels[] = {"Play/Stop", "Pattern", "BPM", "Octaves", "Gate"};

  for (int i = 0; i < VISIBLE_LINES && (uiScrollTop + i) < uiItemCount; i++) {
    int idx = uiScrollTop + i;
    switch (idx) {
      case 0: snprintf(val, sizeof(val), "%s", arp.getIsPlaying() ? "[STOP]" : "[PLAY]"); break;
      case 1: snprintf(val, sizeof(val), "%s", arpPatternNames[arp.getPattern()]); break;
      case 2: snprintf(val, sizeof(val), "%d", (int)arp.getBPM()); break;
      case 3: snprintf(val, sizeof(val), "%d", arp.getMaxOctaves()); break;
      case 4: snprintf(val, sizeof(val), "%d%%", arp.getGateLength()); break;
    }
    drawMenuItem(i, idx, labels[idx], val, idx == uiCursor);
  }

  char status[22];
  snprintf(status, sizeof(status), "Notes:%d", arp.getNoteCount());
  drawStatus(status);

  u8g2.sendBuffer();
}

static void handleArpInput() {
  if (buttons.pressed(BTN_A)) { exitToMenu(); return; }

  if (buttons.pressOrRepeat(BTN_DOWN)) { uiCursor++; uiNeedsRedraw = true; }
  if (buttons.pressOrRepeat(BTN_UP))   { uiCursor--; uiNeedsRedraw = true; }

  // Quick play/stop
  if (buttons.pressed(BTN_C)) { uiNeedsRedraw = true; }

  int iv;
  switch (uiCursor) {
    case 0: // Play/Stop
      if (buttons.pressed(BTN_CENTER)) {
        // Toggle arp play state (notes come from MIDI)
        uiNeedsRedraw = true;
      }
      break;
    case 1: { // Pattern
      iv = (int)arp.getPattern();
      bool fn = buttons.isDown(BTN_B);
      if (buttons.pressOrRepeat(BTN_RIGHT)) { iv = constrain(iv + (fn ? 10 : 1), 0, NUM_ARP_PATTERNS - 1); arp.setPattern((ArpPattern)iv); uiNeedsRedraw = true; }
      if (buttons.pressOrRepeat(BTN_LEFT))  { iv = constrain(iv - (fn ? 10 : 1), 0, NUM_ARP_PATTERNS - 1); arp.setPattern((ArpPattern)iv); uiNeedsRedraw = true; }
      break;
    }
    case 2: { // BPM
      float bpm = arp.getBPM();
      bool fn = buttons.isDown(BTN_B);
      if (adjustFloat(bpm, 30, 300, fn ? 10.0f : 1.0f, 1.0f)) arp.setBPM(bpm);
      break;
    }
    case 3: { // Octaves
      iv = arp.getMaxOctaves();
      if (adjustInt(iv, 1, 4, 1)) arp.setOctaveRange(iv);
      break;
    }
    case 4: { // Gate
      iv = arp.getGateLength();
      if (adjustInt(iv, 10, 100, 5)) arp.setGateLength(iv);
      break;
    }
  }
}

// ═════════════════════════════════════════════════════════════════════════════
// SEQ MODE – 4 sub-pages
// ═════════════════════════════════════════════════════════════════════════════
static void drawSeqGrid() {
  // 16-step grid visualisation
  int y = 20;
  SequencerTrack* trk = seq.getTrack(seq.getActiveTrack());
  if (!trk) return;

  // Step grid: 16 steps, 7px each + 1px gap
  u8g2.setFont(u8g2_font_5x7_tr);
  for (int s = 0; s < MAX_SEQ_STEPS; s++) {
    int x = 1 + s * 8;
    bool active = trk->steps[s].active;
    bool isCurrent = (seq.getIsPlaying() && seq.getTrackStep(seq.getActiveTrack()) == s);
    bool isCursor = (s == seqStepCursor);

    if (isCurrent) {
      u8g2.drawBox(x, y, 7, 9);
      if (active) {
        u8g2.setDrawColor(0);
        u8g2.drawBox(x + 1, y + 1, 5, 7);
        u8g2.setDrawColor(1);
      }
    } else if (active) {
      u8g2.drawBox(x, y, 7, 9);
    } else {
      u8g2.drawFrame(x, y, 7, 9);
    }

    if (isCursor) {
      u8g2.drawFrame(x - 1, y - 1, 9, 11);
    }
  }

  // Step number
  char sn[4];
  snprintf(sn, sizeof(sn), "%d", seqStepCursor + 1);
  u8g2.drawStr(0, y + 20, "S:");
  u8g2.drawStr(12, y + 20, sn);

  // Current step info
  SequencerStep& st = trk->steps[seqStepCursor];
  char info[22];
  if (st.active) {
    snprintf(info, sizeof(info), "%s%d V%d G%d%%",
      noteNames[st.note % 12], (st.note / 12) - 1,
      st.velocity, st.gatePercent);
  } else {
    snprintf(info, sizeof(info), "--- (off)");
  }
  u8g2.drawStr(24, y + 20, info);

  // Prob/tie line
  if (st.active) {
    snprintf(info, sizeof(info), "P:%d%% %s", st.probability, st.tie ? "TIE" : "");
    u8g2.drawStr(24, y + 29, info);
  }
}

static void drawSeqMode() {
  u8g2.clearBuffer();

  char hdr[22];
  snprintf(hdr, sizeof(hdr), "SEQ T%d %s %dBPM",
    seq.getActiveTrack() + 1,
    seq.getIsPlaying() ? ">" : "x",
    (int)seq.getBPM());
  drawHeader(hdr);

  switch ((SeqPage)uiSubPage) {
    case SEQP_GRID:
      drawSeqGrid();
      drawStatus("[A]Bk [B+<>]Pg [C]P/S");
      break;

    case SEQP_STEP_EDIT: {
      SequencerTrack* trk = seq.getTrack(seq.getActiveTrack());
      if (!trk) break;
      SequencerStep& st = trk->steps[seqStepCursor];

      uiItemCount = 5;
      uiClampScroll();
      char val[16];
      const char* labels[] = {"Note", "Velocity", "Gate", "Prob", "Tie"};

      for (int i = 0; i < VISIBLE_LINES && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "%s%d", noteNames[st.note % 12], (st.note / 12) - 1); break;
          case 1: snprintf(val, sizeof(val), "%d", st.velocity); break;
          case 2: snprintf(val, sizeof(val), "%d%%", st.gatePercent); break;
          case 3: snprintf(val, sizeof(val), "%d%%", st.probability); break;
          case 4: snprintf(val, sizeof(val), "%s", st.tie ? "ON" : "OFF"); break;
        }
        drawMenuItem(i, idx, labels[idx], val, idx == uiCursor);
      }
      drawStatus("[A]Back to grid");
      break;
    }

    case SEQP_TRACK_CFG: {
      SequencerTrack* trk = seq.getTrack(seq.getActiveTrack());
      if (!trk) break;

      uiItemCount = 8;
      uiClampScroll();
      char val[16];
      const char* labels[] = {"Track", "Patch", "MIDI Ch", "Output", "Length", "Octave", "Mute", "Solo"};

      for (int i = 0; i < VISIBLE_LINES && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "T%d", seq.getActiveTrack() + 1); break;
          case 1: snprintf(val, sizeof(val), "%d", trk->soundPatchIdx); break;
          case 2: snprintf(val, sizeof(val), "%d", trk->midiChannel); break;
          case 3: snprintf(val, sizeof(val), "%s", seqOutNames[trk->midiOutput]); break;
          case 4: snprintf(val, sizeof(val), "%d", trk->trackLength); break;
          case 5: snprintf(val, sizeof(val), "%+d", trk->octave); break;
          case 6: snprintf(val, sizeof(val), "%s", trk->muted ? "ON" : "OFF"); break;
          case 7: snprintf(val, sizeof(val), "%s", trk->solo ? "ON" : "OFF"); break;
        }
        drawMenuItem(i, idx, labels[idx], val, idx == uiCursor);
      }
      drawStatus("[A]Back");
      break;
    }

    case SEQP_SCALE: {
      uiItemCount = 5;
      uiClampScroll();
      char val[16];
      const char* labels[] = {"Key", "Scale", "Lock", "BPM", "Swing"};

      for (int i = 0; i < VISIBLE_LINES && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "%s", seqRootNames[seq.getGlobalKey()]); break;
          case 1: snprintf(val, sizeof(val), "%s", seqScaleNames[seq.getGlobalScale()]); break;
          case 2: snprintf(val, sizeof(val), "%s", seq.getScaleLock() ? "ON" : "OFF"); break;
          case 3: snprintf(val, sizeof(val), "%d", (int)seq.getBPM()); break;
          case 4: snprintf(val, sizeof(val), "%d%%", seq.getSwing()); break;
        }
        drawMenuItem(i, idx, labels[idx], val, idx == uiCursor);
      }
      drawStatus("[A]Back");
      break;
    }

    default: break;
  }

  u8g2.sendBuffer();
}

static void handleSeqInput() {
  if (buttons.pressed(BTN_A)) {
    if (uiSubPage != SEQP_GRID) {
      uiSubPage = SEQP_GRID;
      uiResetCursor();
      return;
    }
    exitToMenu();
    return;
  }

  // Play/Stop
  if (buttons.pressed(BTN_C)) {
    if (seq.getIsPlaying()) seq.stop(); else seq.play();
    uiNeedsRedraw = true;
    return;
  }

  switch ((SeqPage)uiSubPage) {
    case SEQP_GRID: {
      // Page switching with FN
      if (buttons.isDown(BTN_B)) {
        if (buttons.pressed(BTN_RIGHT)) {
          uiSubPage = (uiSubPage + 1) % SEQP_COUNT;
          uiResetCursor();
          return;
        }
        if (buttons.pressed(BTN_LEFT)) {
          uiSubPage = (uiSubPage - 1 + SEQP_COUNT) % SEQP_COUNT;
          uiResetCursor();
          return;
        }
        // FN+UP/DOWN = switch track
        if (buttons.pressed(BTN_UP)) {
          seq.setActiveTrack(seq.getActiveTrack() - 1);
          uiNeedsRedraw = true; return;
        }
        if (buttons.pressed(BTN_DOWN)) {
          seq.setActiveTrack(seq.getActiveTrack() + 1);
          uiNeedsRedraw = true; return;
        }
      }

      // Step cursor movement
      if (buttons.pressOrRepeat(BTN_RIGHT)) { seqStepCursor = (seqStepCursor + 1) % MAX_SEQ_STEPS; uiNeedsRedraw = true; }
      if (buttons.pressOrRepeat(BTN_LEFT))  { seqStepCursor = (seqStepCursor - 1 + MAX_SEQ_STEPS) % MAX_SEQ_STEPS; uiNeedsRedraw = true; }

      // Toggle step
      if (buttons.pressed(BTN_CENTER)) {
        seq.toggleStep(seq.getActiveTrack(), seqStepCursor);
        uiNeedsRedraw = true;
      }

      // UP = enter step edit for current step
      if (buttons.pressed(BTN_UP) && !buttons.isDown(BTN_B)) {
        uiSubPage = SEQP_STEP_EDIT;
        uiResetCursor();
      }
      // DOWN = enter track config
      if (buttons.pressed(BTN_DOWN) && !buttons.isDown(BTN_B)) {
        uiSubPage = SEQP_TRACK_CFG;
        uiResetCursor();
      }
      break;
    }

    case SEQP_STEP_EDIT: {
      SequencerTrack* trk = seq.getTrack(seq.getActiveTrack());
      if (!trk) break;
      SequencerStep& st = trk->steps[seqStepCursor];

      if (buttons.pressOrRepeat(BTN_DOWN)) { uiCursor++; uiNeedsRedraw = true; }
      if (buttons.pressOrRepeat(BTN_UP))   { uiCursor--; uiNeedsRedraw = true; }

      int iv;
      switch (uiCursor) {
        case 0: { iv = (int)st.note; if (adjustInt(iv, 0, 127, 1)) { st.note = iv; st.active = true; } break; }
        case 1: { iv = (int)st.velocity; if (adjustInt(iv, 1, 127, 5)) st.velocity = iv; break; }
        case 2: { iv = (int)st.gatePercent; if (adjustInt(iv, 1, 100, 5)) st.gatePercent = iv; break; }
        case 3: { iv = (int)st.probability; if (adjustInt(iv, 1, 100, 5)) st.probability = iv; break; }
        case 4: if (buttons.pressed(BTN_CENTER) || buttons.pressed(BTN_RIGHT) || buttons.pressed(BTN_LEFT)) {
          st.tie = !st.tie; uiNeedsRedraw = true;
        } break;
      }
      break;
    }

    case SEQP_TRACK_CFG: {
      SequencerTrack* trk = seq.getTrack(seq.getActiveTrack());
      if (!trk) break;

      if (buttons.pressOrRepeat(BTN_DOWN)) { uiCursor++; uiNeedsRedraw = true; }
      if (buttons.pressOrRepeat(BTN_UP))   { uiCursor--; uiNeedsRedraw = true; }

      int iv;
      switch (uiCursor) {
        case 0: { // Track select
          iv = seq.getActiveTrack();
          if (cycleInt(iv, MAX_SEQ_TRACKS)) seq.setActiveTrack(iv);
          break;
        }
        case 1: { iv = (int)trk->soundPatchIdx; if (adjustInt(iv, 0, 9, 1)) trk->soundPatchIdx = iv; break; }
        case 2: { iv = (int)trk->midiChannel; if (adjustInt(iv, 1, 16, 1)) trk->midiChannel = iv; break; }
        case 3: { iv = (int)trk->midiOutput; if (cycleInt(iv, 3)) trk->midiOutput = iv; break; }
        case 4: { iv = (int)trk->trackLength; if (adjustInt(iv, 1, 16, 1)) trk->trackLength = iv; break; }
        case 5: { iv = (int)trk->octave; if (adjustInt(iv, -3, 3, 1)) trk->octave = iv; break; }
        case 6: if (buttons.pressed(BTN_CENTER) || buttons.pressed(BTN_RIGHT) || buttons.pressed(BTN_LEFT)) {
          trk->muted = !trk->muted; uiNeedsRedraw = true;
        } break;
        case 7: if (buttons.pressed(BTN_CENTER) || buttons.pressed(BTN_RIGHT) || buttons.pressed(BTN_LEFT)) {
          trk->solo = !trk->solo; uiNeedsRedraw = true;
        } break;
      }
      break;
    }

    case SEQP_SCALE: {
      if (buttons.pressOrRepeat(BTN_DOWN)) { uiCursor++; uiNeedsRedraw = true; }
      if (buttons.pressOrRepeat(BTN_UP))   { uiCursor--; uiNeedsRedraw = true; }

      int iv;
      float fv;
      switch (uiCursor) {
        case 0: { iv = seq.getGlobalKey(); if (cycleInt(iv, 12)) seq.setGlobalKey(iv); break; }
        case 1: { iv = seq.getGlobalScale(); if (cycleInt(iv, NUM_SCALES_SEQ)) seq.setGlobalScale(iv); break; }
        case 2: if (buttons.pressed(BTN_CENTER) || buttons.pressed(BTN_RIGHT) || buttons.pressed(BTN_LEFT)) {
          seq.setScaleLock(!seq.getScaleLock()); uiNeedsRedraw = true;
        } break;
        case 3: { fv = seq.getBPM(); if (adjustFloat(fv, 30, 300, buttons.isDown(BTN_B) ? 10.0f : 1.0f, 1.0f)) seq.setBPM(fv); break; }
        case 4: { iv = seq.getSwing(); if (adjustInt(iv, 50, 75, 1)) seq.setSwing(iv); break; }
      }
      break;
    }

    default: break;
  }
}

// ═════════════════════════════════════════════════════════════════════════════
// CHORD MODE
// ═════════════════════════════════════════════════════════════════════════════
static void chordPlayNotes() {
  int baseNote = (chordOctave + 1) * 12 + chordRoot;
  if (!chordHold) {
    // Release previous chord
    for (int i = 0; i < chordActiveCount; i++) {
      if (chordActiveNotes[i] >= 0) synth.noteOff(chordActiveNotes[i]);
    }
    chordActiveCount = 0;
  }

  const ChordTypeDef& ct = chordTypes[chordType];
  chordActiveCount = 0;
  for (int i = 0; i < ct.numNotes; i++) {
    int note = baseNote + ct.intervals[i];
    if (note >= 0 && note <= 127) {
      synth.noteOn(note, 100);
      chordActiveNotes[chordActiveCount++] = note;
      lastPlayedMidiNote = note;
    }
  }
}

static void chordStopAll() {
  for (int i = 0; i < chordActiveCount; i++) {
    if (chordActiveNotes[i] >= 0) synth.noteOff(chordActiveNotes[i]);
  }
  chordActiveCount = 0;
}

static void drawChordMode() {
  u8g2.clearBuffer();

  char hdr[22];
  snprintf(hdr, sizeof(hdr), "CHORD %s %s Oct%d",
    noteNames[chordRoot], chordTypes[chordType].name, chordOctave);
  drawHeader(hdr);

  uiItemCount = 5;
  uiClampScroll();

  char val[16];
  const char* labels[] = {"Root", "Type", "Octave", "Hold", "PLAY"};

  for (int i = 0; i < VISIBLE_LINES && (uiScrollTop + i) < uiItemCount; i++) {
    int idx = uiScrollTop + i;
    switch (idx) {
      case 0: snprintf(val, sizeof(val), "%s", noteNames[chordRoot]); break;
      case 1: snprintf(val, sizeof(val), "%s", chordTypes[chordType].name); break;
      case 2: snprintf(val, sizeof(val), "%d", chordOctave); break;
      case 3: snprintf(val, sizeof(val), "%s", chordHold ? "ON" : "OFF"); break;
      case 4: snprintf(val, sizeof(val), "[CENTER]"); break;
    }
    drawMenuItem(i, idx, labels[idx], val, idx == uiCursor);
  }

  drawStatus("[A]Back [SEL]Play");
  u8g2.sendBuffer();
}

static void handleChordInput() {
  if (buttons.pressed(BTN_A)) { chordStopAll(); exitToMenu(); return; }

  if (buttons.pressOrRepeat(BTN_DOWN)) { uiCursor++; uiNeedsRedraw = true; }
  if (buttons.pressOrRepeat(BTN_UP))   { uiCursor--; uiNeedsRedraw = true; }

  switch (uiCursor) {
    case 0: if (cycleInt(chordRoot, 12)) {} break;
    case 1: if (cycleInt(chordType, NUM_CHORD_TYPES)) {} break;
    case 2: if (adjustInt(chordOctave, 2, 7, 1)) {} break;
    case 3: if (buttons.pressed(BTN_CENTER) || buttons.pressed(BTN_RIGHT) || buttons.pressed(BTN_LEFT)) {
      chordHold = !chordHold;
      if (!chordHold) chordStopAll();
      uiNeedsRedraw = true;
    } break;
    case 4: if (buttons.pressed(BTN_CENTER)) {
      chordPlayNotes();
      uiNeedsRedraw = true;
    } break;
  }
}

// ═════════════════════════════════════════════════════════════════════════════
// PRESETS MODE
// ═════════════════════════════════════════════════════════════════════════════
static int presetSelectedSlot = 0;

static void drawPresetsMode() {
  u8g2.clearBuffer();
  drawHeader("PRESETS");

  uiItemCount = NUM_PRESETS;
  uiClampScroll();

  for (int i = 0; i < VISIBLE_LINES && (uiScrollTop + i) < uiItemCount; i++) {
    int idx = uiScrollTop + i;
    char label[22];
    snprintf(label, sizeof(label), "%c%d:%s",
      (idx < NUM_FACTORY) ? 'F' : 'U',
      idx, presetMgr.getName(idx));
    drawMenuItem(i, idx, label, "", idx == uiCursor);
  }

  char status[22];
  if (uiCursor < NUM_FACTORY)
    snprintf(status, sizeof(status), "[SEL]Load F%d", uiCursor);
  else
    snprintf(status, sizeof(status), "[SEL]Load [B+S]Save");
  drawStatus(status);

  u8g2.sendBuffer();
}

static void handlePresetsInput() {
  if (buttons.pressed(BTN_A)) { exitToMenu(); return; }

  if (buttons.pressOrRepeat(BTN_DOWN)) { uiCursor++; uiNeedsRedraw = true; }
  if (buttons.pressOrRepeat(BTN_UP))   { uiCursor--; uiNeedsRedraw = true; }

  // Load preset
  if (buttons.pressed(BTN_CENTER)) {
    presetSelectedSlot = uiCursor;
    loadPresetToSynth(presetSelectedSlot);
    uiNeedsRedraw = true;
  }

  // Save to user slot (FN + CENTER)
  if (buttons.isDown(BTN_B) && buttons.pressed(BTN_CENTER)) {
    if (uiCursor >= NUM_FACTORY) {
      saveCurrentToPreset(uiCursor);
      uiNeedsRedraw = true;
    }
  }
}

// ═════════════════════════════════════════════════════════════════════════════
// UI INIT / UPDATE / DISPATCH
// ═════════════════════════════════════════════════════════════════════════════
void uiInit() {
  Wire.setSDA(OLED_SDA_PIN);
  Wire.setSCL(OLED_SCL_PIN);
  Wire.begin();

  u8g2.begin();
  u8g2.setContrast(200);

  // Splash screen
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB10_tr);
  u8g2.drawStr(14, 20, "ZOMBIE SS");
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(12, 35, "PROPHET SYNTHESIZER");
  u8g2.drawStr(28, 48, "RP2040 Edition");
  u8g2.sendBuffer();
  delay(1500);

  uiResetCursor();
  currentMode = MODE_MENU;
}

void uiUpdate() {
  // Always redraw since we're running at ~50Hz and the display is small
  uiNeedsRedraw = true;

  switch (currentMode) {
    case MODE_MENU:    handleMenuInput();    drawMenuMode();    break;
    case MODE_SYNTH:   handleSynthInput();   drawSynthMode();   break;
    case MODE_ARP:     handleArpInput();     drawArpMode();     break;
    case MODE_SEQ:     handleSeqInput();     drawSeqMode();     break;
    case MODE_PRESETS: handlePresetsInput(); drawPresetsMode(); break;
    case MODE_CHORD:   handleChordInput();   drawChordMode();   break;
  }
}

#endif
