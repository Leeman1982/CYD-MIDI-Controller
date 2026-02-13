#ifndef ZOMBIE_SYNTH_MODE_H
#define ZOMBIE_SYNTH_MODE_H

#include "common_definitions.h"
#include "ui_elements.h"
#include "synth_engine.h"
#include "zombie_lfo.h"

// forward declared in ZombieSynth.ino — allows SAVE button to navigate
void enterMode(AppMode mode);

// ZOMBIE SS Prophet-8 Style Synthesizer UI
// Black background, red text, white outlines

static SynthEngine* zombieSynth = NULL;

struct ZombieSynthParams {
  int   osc1Wave;
  float osc1Level;
  int   osc2Wave;
  float osc2Level;
  int   filterType;
  float filterCutoff;
  float filterResonance;
  float filterEnvAmount;
  float ampAttack;
  float ampDecay;
  float ampSustain;
  float ampRelease;
  float filterAttack;
  float filterDecay;
  float filterSustain;
  float filterRelease;
  float masterVolume;
  int   currentPage;
  bool  needsRedraw;
  int   activeSlider;
};

static ZombieSynthParams synthParams;

// Exposed for other modules
extern LFOEngine  globalLFO;
extern int        lastPlayedMidiNote;

const char* waveNames[]   = {"SAW","SQR","TRI","SIN","PLS"};
const char* filterNames[] = {"LP","HP","BP","NOTCH"};

// Returns "C4", "D#3" etc. for a MIDI note number
static const char* midiNoteToName(int note) {
  if (note < 0) return "--";
  static char buf[6];
  const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
  int octave = note / 12 - 1;
  snprintf(buf, sizeof(buf), "%s%d", noteNames[note % 12], octave);
  return buf;
}

void zombieSynthInit() {
  if (zombieSynth == NULL) {
    zombieSynth = new SynthEngine();
    zombieSynth->init();
  }

  synthParams.osc1Wave = WAVE_SAW;  synthParams.osc1Level = 0.5f;
  synthParams.osc2Wave = WAVE_SAW;  synthParams.osc2Level = 0.5f;
  synthParams.filterType = FILTER_LOWPASS;
  synthParams.filterCutoff     = 0.8f;
  synthParams.filterResonance  = 0.3f;
  synthParams.filterEnvAmount  = 0.5f;
  synthParams.ampAttack   = 0.01f; synthParams.ampDecay   = 0.3f;
  synthParams.ampSustain  = 0.7f;  synthParams.ampRelease  = 0.5f;
  synthParams.filterAttack  = 0.01f; synthParams.filterDecay  = 0.3f;
  synthParams.filterSustain = 0.5f;  synthParams.filterRelease = 0.3f;
  synthParams.masterVolume = 0.7f;
  synthParams.currentPage  = 0;
  synthParams.needsRedraw  = true;
  synthParams.activeSlider = -1;

  zombieSynth->setOsc1Waveform((WaveformType)synthParams.osc1Wave);
  zombieSynth->setOsc2Waveform((WaveformType)synthParams.osc2Wave);
  zombieSynth->setFilterType((FilterType)synthParams.filterType);
  zombieSynth->setFilterCutoff(synthParams.filterCutoff);
  zombieSynth->setFilterResonance(synthParams.filterResonance);
  zombieSynth->setAmpEnvelope(synthParams.ampAttack, synthParams.ampDecay,
                               synthParams.ampSustain, synthParams.ampRelease);
  zombieSynth->setFilterEnvelope(synthParams.filterAttack, synthParams.filterDecay,
                                  synthParams.filterSustain, synthParams.filterRelease);
  zombieSynth->setMasterVolume(synthParams.masterVolume);

  tft.fillScreen(THEME_BG);
}

// ─── Header ──────────────────────────────────────────────────────────────────
// 5 tabs: OSC(0) FLTR(1) AMP(2) F.ENV(3) LFO(4)
// Each tab: w=58, gap=2 → 5×60=300, startX=5
static const int TAB_W = 58, TAB_H = 22, TAB_Y = 55;
static const int TAB_X[] = {5, 65, 125, 185, 245};
static const char* TAB_NAMES[] = {"OSC", "FLTR", "AMP", "F.ENV", "LFO"};

static const char* audioOutNames[] = {"PCM5052","INT DAC","SPEAKER"};

void drawZombieHeader() {
  tft.fillRect(0, 0, 320, 50, THEME_BG);
  tft.drawRect(0, 0, 320, 50, THEME_OUTLINE);
  tft.drawRect(1, 1, 318, 48, THEME_OUTLINE);

  // Title (centre-left area, font 4 to leave room for buttons)
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString("ZOMBIE SS", 160, 5, 4);
  tft.setTextColor(THEME_ACCENT, THEME_BG);
  tft.drawCentreString("PROPHET SYNTHESIZER", 160, 33, 2);

  // ── Left: BACK / SAVE ────────────────────────────────────────────────────
  tft.fillRoundRect(3, 3, 50, 21, 3, THEME_PRIMARY);
  tft.drawRoundRect(3, 3, 50, 21, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_BG, THEME_PRIMARY);
  tft.drawCentreString("BACK", 28, 8, 2);

  tft.fillRoundRect(3, 27, 50, 20, 3, THEME_ACCENT);
  tft.drawRoundRect(3, 27, 50, 20, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_BG, THEME_ACCENT);
  tft.drawCentreString("SAVE", 28, 32, 2);

  // ── Right: Note name / OUT button ────────────────────────────────────────
  // Note name (top-right)
  char noteBuf[8];
  snprintf(noteBuf, sizeof(noteBuf), "%s", midiNoteToName(lastPlayedMidiNote));
  tft.setTextColor(THEME_ACCENT, THEME_BG);
  tft.drawRightString(noteBuf, 316, 5, 2);

  // Voice count
  if (zombieSynth) {
    char vbuf[8];
    sprintf(vbuf, "%d/8V", zombieSynth->getActiveVoiceCount());
    tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
    tft.drawRightString(vbuf, 316, 20, 2);
  }

  // Audio output selector (tap to cycle)
  uint16_t outBg = (audioOutputMode == AUDIO_PCM5052) ? THEME_PRIMARY :
                   (audioOutputMode == AUDIO_INTERNAL_DAC) ? 0x07E0 /* green */ : THEME_ACCENT;
  tft.fillRoundRect(259, 30, 58, 17, 3, outBg);
  tft.drawRoundRect(259, 30, 58, 17, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_BG, outBg);
  tft.drawCentreString(audioOutNames[audioOutputMode], 288, 34, 2);

  // 5 page tabs
  for (int i = 0; i < 5; i++) {
    bool sel = (i == synthParams.currentPage);
    uint16_t bg  = sel ? THEME_PRIMARY : THEME_BG;
    uint16_t txt = sel ? THEME_BG : THEME_TEXT_DIM;
    tft.fillRoundRect(TAB_X[i], TAB_Y, TAB_W, TAB_H, 3, bg);
    tft.drawRoundRect(TAB_X[i], TAB_Y, TAB_W, TAB_H, 3, THEME_OUTLINE);
    tft.setTextColor(txt, bg);
    tft.drawCentreString(TAB_NAMES[i], TAB_X[i] + TAB_W/2, TAB_Y + 5, 2);
  }
}

// ─── Slider helper ───────────────────────────────────────────────────────────
void drawVerticalSlider(int x, int y, int w, int h, const char* label, float value,
                        const char* valueText = NULL) {
  tft.fillRoundRect(x, y, w, h, 4, THEME_BG);
  tft.drawRoundRect(x, y, w, h, 4, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString(label, x + w/2, y + 4, 2);

  int trackX = x + w/2 - 8;
  int trackY = y + 24;
  int trackW = 16;
  int trackH = h - 48;
  tft.fillRect(trackX, trackY, trackW, trackH, THEME_BG);
  tft.drawRect(trackX, trackY, trackW, trackH, THEME_OUTLINE);

  int fillH = (int)(trackH * value);
  if (fillH > 0)
    tft.fillRect(trackX+1, trackY + trackH - fillH, trackW-2, fillH, THEME_PRIMARY);

  tft.setTextColor(THEME_ACCENT, THEME_BG);
  if (valueText) {
    tft.drawCentreString(valueText, x + w/2, y + h - 14, 2);
  } else {
    char buf[8];
    sprintf(buf, "%d", (int)(value * 100));
    tft.drawCentreString(buf, x + w/2, y + h - 14, 2);
  }
}

void drawButton(int x, int y, int w, int h, const char* text, bool selected = false) {
  uint16_t bg  = selected ? THEME_PRIMARY : THEME_BG;
  uint16_t txt = selected ? THEME_BG : THEME_PRIMARY;
  tft.fillRoundRect(x, y, w, h, 4, bg);
  tft.drawRoundRect(x, y, w, h, 4, THEME_OUTLINE);
  tft.setTextColor(txt, bg);
  tft.drawCentreString(text, x + w/2, y + h/2 - 8, 2);
}

bool handleSliderTouch(int sx, int sy, int sw, int sh, float& value) {
  if (touch.isPressed && touch.x >= sx && touch.x <= sx+sw &&
      touch.y >= sy && touch.y <= sy+sh) {
    int trackY = sy + 24;
    int trackH = sh - 48;
    value = 1.0f - (float)(touch.y - trackY) / (float)trackH;
    value = constrain(value, 0.0f, 1.0f);
    return true;
  }
  return false;
}

// ─── OSC page ────────────────────────────────────────────────────────────────
void zombieSynthDrawOscPage() {
  drawVerticalSlider(10, 83, 65, 150, "OSC1", synthParams.osc1Level);
  drawButton(80, 93, 50, 24, waveNames[synthParams.osc1Wave], true);
  drawButton(80, 122, 23, 24, "<", false);
  drawButton(107, 122, 23, 24, ">", false);
  drawVerticalSlider(140, 83, 65, 150, "OSC2", synthParams.osc2Level);
  drawButton(210, 93, 50, 24, waveNames[synthParams.osc2Wave], true);
  drawButton(210, 122, 23, 24, "<", false);
  drawButton(237, 122, 23, 24, ">", false);
  drawVerticalSlider(268, 83, 48, 150, "VOL", synthParams.masterVolume);
}

// ─── Filter page ─────────────────────────────────────────────────────────────
void zombieSynthDrawFilterPage() {
  drawVerticalSlider(10, 83, 65, 150, "CUTOFF", synthParams.filterCutoff);
  drawVerticalSlider(80, 83, 65, 150, "RESO",   synthParams.filterResonance);
  drawVerticalSlider(150, 83, 65, 150, "ENV",   synthParams.filterEnvAmount);
  drawButton(225, 93, 85, 28, filterNames[synthParams.filterType], true);
  drawButton(225, 128, 40, 24, "<", false);
  drawButton(270, 128, 40, 24, ">", false);
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawCentreString("TYPE", 267, 160, 2);
}

// ─── Amp envelope page ───────────────────────────────────────────────────────
void zombieSynthDrawAmpEnvPage() {
  char buf[20];
  sprintf(buf, "%.2fs", synthParams.ampAttack);
  drawVerticalSlider(10, 83, 70, 150, "ATK", synthParams.ampAttack/2.0f, buf);
  sprintf(buf, "%.2fs", synthParams.ampDecay);
  drawVerticalSlider(85, 83, 70, 150, "DEC", synthParams.ampDecay, buf);
  drawVerticalSlider(160, 83, 70, 150, "SUS", synthParams.ampSustain);
  sprintf(buf, "%.2fs", synthParams.ampRelease);
  drawVerticalSlider(235, 83, 70, 150, "REL", synthParams.ampRelease, buf);
}

// ─── Filter envelope page ────────────────────────────────────────────────────
void zombieSynthDrawFilterEnvPage() {
  char buf[20];
  sprintf(buf, "%.2fs", synthParams.filterAttack);
  drawVerticalSlider(10, 83, 70, 150, "ATK", synthParams.filterAttack/2.0f, buf);
  sprintf(buf, "%.2fs", synthParams.filterDecay);
  drawVerticalSlider(85, 83, 70, 150, "DEC", synthParams.filterDecay, buf);
  drawVerticalSlider(160, 83, 70, 150, "SUS", synthParams.filterSustain);
  sprintf(buf, "%.2fs", synthParams.filterRelease);
  drawVerticalSlider(235, 83, 70, 150, "REL", synthParams.filterRelease, buf);
}

// ─── LFO page ────────────────────────────────────────────────────────────────
void zombieSynthDrawLFOPage() {
  // Wave type row (y=83..108)
  for (int i = 0; i < 5; i++) {
    bool sel = (i == (int)globalLFO.wave);
    int wx = 5 + i * 62;
    drawButton(wx, 83, 59, 25, lfoWaveNames[i], sel);
  }

  // Rate slider (0-20 Hz)
  char rateBuf[12];
  sprintf(rateBuf, "%.1fHz", globalLFO.rate);
  drawVerticalSlider(12, 113, 65, 108, "RATE", globalLFO.rate / 20.0f, rateBuf);

  // Depth slider
  char depBuf[8];
  sprintf(depBuf, "%d%%", (int)(globalLFO.depth * 100));
  drawVerticalSlider(85, 113, 65, 108, "DEPTH", globalLFO.depth, depBuf);

  // Enable/Disable button
  bool en = globalLFO.enabled;
  tft.fillRoundRect(160, 113, 150, 30, 4, en ? THEME_PRIMARY : THEME_BG);
  tft.drawRoundRect(160, 113, 150, 30, 4, THEME_OUTLINE);
  tft.setTextColor(en ? THEME_BG : THEME_PRIMARY, en ? THEME_PRIMARY : THEME_BG);
  tft.drawCentreString(en ? "LFO: ON" : "LFO: OFF", 235, 121, 2);

  // Target row (FILTER / PITCH / AMP)
  const char* tnames[] = {"FILTER", "PITCH", "AMP"};
  for (int i = 0; i < 3; i++) {
    bool sel = (i == (int)globalLFO.target);
    tft.fillRoundRect(160 + i*50, 150, 47, 24, 3, sel ? THEME_PRIMARY : THEME_BG);
    tft.drawRoundRect(160 + i*50, 150, 47, 24, 3, THEME_OUTLINE);
    tft.setTextColor(sel ? THEME_BG : THEME_PRIMARY, sel ? THEME_PRIMARY : THEME_BG);
    tft.drawCentreString(tnames[i], 160 + i*50 + 23, 156, 2);
  }

  // Target label
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawCentreString("TARGET", 235, 178, 2);
}

// ─── Main draw ───────────────────────────────────────────────────────────────
void zombieSynthDraw() {
  if (!synthParams.needsRedraw) return;

  drawZombieHeader();
  tft.fillRect(0, 80, 320, 160, THEME_BG);

  switch (synthParams.currentPage) {
    case 0: zombieSynthDrawOscPage();       break;
    case 1: zombieSynthDrawFilterPage();    break;
    case 2: zombieSynthDrawAmpEnvPage();    break;
    case 3: zombieSynthDrawFilterEnvPage(); break;
    case 4: zombieSynthDrawLFOPage();       break;
  }

  synthParams.needsRedraw = false;
}

// ─── Touch handler ────────────────────────────────────────────────────────────
void zombieSynthHandleTouch() {
  if (!touch.justPressed && !touch.isPressed) return;

  // BACK button
  if (touch.justPressed && isButtonPressed(3, 3, 50, 21)) { exitToMenu(); return; }

  // SAVE button → navigate to preset page
  if (touch.justPressed && isButtonPressed(3, 27, 50, 20)) {
    enterMode(ZOMBIE_PRESETS);
    return;
  }

  // AUDIO OUTPUT button → cycle PCM5052 → INT DAC → SPEAKER
  if (touch.justPressed && isButtonPressed(259, 30, 58, 17)) {
    audioOutputMode = (AudioOutputMode)((audioOutputMode + 1) % 3);
    SynthEngine* s = getZombieSynth();
    if (s) s->reinitOutput();
    synthParams.needsRedraw = true;
    return;
  }

  // Page tabs
  if (touch.justPressed) {
    for (int i = 0; i < 5; i++) {
      if (isButtonPressed(TAB_X[i], TAB_Y, TAB_W, TAB_H)) {
        synthParams.currentPage = i;
        synthParams.needsRedraw = true;
        return;
      }
    }
  }

  bool changed = false;

  switch (synthParams.currentPage) {
    case 0: {
      if (handleSliderTouch(10, 83, 65, 150, synthParams.osc1Level))   changed = true;
      if (handleSliderTouch(140, 83, 65, 150, synthParams.osc2Level))  changed = true;
      if (handleSliderTouch(268, 83, 48, 150, synthParams.masterVolume)) {
        zombieSynth->setMasterVolume(synthParams.masterVolume);
        changed = true;
      }
      if (touch.justPressed) {
        if (isButtonPressed(80, 122, 23, 24)) { synthParams.osc1Wave = (synthParams.osc1Wave-1+5)%5; zombieSynth->setOsc1Waveform((WaveformType)synthParams.osc1Wave); changed = true; }
        if (isButtonPressed(107,122, 23, 24)) { synthParams.osc1Wave = (synthParams.osc1Wave+1)%5;   zombieSynth->setOsc1Waveform((WaveformType)synthParams.osc1Wave); changed = true; }
        if (isButtonPressed(210,122, 23, 24)) { synthParams.osc2Wave = (synthParams.osc2Wave-1+5)%5; zombieSynth->setOsc2Waveform((WaveformType)synthParams.osc2Wave); changed = true; }
        if (isButtonPressed(237,122, 23, 24)) { synthParams.osc2Wave = (synthParams.osc2Wave+1)%5;   zombieSynth->setOsc2Waveform((WaveformType)synthParams.osc2Wave); changed = true; }
      }
      break;
    }
    case 1: {
      if (handleSliderTouch(10, 83, 65, 150, synthParams.filterCutoff))    { zombieSynth->setFilterCutoff(synthParams.filterCutoff);       changed = true; }
      if (handleSliderTouch(80, 83, 65, 150, synthParams.filterResonance)) { zombieSynth->setFilterResonance(synthParams.filterResonance); changed = true; }
      if (handleSliderTouch(150,83, 65, 150, synthParams.filterEnvAmount)) changed = true;
      if (touch.justPressed) {
        if (isButtonPressed(225,128, 40, 24)) { synthParams.filterType=(synthParams.filterType-1+4)%4; zombieSynth->setFilterType((FilterType)synthParams.filterType); changed=true; }
        if (isButtonPressed(270,128, 40, 24)) { synthParams.filterType=(synthParams.filterType+1)%4;   zombieSynth->setFilterType((FilterType)synthParams.filterType); changed=true; }
      }
      break;
    }
    case 2: {
      if (handleSliderTouch(10, 83, 70, 150, synthParams.ampAttack))  { synthParams.ampAttack*=2.0f; zombieSynth->setAmpEnvelope(synthParams.ampAttack,synthParams.ampDecay,synthParams.ampSustain,synthParams.ampRelease); changed=true; }
      if (handleSliderTouch(85, 83, 70, 150, synthParams.ampDecay))   { zombieSynth->setAmpEnvelope(synthParams.ampAttack,synthParams.ampDecay,synthParams.ampSustain,synthParams.ampRelease); changed=true; }
      if (handleSliderTouch(160,83, 70, 150, synthParams.ampSustain)) { zombieSynth->setAmpEnvelope(synthParams.ampAttack,synthParams.ampDecay,synthParams.ampSustain,synthParams.ampRelease); changed=true; }
      if (handleSliderTouch(235,83, 70, 150, synthParams.ampRelease)) { zombieSynth->setAmpEnvelope(synthParams.ampAttack,synthParams.ampDecay,synthParams.ampSustain,synthParams.ampRelease); changed=true; }
      break;
    }
    case 3: {
      if (handleSliderTouch(10, 83, 70, 150, synthParams.filterAttack))  { synthParams.filterAttack*=2.0f; zombieSynth->setFilterEnvelope(synthParams.filterAttack,synthParams.filterDecay,synthParams.filterSustain,synthParams.filterRelease); changed=true; }
      if (handleSliderTouch(85, 83, 70, 150, synthParams.filterDecay))   { zombieSynth->setFilterEnvelope(synthParams.filterAttack,synthParams.filterDecay,synthParams.filterSustain,synthParams.filterRelease); changed=true; }
      if (handleSliderTouch(160,83, 70, 150, synthParams.filterSustain)) { zombieSynth->setFilterEnvelope(synthParams.filterAttack,synthParams.filterDecay,synthParams.filterSustain,synthParams.filterRelease); changed=true; }
      if (handleSliderTouch(235,83, 70, 150, synthParams.filterRelease)) { zombieSynth->setFilterEnvelope(synthParams.filterAttack,synthParams.filterDecay,synthParams.filterSustain,synthParams.filterRelease); changed=true; }
      break;
    }
    case 4: { // LFO page
      if (touch.justPressed) {
        // Wave type buttons
        for (int i = 0; i < 5; i++) {
          if (isButtonPressed(5 + i*62, 83, 59, 25)) {
            globalLFO.wave = (LFOWave)i;
            changed = true;
          }
        }
        // Enable/disable toggle
        if (isButtonPressed(160, 113, 150, 30)) {
          globalLFO.enabled = !globalLFO.enabled;
          changed = true;
        }
        // Target buttons
        for (int i = 0; i < 3; i++) {
          if (isButtonPressed(160 + i*50, 150, 47, 24)) {
            globalLFO.target = (LFOTarget)i;
            changed = true;
          }
        }
      }
      // Rate slider
      float rateNorm = globalLFO.rate / 20.0f;
      if (handleSliderTouch(12, 113, 65, 108, rateNorm)) {
        globalLFO.rate = rateNorm * 20.0f;
        changed = true;
      }
      // Depth slider
      if (handleSliderTouch(85, 113, 65, 108, globalLFO.depth)) {
        globalLFO.enabled = (globalLFO.depth > 0.001f);
        changed = true;
      }
      break;
    }
  }

  if (changed) synthParams.needsRedraw = true;
}

void zombieSynthUpdate() {
  // Refresh note-name display in header without triggering full redraw
  static int prevNote = -2;
  if (lastPlayedMidiNote != prevNote) {
    prevNote = lastPlayedMidiNote;
    tft.fillRect(270, 33, 48, 14, THEME_BG);
    char noteBuf[8];
    snprintf(noteBuf, sizeof(noteBuf), " %s", midiNoteToName(lastPlayedMidiNote));
    tft.setTextColor(THEME_ACCENT, THEME_BG);
    tft.drawRightString(noteBuf, 316, 34, 2);
  }
}

SynthEngine* getZombieSynth() {
  return zombieSynth;
}

#endif
