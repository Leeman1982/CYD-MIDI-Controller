#ifndef OLED_UI_H
#define OLED_UI_H

// ═══════════════════════════════════════════════════════════════════════════════
// ZOMBIE SS – OLED Graphical UI (RP2040 / 4×4 Matrix Keypad)
// ═══════════════════════════════════════════════════════════════════════════════
//
// Complete graphical UI for 128×64 SH1106 OLED with 16-button 4×4 matrix.
// Uses Adafruit_SH110X + Adafruit_GFX libraries.
//
// Button layout:
//   [BACK ] [ UP  ] [PGUP ] [PLAY ]     Row 1
//   [LEFT ] [ OK  ] [RIGHT] [ FN  ]     Row 2
//   [DOWN ] [PGDN ] [ −   ] [ +   ]     Row 3
//   [ Q1  ] [ Q2  ] [ Q3  ] [ Q4  ]     Row 4 (context-sensitive)
//
// Display layout (128×64):
//   ┌────────────────────────────────┐
//   │ ▌ INVERSE HEADER BAR  Note ▌  │  0-9   (10px)
//   │ Label ████████████████ Val    │  10-19  param row 1
//   │ Label ██████████       Val    │  20-29  param row 2
//   │ Label ████████████████ Val    │  30-39  param row 3
//   │ Label ██████         Val      │  40-49  param row 4
//   │                                │  50-53  (spacer)
//   │ [Q1 ] [Q2 ] [Q3 ] [Q4 ]      │  54-63  context labels
//   └────────────────────────────────┘
//
// Navigation:
//   UP/DOWN     – Select parameter row (cursor)
//   LEFT/RIGHT  – Adjust selected value (coarse)
//   PLUS/MINUS  – Adjust selected value (fine)
//   OK          – Toggle / Enter / Confirm
//   BACK        – Return to parent / menu
//   PLAY        – Quick play/stop for arp/seq
//   FN (hold)   – Modifier (shift functions)
//   PGUP/PGDN   – Previous/next sub-page
//   Q1-Q4       – Context-sensitive actions (shown at bottom)
// ═══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
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
// Adafruit_SH1106G for 1.3" 128×64 SH1106 OLED (I2C)
Adafruit_SH1106G display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// ── UI Constants ────────────────────────────────────────────────────────────
#define SCREEN_W      128
#define SCREEN_H      64
#define HEADER_H      10
#define FOOTER_H      10
#define PARAM_Y_START 12
#define PARAM_ROW_H   10
#define MAX_VISIBLE   4       // 4 parameter rows visible at a time
#define BAR_X         36      // Bar graph left edge
#define BAR_W         60      // Bar graph width
#define VAL_X         99      // Value text left edge

// ── UI State ────────────────────────────────────────────────────────────────
static int  uiCursor      = 0;
static int  uiScrollTop   = 0;
static int  uiItemCount   = 0;
static int  uiSubPage     = 0;

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
static int  chordRoot       = 0;
static int  chordType       = 0;
static int  chordOctave     = 4;
static bool chordHold       = false;
static int  chordActiveNotes[5];
static int  chordActiveCount = 0;

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

// Preset state
static int presetSelectedSlot = 0;

// ── Drawing Helpers ─────────────────────────────────────────────────────────

// Text helper: draw string at (x, yBaseline) matching U8g2 coordinate system.
// Adafruit GFX default font: 6×8 px, cursor is top-left. Baseline offset = 7.
static void drawText(int16_t x, int16_t yBaseline, const char* text) {
  display.setCursor(x, yBaseline - 7);
  display.print(text);
}

// Width of string in pixels (default font: 6 px per character at textSize 1)
static int textWidth(const char* text) {
  return (int)strlen(text) * 6;
}

static void uiClampScroll() {
  if (uiCursor < 0) uiCursor = 0;
  if (uiCursor >= uiItemCount) uiCursor = uiItemCount - 1;
  if (uiCursor < 0) uiCursor = 0;
  if (uiCursor < uiScrollTop) uiScrollTop = uiCursor;
  if (uiCursor >= uiScrollTop + MAX_VISIBLE) uiScrollTop = uiCursor - MAX_VISIBLE + 1;
  if (uiScrollTop < 0) uiScrollTop = 0;
}

static void uiResetCursor() {
  uiCursor = 0;
  uiScrollTop = 0;
}

// Draw inverse header bar (top 10px)
static void drawHeader(const char* title) {
  display.fillRect(0, 0, SCREEN_W, HEADER_H, SH110X_WHITE);
  display.setTextSize(1);
  display.setTextColor(SH110X_BLACK);
  drawText(2, 8, title);

  // Show last played note (top-right)
  if (lastPlayedMidiNote >= 0) {
    char nb[6];
    snprintf(nb, sizeof(nb), "%s%d", midiNoteName(lastPlayedMidiNote), midiNoteOctave(lastPlayedMidiNote));
    int w = textWidth(nb);
    drawText(SCREEN_W - w - 2, 8, nb);
  }
  display.setTextColor(SH110X_WHITE);
}

// Draw Q1-Q4 context labels at the bottom (4 zones, 32px each)
static void drawFooter(const char* q1, const char* q2, const char* q3, const char* q4) {
  const char* labels[4] = {q1, q2, q3, q4};
  display.setTextSize(1);
  int footerY = SCREEN_H - FOOTER_H;

  // Thin separator line
  display.drawFastHLine(0, footerY, SCREEN_W, SH110X_WHITE);

  for (int i = 0; i < 4; i++) {
    int x = i * 32;
    int tw = textWidth(labels[i]);
    int cx = x + (32 - tw) / 2;
    drawText(cx, SCREEN_H - 2, labels[i]);

    // Vertical separator between zones (except after last)
    if (i < 3) display.drawFastVLine(x + 32, footerY + 1, FOOTER_H - 1, SH110X_WHITE);
  }
}

// Draw a horizontal bar graph
static void drawBar(int x, int y, int w, int h, float value) {
  value = constrain(value, 0.0f, 1.0f);
  display.drawRect(x, y, w, h, SH110X_WHITE);
  int fill = (int)(value * (w - 2));
  if (fill > 0) display.fillRect(x + 1, y + 1, fill, h - 2, SH110X_WHITE);
}

// Draw a parameter row: label + bar + value text
// screenRow: 0-3 (visible rows), selected: draw inverted cursor
static void drawParamRow(int screenRow, const char* label, float barVal, const char* valText, bool selected) {
  int y = PARAM_Y_START + screenRow * PARAM_ROW_H;

  if (selected) {
    // Highlight cursor indicator
    display.fillTriangle(0, y + 1, 4, y + 4, 0, y + 7, SH110X_WHITE);
  }

  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  drawText(6, y + 7, label);

  // Bar graph (only if barVal >= 0)
  if (barVal >= 0.0f) {
    drawBar(BAR_X, y + 1, BAR_W, 7, barVal);
  }

  // Right-aligned value text
  int vw = textWidth(valText);
  drawText(SCREEN_W - vw - 1, y + 7, valText);
}

// Draw a parameter row without bar (for text/enum values)
static void drawTextRow(int screenRow, const char* label, const char* valText, bool selected) {
  int y = PARAM_Y_START + screenRow * PARAM_ROW_H;

  if (selected) {
    display.fillTriangle(0, y + 1, 4, y + 4, 0, y + 7, SH110X_WHITE);
  }

  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  drawText(6, y + 7, label);

  int vw = textWidth(valText);
  drawText(SCREEN_W - vw - 1, y + 7, valText);
}

// Draw scroll indicators (up/down arrows)
static void drawScrollIndicators() {
  if (uiItemCount > MAX_VISIBLE) {
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    if (uiScrollTop > 0) drawText(122, PARAM_Y_START + 7, "^");
    if (uiScrollTop + MAX_VISIBLE < uiItemCount) drawText(122, PARAM_Y_START + MAX_VISIBLE * PARAM_ROW_H - 3, "v");
  }
}

// ── Value Adjustment Helpers ────────────────────────────────────────────────

// Adjust float with PLUS/MINUS (fine) and LEFT/RIGHT (coarse)
static bool adjustFloat(float& val, float minV, float maxV, float coarse, float fine) {
  bool changed = false;
  // Coarse: LEFT/RIGHT
  if (buttons.pressOrRepeat(BTN_RIGHT)) { val = constrain(val + coarse, minV, maxV); changed = true; }
  if (buttons.pressOrRepeat(BTN_LEFT))  { val = constrain(val - coarse, minV, maxV); changed = true; }
  // Fine: PLUS/MINUS
  if (buttons.pressOrRepeat(BTN_PLUS))  { val = constrain(val + fine, minV, maxV); changed = true; }
  if (buttons.pressOrRepeat(BTN_MINUS)) { val = constrain(val - fine, minV, maxV); changed = true; }
  return changed;
}

// Adjust int with PLUS/MINUS (1) and LEFT/RIGHT (step)
static bool adjustInt(int& val, int minV, int maxV, int step) {
  bool changed = false;
  if (buttons.pressOrRepeat(BTN_RIGHT)) { val = constrain(val + step, minV, maxV); changed = true; }
  if (buttons.pressOrRepeat(BTN_LEFT))  { val = constrain(val - step, minV, maxV); changed = true; }
  if (buttons.pressOrRepeat(BTN_PLUS))  { val = constrain(val + 1, minV, maxV); changed = true; }
  if (buttons.pressOrRepeat(BTN_MINUS)) { val = constrain(val - 1, minV, maxV); changed = true; }
  return changed;
}

// Cycle an int value (wraps around)
static bool cycleInt(int& val, int count) {
  if (buttons.pressed(BTN_RIGHT) || buttons.pressed(BTN_PLUS)) { val = (val + 1) % count; return true; }
  if (buttons.pressed(BTN_LEFT) || buttons.pressed(BTN_MINUS))  { val = (val - 1 + count) % count; return true; }
  return false;
}

// Common UP/DOWN navigation
static bool handleUpDown() {
  if (buttons.pressOrRepeat(BTN_DOWN)) { uiCursor++; return true; }
  if (buttons.pressOrRepeat(BTN_UP))   { uiCursor--; return true; }
  return false;
}

// ═════════════════════════════════════════════════════════════════════════════
// MAIN MENU
// ═════════════════════════════════════════════════════════════════════════════
static void drawMenuMode() {
  display.clearDisplay();
  drawHeader("ZOMBIE SS PROPHET");

  static const char* menuItems[] = {"SYNTH", "ARP", "SEQ", "PRESETS", "CHORDS"};
  uiItemCount = 5;
  uiClampScroll();

  for (int i = 0; i < MAX_VISIBLE && (uiScrollTop + i) < uiItemCount; i++) {
    int idx = uiScrollTop + i;
    char val[10] = "";
    switch (idx) {
      case 0: snprintf(val, sizeof(val), "%dV", synth.getActiveVoiceCount()); break;
      case 1: if (arp.getIsPlaying()) snprintf(val, sizeof(val), "PLAY"); break;
      case 2: if (seq.getIsPlaying()) snprintf(val, sizeof(val), "PLAY"); break;
    }
    drawTextRow(i, menuItems[idx], val, idx == uiCursor);
  }

  drawScrollIndicators();
  drawFooter("SYNTH", "ARP", "SEQ", "PRE");
  display.display();
}

static void handleMenuInput() {
  static const AppMode menuModes[] = {MODE_SYNTH, MODE_ARP, MODE_SEQ, MODE_PRESETS, MODE_CHORD};

  handleUpDown();

  // OK to enter selected mode
  if (buttons.pressed(BTN_OK)) {
    if (uiCursor >= 0 && uiCursor < 5) {
      currentMode = menuModes[uiCursor];
      uiResetCursor();
      uiSubPage = 0;
    }
  }

  // Q1-Q4 quick jump
  if (buttons.pressed(BTN_Q1)) { currentMode = MODE_SYNTH;   uiResetCursor(); uiSubPage = 0; }
  if (buttons.pressed(BTN_Q2)) { currentMode = MODE_ARP;     uiResetCursor(); uiSubPage = 0; }
  if (buttons.pressed(BTN_Q3)) { currentMode = MODE_SEQ;     uiResetCursor(); uiSubPage = 0; }
  if (buttons.pressed(BTN_Q4)) { currentMode = MODE_PRESETS;  uiResetCursor(); uiSubPage = 0; }
}

// ═════════════════════════════════════════════════════════════════════════════
// SYNTH MODE – 8 parameter pages with bar graphs
// ═════════════════════════════════════════════════════════════════════════════
static void drawSynthMode() {
  display.clearDisplay();

  char hdr[22];
  snprintf(hdr, sizeof(hdr), "SYNTH:%s", synthPageNames[uiSubPage]);
  drawHeader(hdr);

  char val[16];

  switch ((SynthPage)uiSubPage) {

    case SPAGE_OSC: {
      uiItemCount = 6;
      uiClampScroll();
      for (int i = 0; i < MAX_VISIBLE && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        bool sel = (idx == uiCursor);
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "%s", waveformNames[synth.getOsc1Wave()]);
                  drawTextRow(i, "O1 Wav", val, sel); break;
          case 1: snprintf(val, sizeof(val), "%d%%", (int)(synth.getOsc1Level() * 100));
                  drawParamRow(i, "O1 Lvl", synth.getOsc1Level(), val, sel); break;
          case 2: snprintf(val, sizeof(val), "%s", waveformNames[synth.getOsc2Wave()]);
                  drawTextRow(i, "O2 Wav", val, sel); break;
          case 3: snprintf(val, sizeof(val), "%d%%", (int)(synth.getOsc2Level() * 100));
                  drawParamRow(i, "O2 Lvl", synth.getOsc2Level(), val, sel); break;
          case 4: { float d = synth.getOsc2Detune() / 0.02f;
                  snprintf(val, sizeof(val), "%d%%", (int)(d * 100));
                  drawParamRow(i, "Detune", d, val, sel); break; }
          case 5: snprintf(val, sizeof(val), "%+d", (int)synth.getOsc2Semitones());
                  drawTextRow(i, "Semi", val, sel); break;
        }
      }
      break;
    }

    case SPAGE_FILTER: {
      uiItemCount = 4;
      uiClampScroll();
      for (int i = 0; i < MAX_VISIBLE && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        bool sel = (idx == uiCursor);
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "%s", filterTypeNames[synth.getFilterType()]);
                  drawTextRow(i, "Type", val, sel); break;
          case 1: snprintf(val, sizeof(val), "%d%%", (int)(synth.getFilterCutoff() * 100));
                  drawParamRow(i, "Cutoff", synth.getFilterCutoff(), val, sel); break;
          case 2: snprintf(val, sizeof(val), "%d%%", (int)(synth.getFilterResonance() * 100));
                  drawParamRow(i, "Reso", synth.getFilterResonance(), val, sel); break;
          case 3: snprintf(val, sizeof(val), "%d%%", (int)(synth.getFilterEnvAmount() * 100));
                  drawParamRow(i, "EnvAmt", synth.getFilterEnvAmount(), val, sel); break;
        }
      }
      break;
    }

    case SPAGE_AMP_ENV: {
      uiItemCount = 4;
      uiClampScroll();
      float a = synth.getAmpAttack(), d = synth.getAmpDecay();
      float s = synth.getAmpSustain(), r = synth.getAmpRelease();
      for (int i = 0; i < MAX_VISIBLE && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        bool sel = (idx == uiCursor);
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "%dms", (int)(a * 1000));
                  drawParamRow(i, "Atk", a / 2.0f, val, sel); break;
          case 1: snprintf(val, sizeof(val), "%dms", (int)(d * 1000));
                  drawParamRow(i, "Dec", d / 2.0f, val, sel); break;
          case 2: snprintf(val, sizeof(val), "%d%%", (int)(s * 100));
                  drawParamRow(i, "Sus", s, val, sel); break;
          case 3: snprintf(val, sizeof(val), "%dms", (int)(r * 1000));
                  drawParamRow(i, "Rel", r / 2.0f, val, sel); break;
        }
      }
      break;
    }

    case SPAGE_FILT_ENV: {
      uiItemCount = 4;
      uiClampScroll();
      float a = synth.getFilterAttack(), d = synth.getFilterDecay();
      float s = synth.getFilterSustain(), r = synth.getFilterRelease();
      for (int i = 0; i < MAX_VISIBLE && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        bool sel = (idx == uiCursor);
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "%dms", (int)(a * 1000));
                  drawParamRow(i, "Atk", a / 2.0f, val, sel); break;
          case 1: snprintf(val, sizeof(val), "%dms", (int)(d * 1000));
                  drawParamRow(i, "Dec", d / 2.0f, val, sel); break;
          case 2: snprintf(val, sizeof(val), "%d%%", (int)(s * 100));
                  drawParamRow(i, "Sus", s, val, sel); break;
          case 3: snprintf(val, sizeof(val), "%dms", (int)(r * 1000));
                  drawParamRow(i, "Rel", r / 2.0f, val, sel); break;
        }
      }
      break;
    }

    case SPAGE_LFO: {
      uiItemCount = 5;
      uiClampScroll();
      for (int i = 0; i < MAX_VISIBLE && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        bool sel = (idx == uiCursor);
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "%s", globalLFO.enabled ? "ON" : "OFF");
                  drawTextRow(i, "Enable", val, sel); break;
          case 1: snprintf(val, sizeof(val), "%s", lfoWaveNames[globalLFO.wave]);
                  drawTextRow(i, "Wave", val, sel); break;
          case 2: snprintf(val, sizeof(val), "%s", lfoTargetNames[globalLFO.target]);
                  drawTextRow(i, "Target", val, sel); break;
          case 3: snprintf(val, sizeof(val), "%.1fHz", globalLFO.rate);
                  drawParamRow(i, "Rate", globalLFO.rate / 20.0f, val, sel); break;
          case 4: snprintf(val, sizeof(val), "%d%%", (int)(globalLFO.depth * 100));
                  drawParamRow(i, "Depth", globalLFO.depth, val, sel); break;
        }
      }
      break;
    }

    case SPAGE_FX_CHORUS: {
      uiItemCount = 4;
      uiClampScroll();
      for (int i = 0; i < MAX_VISIBLE && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        bool sel = (idx == uiCursor);
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "%s", fx.chorus.enabled ? "ON" : "OFF");
                  drawTextRow(i, "Enable", val, sel); break;
          case 1: snprintf(val, sizeof(val), "%.1fHz", fx.chorus.rate);
                  drawParamRow(i, "Rate", fx.chorus.rate / 10.0f, val, sel); break;
          case 2: snprintf(val, sizeof(val), "%d%%", (int)(fx.chorus.depth / 0.01f * 100));
                  drawParamRow(i, "Depth", fx.chorus.depth / 0.01f, val, sel); break;
          case 3: snprintf(val, sizeof(val), "%d%%", (int)(fx.chorus.mix * 100));
                  drawParamRow(i, "Mix", fx.chorus.mix, val, sel); break;
        }
      }
      break;
    }

    case SPAGE_FX_DELAY: {
      uiItemCount = 4;
      uiClampScroll();
      for (int i = 0; i < MAX_VISIBLE && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        bool sel = (idx == uiCursor);
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "%s", fx.delay.enabled ? "ON" : "OFF");
                  drawTextRow(i, "Enable", val, sel); break;
          case 1: snprintf(val, sizeof(val), "%dms", (int)fx.delay.delayMs);
                  drawParamRow(i, "Time", fx.delay.delayMs / 250.0f, val, sel); break;
          case 2: snprintf(val, sizeof(val), "%d%%", (int)(fx.delay.feedback * 100));
                  drawParamRow(i, "Feedbk", fx.delay.feedback / 0.9f, val, sel); break;
          case 3: snprintf(val, sizeof(val), "%d%%", (int)(fx.delay.mix * 100));
                  drawParamRow(i, "Mix", fx.delay.mix, val, sel); break;
        }
      }
      break;
    }

    case SPAGE_FX_REVERB: {
      uiItemCount = 4;
      uiClampScroll();
      for (int i = 0; i < MAX_VISIBLE && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        bool sel = (idx == uiCursor);
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "%s", fx.reverb.enabled ? "ON" : "OFF");
                  drawTextRow(i, "Enable", val, sel); break;
          case 1: snprintf(val, sizeof(val), "%d%%", (int)(fx.reverb.roomSize * 100));
                  drawParamRow(i, "Room", fx.reverb.roomSize, val, sel); break;
          case 2: snprintf(val, sizeof(val), "%d%%", (int)(fx.reverb.damping * 100));
                  drawParamRow(i, "Damp", fx.reverb.damping, val, sel); break;
          case 3: snprintf(val, sizeof(val), "%d%%", (int)(fx.reverb.mix * 100));
                  drawParamRow(i, "Mix", fx.reverb.mix, val, sel); break;
        }
      }
      break;
    }

    default: break;
  }

  // Scroll indicators if needed
  drawScrollIndicators();

  // Footer: Q1-Q4 switch between synth sub-page groups
  drawFooter("OSC", "FLT", "ENV", "FX");
  display.display();
}

static void handleSynthInput() {
  // BACK
  if (buttons.pressed(BTN_BACK)) { exitToMenu(); return; }

  // PGUP/PGDN to cycle sub-pages
  if (buttons.pressed(BTN_PGDN)) {
    uiSubPage = (uiSubPage + 1) % SPAGE_COUNT;
    uiResetCursor();
    return;
  }
  if (buttons.pressed(BTN_PGUP)) {
    uiSubPage = (uiSubPage - 1 + SPAGE_COUNT) % SPAGE_COUNT;
    uiResetCursor();
    return;
  }

  // Q1-Q4 quick page jumps
  if (buttons.pressed(BTN_Q1)) { uiSubPage = SPAGE_OSC;       uiResetCursor(); return; }
  if (buttons.pressed(BTN_Q2)) { uiSubPage = SPAGE_FILTER;    uiResetCursor(); return; }
  if (buttons.pressed(BTN_Q3)) { uiSubPage = SPAGE_AMP_ENV;   uiResetCursor(); return; }
  if (buttons.pressed(BTN_Q4)) {
    // FX group: cycle through chorus/delay/reverb, or jump to first FX page
    if (uiSubPage >= SPAGE_FX_CHORUS && uiSubPage <= SPAGE_FX_REVERB) {
      uiSubPage = (uiSubPage + 1 > SPAGE_FX_REVERB) ? SPAGE_FX_CHORUS : uiSubPage + 1;
    } else {
      uiSubPage = SPAGE_FX_CHORUS;
    }
    uiResetCursor();
    return;
  }

  // UP/DOWN navigation
  handleUpDown();

  // Parameter adjustment based on current page and cursor
  float fv;
  int iv;

  switch ((SynthPage)uiSubPage) {
    case SPAGE_OSC:
      switch (uiCursor) {
        case 0: { iv = (int)synth.getOsc1Wave(); if (cycleInt(iv, 7)) synth.setOsc1Waveform((WaveformType)iv); break; }
        case 1: { fv = synth.getOsc1Level(); if (adjustFloat(fv, 0, 1, 0.05f, 0.01f)) synth.setOsc1Level(fv); break; }
        case 2: { iv = (int)synth.getOsc2Wave(); if (cycleInt(iv, 7)) synth.setOsc2Waveform((WaveformType)iv); break; }
        case 3: { fv = synth.getOsc2Level(); if (adjustFloat(fv, 0, 1, 0.05f, 0.01f)) synth.setOsc2Level(fv); break; }
        case 4: { fv = synth.getOsc2Detune(); if (adjustFloat(fv, 0, 0.02f, 0.001f, 0.0002f)) synth.setOsc2Detune(fv); break; }
        case 5: { fv = synth.getOsc2Semitones(); if (adjustFloat(fv, -12, 12, 1.0f, 1.0f)) synth.setOsc2Semitones(fv); break; }
      }
      break;

    case SPAGE_FILTER:
      switch (uiCursor) {
        case 0: { iv = (int)synth.getFilterType(); if (cycleInt(iv, 4)) synth.setFilterType((FilterType)iv); break; }
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
        case 0: if (buttons.pressed(BTN_OK) || buttons.pressed(BTN_RIGHT) || buttons.pressed(BTN_LEFT)) {
          globalLFO.enabled = !globalLFO.enabled;
        } break;
        case 1: { iv = (int)globalLFO.wave; if (cycleInt(iv, 5)) globalLFO.wave = (LFOWave)iv; break; }
        case 2: { iv = (int)globalLFO.target; if (cycleInt(iv, 6)) globalLFO.target = (LFOTarget)iv; break; }
        case 3: { fv = globalLFO.rate; if (adjustFloat(fv, 0.1f, 20.0f, 0.5f, 0.1f)) globalLFO.rate = fv; break; }
        case 4: { fv = globalLFO.depth; if (adjustFloat(fv, 0, 1, 0.05f, 0.01f)) globalLFO.depth = fv; break; }
      }
      break;

    case SPAGE_FX_CHORUS:
      switch (uiCursor) {
        case 0: if (buttons.pressed(BTN_OK) || buttons.pressed(BTN_RIGHT) || buttons.pressed(BTN_LEFT)) {
          fx.chorus.enabled = !fx.chorus.enabled;
        } break;
        case 1: { fv = fx.chorus.rate; if (adjustFloat(fv, 0.1f, 10.0f, 0.5f, 0.1f)) fx.chorus.rate = fv; break; }
        case 2: { fv = fx.chorus.depth; if (adjustFloat(fv, 0.001f, 0.01f, 0.001f, 0.0005f)) fx.chorus.depth = fv; break; }
        case 3: { fv = fx.chorus.mix; if (adjustFloat(fv, 0, 1, 0.05f, 0.01f)) fx.chorus.mix = fv; break; }
      }
      break;

    case SPAGE_FX_DELAY:
      switch (uiCursor) {
        case 0: if (buttons.pressed(BTN_OK) || buttons.pressed(BTN_RIGHT) || buttons.pressed(BTN_LEFT)) {
          fx.delay.enabled = !fx.delay.enabled;
        } break;
        case 1: { fv = fx.delay.delayMs; if (adjustFloat(fv, 10, 250, 10.0f, 5.0f)) fx.delay.delayMs = fv; break; }
        case 2: { fv = fx.delay.feedback; if (adjustFloat(fv, 0, 0.9f, 0.05f, 0.01f)) fx.delay.feedback = fv; break; }
        case 3: { fv = fx.delay.mix; if (adjustFloat(fv, 0, 1, 0.05f, 0.01f)) fx.delay.mix = fv; break; }
      }
      break;

    case SPAGE_FX_REVERB:
      switch (uiCursor) {
        case 0: if (buttons.pressed(BTN_OK) || buttons.pressed(BTN_RIGHT) || buttons.pressed(BTN_LEFT)) {
          fx.reverb.enabled = !fx.reverb.enabled;
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
  display.clearDisplay();

  char hdr[22];
  snprintf(hdr, sizeof(hdr), "ARP %s %dBPM",
    arp.getIsPlaying() ? ">" : "x", (int)arp.getBPM());
  drawHeader(hdr);

  uiItemCount = 5;
  uiClampScroll();

  char val[16];
  for (int i = 0; i < MAX_VISIBLE && (uiScrollTop + i) < uiItemCount; i++) {
    int idx = uiScrollTop + i;
    bool sel = (idx == uiCursor);
    switch (idx) {
      case 0: snprintf(val, sizeof(val), "%s", arp.getIsPlaying() ? "STOP" : "PLAY");
              drawTextRow(i, "Play", val, sel); break;
      case 1: snprintf(val, sizeof(val), "%s", arpPatternNames[arp.getPattern()]);
              drawTextRow(i, "Patt", val, sel); break;
      case 2: snprintf(val, sizeof(val), "%d", (int)arp.getBPM());
              drawParamRow(i, "BPM", (arp.getBPM() - 30.0f) / 270.0f, val, sel); break;
      case 3: snprintf(val, sizeof(val), "%d oct", arp.getMaxOctaves());
              drawParamRow(i, "Oct", (arp.getMaxOctaves() - 1) / 3.0f, val, sel); break;
      case 4: snprintf(val, sizeof(val), "%d%%", arp.getGateLength());
              drawParamRow(i, "Gate", arp.getGateLength() / 100.0f, val, sel); break;
    }
  }

  // Scroll indicators
  drawScrollIndicators();

  // Show note count in the area above the footer
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  char nc[12];
  snprintf(nc, sizeof(nc), "Notes:%d", arp.getNoteCount());
  drawText(2, 52, nc);

  drawFooter("PLAY", "PATT", "BPM", "GATE");
  display.display();
}

static void handleArpInput() {
  if (buttons.pressed(BTN_BACK)) { exitToMenu(); return; }

  handleUpDown();

  // PLAY button: quick play/stop
  if (buttons.pressed(BTN_PLAY)) {
    // Arp toggled by PLAY button - notes come from MIDI
  }

  int iv;
  switch (uiCursor) {
    case 0: // Play/Stop
      if (buttons.pressed(BTN_OK)) {
        // Toggle arp play state
      }
      break;
    case 1: { // Pattern
      iv = (int)arp.getPattern();
      if (buttons.pressOrRepeat(BTN_RIGHT) || buttons.pressOrRepeat(BTN_PLUS)) {
        iv = constrain(iv + 1, 0, NUM_ARP_PATTERNS - 1); arp.setPattern((ArpPattern)iv);
      }
      if (buttons.pressOrRepeat(BTN_LEFT) || buttons.pressOrRepeat(BTN_MINUS)) {
        iv = constrain(iv - 1, 0, NUM_ARP_PATTERNS - 1); arp.setPattern((ArpPattern)iv);
      }
      // FN + LEFT/RIGHT for fast ±10
      if (buttons.isDown(BTN_FN)) {
        if (buttons.pressOrRepeat(BTN_RIGHT)) {
          iv = constrain((int)arp.getPattern() + 10, 0, NUM_ARP_PATTERNS - 1); arp.setPattern((ArpPattern)iv);
        }
        if (buttons.pressOrRepeat(BTN_LEFT)) {
          iv = constrain((int)arp.getPattern() - 10, 0, NUM_ARP_PATTERNS - 1); arp.setPattern((ArpPattern)iv);
        }
      }
      break;
    }
    case 2: { // BPM
      float bpm = arp.getBPM();
      if (adjustFloat(bpm, 30, 300, 5.0f, 1.0f)) arp.setBPM(bpm);
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

  // Q1: jump to Play/Stop param
  if (buttons.pressed(BTN_Q1)) { uiCursor = 0; }
  // Q2: jump to Pattern
  if (buttons.pressed(BTN_Q2)) { uiCursor = 1; }
  // Q3: jump to BPM
  if (buttons.pressed(BTN_Q3)) { uiCursor = 2; }
  // Q4: jump to Gate
  if (buttons.pressed(BTN_Q4)) { uiCursor = 4; }
}

// ═════════════════════════════════════════════════════════════════════════════
// SEQ MODE – 4 sub-pages
// ═════════════════════════════════════════════════════════════════════════════

// ── SEQ: Grid View ──────────────────────────────────────────────────────────
static void drawSeqGrid() {
  SequencerTrack* trk = seq.getTrack(seq.getActiveTrack());
  if (!trk) return;

  int gridY = 13;

  // 16 steps: 7px wide + 1px gap = 8px each = 128px total
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  for (int s = 0; s < MAX_SEQ_STEPS; s++) {
    int x = s * 8;
    bool active = trk->steps[s].active;
    bool isCurrent = (seq.getIsPlaying() && seq.getTrackStep(seq.getActiveTrack()) == s);
    bool isCursor = (s == seqStepCursor);

    if (isCurrent) {
      // Playing step: filled box with hollow center if active
      display.fillRect(x, gridY, 7, 9, SH110X_WHITE);
      if (active) {
        display.fillRect(x + 1, gridY + 1, 5, 7, SH110X_BLACK);
      }
    } else if (active) {
      display.fillRect(x, gridY, 7, 9, SH110X_WHITE);
    } else {
      display.drawRect(x, gridY, 7, 9, SH110X_WHITE);
    }

    if (isCursor) {
      // Cursor: outer frame
      if (x > 0) display.drawRect(x - 1, gridY - 1, 9, 11, SH110X_WHITE);
      else display.drawRect(x, gridY - 1, 8, 11, SH110X_WHITE);
    }
  }

  // Step info below grid
  int infoY = gridY + 12;
  SequencerStep& st = trk->steps[seqStepCursor];
  char info[22];

  // Line 1: Step number + note info
  if (st.active) {
    snprintf(info, sizeof(info), "S%d %s%d V%d G%d%%",
      seqStepCursor + 1,
      noteNames[st.note % 12], (st.note / 12) - 1,
      st.velocity, st.gatePercent);
  } else {
    snprintf(info, sizeof(info), "S%d --- (off)", seqStepCursor + 1);
  }
  drawText(0, infoY + 7, info);

  // Line 2: Probability + tie
  if (st.active) {
    snprintf(info, sizeof(info), "P:%d%% %s Trk:%d/%d",
      st.probability, st.tie ? "TIE" : "",
      seq.getActiveTrack() + 1, MAX_SEQ_TRACKS);
  } else {
    snprintf(info, sizeof(info), "Trk:%d/%d Len:%d",
      seq.getActiveTrack() + 1, MAX_SEQ_TRACKS, trk->trackLength);
  }
  drawText(0, infoY + 16, info);
}

static void drawSeqMode() {
  display.clearDisplay();

  char hdr[22];
  snprintf(hdr, sizeof(hdr), "SEQ T%d %s %dBPM",
    seq.getActiveTrack() + 1,
    seq.getIsPlaying() ? ">" : "x",
    (int)seq.getBPM());
  drawHeader(hdr);

  switch ((SeqPage)uiSubPage) {

    case SEQP_GRID:
      drawSeqGrid();
      drawFooter("T1", "T2", "T3", "T4");
      break;

    case SEQP_STEP_EDIT: {
      SequencerTrack* trk = seq.getTrack(seq.getActiveTrack());
      if (!trk) break;
      SequencerStep& st = trk->steps[seqStepCursor];

      uiItemCount = 5;
      uiClampScroll();
      char val[16];

      for (int i = 0; i < MAX_VISIBLE && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        bool sel = (idx == uiCursor);
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "%s%d", noteNames[st.note % 12], (st.note / 12) - 1);
                  drawParamRow(i, "Note", st.note / 127.0f, val, sel); break;
          case 1: snprintf(val, sizeof(val), "%d", st.velocity);
                  drawParamRow(i, "Vel", st.velocity / 127.0f, val, sel); break;
          case 2: snprintf(val, sizeof(val), "%d%%", st.gatePercent);
                  drawParamRow(i, "Gate", st.gatePercent / 100.0f, val, sel); break;
          case 3: snprintf(val, sizeof(val), "%d%%", st.probability);
                  drawParamRow(i, "Prob", st.probability / 100.0f, val, sel); break;
          case 4: snprintf(val, sizeof(val), "%s", st.tie ? "ON" : "OFF");
                  drawTextRow(i, "Tie", val, sel); break;
        }
      }
      drawFooter("NOTE", "VEL", "GATE", "PROB");
      break;
    }

    case SEQP_TRACK_CFG: {
      SequencerTrack* trk = seq.getTrack(seq.getActiveTrack());
      if (!trk) break;

      uiItemCount = 8;
      uiClampScroll();
      char val[16];

      for (int i = 0; i < MAX_VISIBLE && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        bool sel = (idx == uiCursor);
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "T%d", seq.getActiveTrack() + 1);
                  drawTextRow(i, "Track", val, sel); break;
          case 1: snprintf(val, sizeof(val), "%d", trk->soundPatchIdx);
                  drawTextRow(i, "Patch", val, sel); break;
          case 2: snprintf(val, sizeof(val), "%d", trk->midiChannel);
                  drawTextRow(i, "MIDI Ch", val, sel); break;
          case 3: snprintf(val, sizeof(val), "%s", seqOutNames[trk->midiOutput]);
                  drawTextRow(i, "Output", val, sel); break;
          case 4: snprintf(val, sizeof(val), "%d", trk->trackLength);
                  drawParamRow(i, "Length", trk->trackLength / 16.0f, val, sel); break;
          case 5: snprintf(val, sizeof(val), "%+d", trk->octave);
                  drawTextRow(i, "Octave", val, sel); break;
          case 6: snprintf(val, sizeof(val), "%s", trk->muted ? "MUTE" : "---");
                  drawTextRow(i, "Mute", val, sel); break;
          case 7: snprintf(val, sizeof(val), "%s", trk->solo ? "SOLO" : "---");
                  drawTextRow(i, "Solo", val, sel); break;
        }
      }

      drawScrollIndicators();
      drawFooter("CLR", "RND", "COPY", "MUTE");
      break;
    }

    case SEQP_SCALE: {
      uiItemCount = 5;
      uiClampScroll();
      char val[16];

      for (int i = 0; i < MAX_VISIBLE && (uiScrollTop + i) < uiItemCount; i++) {
        int idx = uiScrollTop + i;
        bool sel = (idx == uiCursor);
        switch (idx) {
          case 0: snprintf(val, sizeof(val), "%s", seqRootNames[seq.getGlobalKey()]);
                  drawTextRow(i, "Key", val, sel); break;
          case 1: snprintf(val, sizeof(val), "%s", seqScaleNames[seq.getGlobalScale()]);
                  drawTextRow(i, "Scale", val, sel); break;
          case 2: snprintf(val, sizeof(val), "%s", seq.getScaleLock() ? "ON" : "OFF");
                  drawTextRow(i, "Lock", val, sel); break;
          case 3: snprintf(val, sizeof(val), "%d", (int)seq.getBPM());
                  drawParamRow(i, "BPM", (seq.getBPM() - 30.0f) / 270.0f, val, sel); break;
          case 4: snprintf(val, sizeof(val), "%d%%", seq.getSwing());
                  drawParamRow(i, "Swing", (seq.getSwing() - 50) / 25.0f, val, sel); break;
        }
      }

      drawScrollIndicators();
      drawFooter("KEY", "SCAL", "LOCK", "BPM");
      break;
    }

    default: break;
  }

  display.display();
}

static void handleSeqInput() {
  // BACK: go up one level (sub-page → grid → menu)
  if (buttons.pressed(BTN_BACK)) {
    if (uiSubPage != SEQP_GRID) {
      uiSubPage = SEQP_GRID;
      uiResetCursor();
      return;
    }
    exitToMenu();
    return;
  }

  // PLAY button: quick play/stop
  if (buttons.pressed(BTN_PLAY)) {
    if (seq.getIsPlaying()) seq.stop(); else seq.play();
    return;
  }

  // PGUP/PGDN: switch sub-pages
  if (buttons.pressed(BTN_PGDN)) {
    uiSubPage = (uiSubPage + 1) % SEQP_COUNT;
    uiResetCursor();
    return;
  }
  if (buttons.pressed(BTN_PGUP)) {
    uiSubPage = (uiSubPage - 1 + SEQP_COUNT) % SEQP_COUNT;
    uiResetCursor();
    return;
  }

  switch ((SeqPage)uiSubPage) {

    case SEQP_GRID: {
      // Q1-Q4: switch tracks directly
      if (buttons.pressed(BTN_Q1)) { seq.setActiveTrack(0); return; }
      if (buttons.pressed(BTN_Q2)) { seq.setActiveTrack(1); return; }
      if (buttons.pressed(BTN_Q3)) { seq.setActiveTrack(2); return; }
      if (buttons.pressed(BTN_Q4)) { seq.setActiveTrack(3); return; }

      // LEFT/RIGHT: move step cursor
      if (buttons.pressOrRepeat(BTN_RIGHT)) { seqStepCursor = (seqStepCursor + 1) % MAX_SEQ_STEPS; }
      if (buttons.pressOrRepeat(BTN_LEFT))  { seqStepCursor = (seqStepCursor - 1 + MAX_SEQ_STEPS) % MAX_SEQ_STEPS; }

      // OK: toggle step on/off
      if (buttons.pressed(BTN_OK)) {
        seq.toggleStep(seq.getActiveTrack(), seqStepCursor);
      }

      // UP: enter step edit for cursor position
      if (buttons.pressed(BTN_UP)) {
        uiSubPage = SEQP_STEP_EDIT;
        uiResetCursor();
      }
      // DOWN: enter track config
      if (buttons.pressed(BTN_DOWN)) {
        uiSubPage = SEQP_TRACK_CFG;
        uiResetCursor();
      }

      // FN + UP/DOWN: switch tracks
      if (buttons.isDown(BTN_FN)) {
        if (buttons.pressed(BTN_UP))   { seq.setActiveTrack(seq.getActiveTrack() - 1); }
        if (buttons.pressed(BTN_DOWN)) { seq.setActiveTrack(seq.getActiveTrack() + 1); }
      }
      break;
    }

    case SEQP_STEP_EDIT: {
      SequencerTrack* trk = seq.getTrack(seq.getActiveTrack());
      if (!trk) break;
      SequencerStep& st = trk->steps[seqStepCursor];

      handleUpDown();

      // Q1-Q4: jump to param
      if (buttons.pressed(BTN_Q1)) { uiCursor = 0; }
      if (buttons.pressed(BTN_Q2)) { uiCursor = 1; }
      if (buttons.pressed(BTN_Q3)) { uiCursor = 2; }
      if (buttons.pressed(BTN_Q4)) { uiCursor = 3; }

      int iv;
      switch (uiCursor) {
        case 0: { iv = (int)st.note; if (adjustInt(iv, 0, 127, 1)) { st.note = iv; st.active = true; } break; }
        case 1: { iv = (int)st.velocity; if (adjustInt(iv, 1, 127, 5)) st.velocity = iv; break; }
        case 2: { iv = (int)st.gatePercent; if (adjustInt(iv, 1, 100, 5)) st.gatePercent = iv; break; }
        case 3: { iv = (int)st.probability; if (adjustInt(iv, 1, 100, 5)) st.probability = iv; break; }
        case 4: if (buttons.pressed(BTN_OK) || buttons.pressed(BTN_RIGHT) || buttons.pressed(BTN_LEFT)) {
          st.tie = !st.tie;
        } break;
      }
      break;
    }

    case SEQP_TRACK_CFG: {
      SequencerTrack* trk = seq.getTrack(seq.getActiveTrack());
      if (!trk) break;

      handleUpDown();

      // Q1: clear track, Q2: randomize, Q3: copy (clipboard), Q4: toggle mute
      if (buttons.pressed(BTN_Q1)) { trk->clear(); }
      if (buttons.pressed(BTN_Q2)) { trk->randomize(); }
      // Q3: copy current track to clipboard
      if (buttons.pressed(BTN_Q3)) {
        // No clipboard in current scope, just skip for now
      }
      if (buttons.pressed(BTN_Q4)) { trk->muted = !trk->muted; }

      int iv;
      switch (uiCursor) {
        case 0: { iv = seq.getActiveTrack(); if (cycleInt(iv, MAX_SEQ_TRACKS)) seq.setActiveTrack(iv); break; }
        case 1: { iv = (int)trk->soundPatchIdx; if (adjustInt(iv, 0, 9, 1)) trk->soundPatchIdx = iv; break; }
        case 2: { iv = (int)trk->midiChannel; if (adjustInt(iv, 1, 16, 1)) trk->midiChannel = iv; break; }
        case 3: { iv = (int)trk->midiOutput; if (cycleInt(iv, 3)) trk->midiOutput = iv; break; }
        case 4: { iv = (int)trk->trackLength; if (adjustInt(iv, 1, 16, 1)) trk->trackLength = iv; break; }
        case 5: { iv = (int)trk->octave; if (adjustInt(iv, -3, 3, 1)) trk->octave = iv; break; }
        case 6: if (buttons.pressed(BTN_OK) || buttons.pressed(BTN_RIGHT) || buttons.pressed(BTN_LEFT)) {
          trk->muted = !trk->muted;
        } break;
        case 7: if (buttons.pressed(BTN_OK) || buttons.pressed(BTN_RIGHT) || buttons.pressed(BTN_LEFT)) {
          trk->solo = !trk->solo;
        } break;
      }
      break;
    }

    case SEQP_SCALE: {
      handleUpDown();

      // Q1-Q4: jump to params
      if (buttons.pressed(BTN_Q1)) { uiCursor = 0; }
      if (buttons.pressed(BTN_Q2)) { uiCursor = 1; }
      if (buttons.pressed(BTN_Q3)) { uiCursor = 2; }
      if (buttons.pressed(BTN_Q4)) { uiCursor = 3; }

      int iv;
      float fv;
      switch (uiCursor) {
        case 0: { iv = seq.getGlobalKey(); if (cycleInt(iv, 12)) seq.setGlobalKey(iv); break; }
        case 1: { iv = seq.getGlobalScale(); if (cycleInt(iv, NUM_SCALES_SEQ)) seq.setGlobalScale(iv); break; }
        case 2: if (buttons.pressed(BTN_OK) || buttons.pressed(BTN_RIGHT) || buttons.pressed(BTN_LEFT)) {
          seq.setScaleLock(!seq.getScaleLock());
        } break;
        case 3: { fv = seq.getBPM(); if (adjustFloat(fv, 30, 300, 5.0f, 1.0f)) seq.setBPM(fv); break; }
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
  display.clearDisplay();

  char hdr[22];
  snprintf(hdr, sizeof(hdr), "CHORD %s %s Oct%d",
    noteNames[chordRoot], chordTypes[chordType].name, chordOctave);
  drawHeader(hdr);

  uiItemCount = 5;
  uiClampScroll();

  char val[16];
  for (int i = 0; i < MAX_VISIBLE && (uiScrollTop + i) < uiItemCount; i++) {
    int idx = uiScrollTop + i;
    bool sel = (idx == uiCursor);
    switch (idx) {
      case 0: snprintf(val, sizeof(val), "%s", noteNames[chordRoot]);
              drawTextRow(i, "Root", val, sel); break;
      case 1: snprintf(val, sizeof(val), "%s", chordTypes[chordType].name);
              drawTextRow(i, "Type", val, sel); break;
      case 2: snprintf(val, sizeof(val), "%d", chordOctave);
              drawTextRow(i, "Octave", val, sel); break;
      case 3: snprintf(val, sizeof(val), "%s", chordHold ? "ON" : "OFF");
              drawTextRow(i, "Hold", val, sel); break;
      case 4: snprintf(val, sizeof(val), "[OK]");
              drawTextRow(i, "PLAY", val, sel); break;
    }
  }

  drawScrollIndicators();
  drawFooter("ROOT", "TYPE", "OCT", "HOLD");
  display.display();
}

static void handleChordInput() {
  if (buttons.pressed(BTN_BACK)) { chordStopAll(); exitToMenu(); return; }

  handleUpDown();

  // Q1-Q4: jump to parameters
  if (buttons.pressed(BTN_Q1)) { uiCursor = 0; }
  if (buttons.pressed(BTN_Q2)) { uiCursor = 1; }
  if (buttons.pressed(BTN_Q3)) { uiCursor = 2; }
  if (buttons.pressed(BTN_Q4)) { chordHold = !chordHold; if (!chordHold) chordStopAll(); }

  switch (uiCursor) {
    case 0: cycleInt(chordRoot, 12); break;
    case 1: cycleInt(chordType, NUM_CHORD_TYPES); break;
    case 2: adjustInt(chordOctave, 2, 7, 1); break;
    case 3: if (buttons.pressed(BTN_OK) || buttons.pressed(BTN_RIGHT) || buttons.pressed(BTN_LEFT)) {
      chordHold = !chordHold;
      if (!chordHold) chordStopAll();
    } break;
    case 4: if (buttons.pressed(BTN_OK)) {
      chordPlayNotes();
    } break;
  }

  // PLAY button also triggers chord
  if (buttons.pressed(BTN_PLAY)) {
    chordPlayNotes();
  }
}

// ═════════════════════════════════════════════════════════════════════════════
// PRESETS MODE
// ═════════════════════════════════════════════════════════════════════════════
static void drawPresetsMode() {
  display.clearDisplay();

  char hdr[22];
  snprintf(hdr, sizeof(hdr), "PRESET %c%d",
    (uiCursor < NUM_FACTORY) ? 'F' : 'U', uiCursor);
  drawHeader(hdr);

  uiItemCount = NUM_PRESETS;
  uiClampScroll();

  for (int i = 0; i < MAX_VISIBLE && (uiScrollTop + i) < uiItemCount; i++) {
    int idx = uiScrollTop + i;
    char label[22];
    snprintf(label, sizeof(label), "%c%d:%s",
      (idx < NUM_FACTORY) ? 'F' : 'U',
      idx, presetMgr.getName(idx));
    drawTextRow(i, label, "", idx == uiCursor);
  }

  // Scroll indicators
  drawScrollIndicators();

  // Show preset info in the area above footer
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  if (uiCursor < NUM_FACTORY)
    drawText(2, 52, "[OK]Load");
  else
    drawText(2, 52, "[OK]Load [FN+OK]Save");

  drawFooter("LOAD", "SAVE", "FACT", "USER");
  display.display();
}

static void handlePresetsInput() {
  if (buttons.pressed(BTN_BACK)) { exitToMenu(); return; }

  handleUpDown();

  // Load preset
  if (buttons.pressed(BTN_OK)) {
    if (buttons.isDown(BTN_FN)) {
      // FN+OK = save to user slot
      if (uiCursor >= NUM_FACTORY) {
        saveCurrentToPreset(uiCursor);
      }
    } else {
      // OK = load preset
      presetSelectedSlot = uiCursor;
      loadPresetToSynth(presetSelectedSlot);
    }
  }

  // Q1: load current selection
  if (buttons.pressed(BTN_Q1)) {
    presetSelectedSlot = uiCursor;
    loadPresetToSynth(presetSelectedSlot);
  }

  // Q2: save to current slot (user only)
  if (buttons.pressed(BTN_Q2)) {
    if (uiCursor >= NUM_FACTORY) {
      saveCurrentToPreset(uiCursor);
    }
  }

  // Q3: jump to factory presets
  if (buttons.pressed(BTN_Q3)) {
    uiCursor = 0;
    uiScrollTop = 0;
  }

  // Q4: jump to user presets
  if (buttons.pressed(BTN_Q4)) {
    uiCursor = NUM_FACTORY;
    uiScrollTop = NUM_FACTORY;
  }
}

// ═════════════════════════════════════════════════════════════════════════════
// UI INIT / UPDATE / DISPATCH
// ═════════════════════════════════════════════════════════════════════════════
// I2C scanner – prints all detected devices on the bus via Serial.
// Runs at boot so you can verify the OLED address (usually 0x3C).
static void i2cScan() {
  Serial.println("I2C scan:");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  0x%02X found\n", addr);
      found++;
    }
  }
  if (found == 0) {
    Serial.println("  ** NO devices found — check SDA/SCL wiring! **");
  } else {
    Serial.printf("  %d device(s) on bus\n", found);
  }
}

void uiInit() {
  // ── I2C bus: configure pins FIRST, then begin ──────────────────────────
  Wire.setSDA(OLED_SDA_PIN);
  Wire.setSCL(OLED_SCL_PIN);
  Wire.setClock(OLED_I2C_FREQ);
  Wire.begin();

  // Scan the bus so the user can verify the OLED is detected
  i2cScan();

  // ── Adafruit SH1106 init ──────────────────────────────────────────────
  if (!display.begin(OLED_ADDR, true)) {
    Serial.println("** SH1106 init FAILED — check wiring/address! **");
  } else {
    Serial.println("OLED: Adafruit_SH1106G 128x64 OK");
  }
  Serial.printf("OLED addr : 0x%02X  SDA=%d SCL=%d\n", OLED_ADDR, OLED_SDA_PIN, OLED_SCL_PIN);

  display.setContrast(200);
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setTextWrap(false);

  // ── Splash screen ─────────────────────────────────────────────────────
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 4);
  display.print("ZOMBIE SS");
  display.setTextSize(1);
  display.setCursor(12, 28);
  display.print("PROPHET SYNTHESIZER");
  display.setCursor(28, 41);
  display.print("RP2040 Edition");
  display.display();
  delay(1500);

  uiResetCursor();
  currentMode = MODE_MENU;
}

void uiUpdate() {
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
