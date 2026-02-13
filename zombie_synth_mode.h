#ifndef ZOMBIE_SYNTH_MODE_H
#define ZOMBIE_SYNTH_MODE_H

#include "common_definitions.h"
#include "ui_elements.h"
#include "synth_engine.h"
#include "zombie_lfo.h"

// forward declaration (ZombieSynth.ino)
void enterMode(AppMode mode);

static SynthEngine* zombieSynth = NULL;

struct ZombieSynthParams {
  int   osc1Wave;
  float osc1Level;
  int   osc2Wave;
  float osc2Level;
  float osc2Detune;     // 0-1 UI → 0..0.02 ratio
  float osc2Semitones;  // -12..+12
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

extern LFOEngine  globalLFO;
extern int        lastPlayedMidiNote;

// 7 waveforms (extended)
const char* waveNames[] = {"SAW","SQR","TRI","SIN","PLS","NOI","SUP"};
static const int NUM_WAVES = 7;

const char* filterNames[] = {"LP","HP","BP","NOTCH"};

static const char* audioOutNames[] = {"PCM5052","INT DAC","SPEAKER"};

static const char* midiNoteToName(int note) {
  if (note < 0) return "--";
  static char buf[6];
  const char* nn[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
  snprintf(buf, sizeof(buf), "%s%d", nn[note % 12], note/12 - 1);
  return buf;
}

// ─── Tabs: 6 pages ────────────────────────────────────────────────────────────
//  OSC(0) FLTR(1) AMP(2) FENV(3) LFO(4) FX(5)
//  6×48 = 288px + 7×2 = 14px gap → total 302, startX=9
static const int TAB_W = 48, TAB_H = 22, TAB_Y = 55;
static const int TAB_X[] = {5, 55, 105, 155, 205, 258};
static const char* TAB_NAMES[] = {"OSC", "FLTR", "AMP", "FENV", "LFO", "FX"};
static const int NUM_TABS = 6;

void zombieSynthInit() {
  if (zombieSynth == NULL) {
    zombieSynth = new SynthEngine();
    zombieSynth->init();
  }

  synthParams.osc1Wave = WAVE_SAW;  synthParams.osc1Level = 0.5f;
  synthParams.osc2Wave = WAVE_SAW;  synthParams.osc2Level = 0.5f;
  synthParams.osc2Detune    = 0.25f;  // UI 0.25 → 0.005 ratio
  synthParams.osc2Semitones = 0.0f;
  synthParams.filterType      = FILTER_LOWPASS;
  synthParams.filterCutoff    = 0.8f;
  synthParams.filterResonance = 0.3f;
  synthParams.filterEnvAmount = 0.5f;
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
void drawZombieHeader() {
  tft.fillRect(0, 0, 320, 50, THEME_BG);
  tft.drawRect(0, 0, 320, 50, THEME_OUTLINE);
  tft.drawRect(1, 1, 318, 48, THEME_OUTLINE);

  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString("ZOMBIE SS", 160, 5, 4);
  tft.setTextColor(THEME_ACCENT, THEME_BG);
  tft.drawCentreString("PROPHET SYNTHESIZER", 160, 33, 2);

  // BACK
  tft.fillRoundRect(3, 3, 50, 21, 3, THEME_PRIMARY);
  tft.drawRoundRect(3, 3, 50, 21, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_BG, THEME_PRIMARY);
  tft.drawCentreString("BACK", 28, 8, 2);

  // SAVE
  tft.fillRoundRect(3, 27, 50, 20, 3, THEME_ACCENT);
  tft.drawRoundRect(3, 27, 50, 20, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_BG, THEME_ACCENT);
  tft.drawCentreString("SAVE", 28, 32, 2);

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

  // Audio output selector
  uint16_t outBg = (audioOutputMode == AUDIO_PCM5052) ? THEME_PRIMARY :
                   (audioOutputMode == AUDIO_INTERNAL_DAC) ? 0x07E0 : THEME_ACCENT;
  tft.fillRoundRect(259, 30, 58, 17, 3, outBg);
  tft.drawRoundRect(259, 30, 58, 17, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_BG, outBg);
  tft.drawCentreString(audioOutNames[audioOutputMode], 288, 34, 2);

  // 6 page tabs
  for (int i = 0; i < NUM_TABS; i++) {
    bool sel = (i == synthParams.currentPage);
    uint16_t bg  = sel ? THEME_PRIMARY : THEME_BG;
    uint16_t txt = sel ? THEME_BG : THEME_TEXT_DIM;
    tft.fillRoundRect(TAB_X[i], TAB_Y, TAB_W, TAB_H, 3, bg);
    tft.drawRoundRect(TAB_X[i], TAB_Y, TAB_W, TAB_H, 3, THEME_OUTLINE);
    tft.setTextColor(txt, bg);
    tft.drawCentreString(TAB_NAMES[i], TAB_X[i] + TAB_W/2, TAB_Y + 5, 2);
  }
}

// ─── Widget helpers ───────────────────────────────────────────────────────────
void drawVerticalSlider(int x, int y, int w, int h, const char* label, float value,
                        const char* valueText = NULL) {
  tft.fillRoundRect(x, y, w, h, 4, THEME_BG);
  tft.drawRoundRect(x, y, w, h, 4, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString(label, x + w/2, y + 4, 2);

  int trackX = x + w/2 - 8, trackY = y + 24;
  int trackW = 16, trackH = h - 48;
  tft.fillRect(trackX, trackY, trackW, trackH, THEME_BG);
  tft.drawRect(trackX, trackY, trackW, trackH, THEME_OUTLINE);

  int fillH = (int)(trackH * value);
  if (fillH > 0)
    tft.fillRect(trackX+1, trackY + trackH - fillH, trackW-2, fillH, THEME_PRIMARY);

  tft.setTextColor(THEME_ACCENT, THEME_BG);
  if (valueText) tft.drawCentreString(valueText, x + w/2, y + h - 14, 2);
  else { char buf[8]; sprintf(buf, "%d", (int)(value*100)); tft.drawCentreString(buf, x+w/2, y+h-14, 2); }
}

// Compact horizontal slider: label(w~38) | bar(w~barW) | valText right-aligned
void drawHorzSlider(int x, int y, int w, int h, const char* label, float value,
                    const char* valText = NULL) {
  tft.fillRect(x, y, w, h, THEME_BG);
  tft.drawRect(x, y, w, h, THEME_OUTLINE);
  int lw = 36; // label area
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawString(label, x + 2, y + h/2 - 6, 2);

  int bx = x + lw + 2, bw = w - lw - 4 - 26, bh = h - 6, by = y + 3;
  tft.drawRect(bx, by, bw, bh, THEME_OUTLINE);
  int fillW = (int)(bw * value);
  if (fillW > 0) tft.fillRect(bx+1, by+1, fillW, bh-2, THEME_PRIMARY);

  tft.setTextColor(THEME_ACCENT, THEME_BG);
  char tmp[10];
  if (!valText) { sprintf(tmp, "%d%%", (int)(value*100)); valText = tmp; }
  tft.drawRightString(valText, x + w - 1, y + h/2 - 6, 2);
}

bool handleHorzSlider(int x, int y, int w, int h, float& value) {
  int lw = 36;
  int bx = x + lw + 2, bw = w - lw - 4 - 26;
  if (touch.isPressed && touch.x >= bx && touch.x <= bx+bw &&
      touch.y >= y && touch.y <= y+h) {
    value = (float)(touch.x - bx) / (float)bw;
    value = constrain(value, 0.0f, 1.0f);
    return true;
  }
  return false;
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
    int trackY = sy + 24, trackH = sh - 48;
    value = 1.0f - (float)(touch.y - trackY) / (float)trackH;
    value = constrain(value, 0.0f, 1.0f);
    return true;
  }
  return false;
}

// ─── OSC page ────────────────────────────────────────────────────────────────
void zombieSynthDrawOscPage() {
  // OSC1
  drawVerticalSlider(10, 83, 55, 150, "OSC1", synthParams.osc1Level);
  drawButton(70, 93, 55, 24, waveNames[synthParams.osc1Wave], true);
  drawButton(70, 122, 26, 24, "<", false);
  drawButton(99, 122, 26, 24, ">", false);

  // OSC2
  drawVerticalSlider(133, 83, 55, 150, "OSC2", synthParams.osc2Level);
  drawButton(193, 93, 55, 24, waveNames[synthParams.osc2Wave], true);
  drawButton(193, 122, 26, 24, "<", false);
  drawButton(222, 122, 26, 24, ">", false);

  // Detune + Semitones (right column, stacked)
  char detBuf[10];
  sprintf(detBuf, "%.3f", synthParams.osc2Detune * 0.02f);
  drawVerticalSlider(255, 83, 58, 70, "DETU", synthParams.osc2Detune, detBuf);
  char semBuf[8];
  float semiNorm = (synthParams.osc2Semitones + 12.0f) / 24.0f;
  sprintf(semBuf, "%+.0f", synthParams.osc2Semitones);
  drawVerticalSlider(255, 158, 58, 75, "SEMI", semiNorm, semBuf);
}

// ─── Filter page ─────────────────────────────────────────────────────────────
void zombieSynthDrawFilterPage() {
  drawVerticalSlider(10, 83, 65, 150, "CUTOFF", synthParams.filterCutoff);
  drawVerticalSlider(80, 83, 65, 150, "RESO",   synthParams.filterResonance);
  drawVerticalSlider(150, 83, 65, 150, "ENV",    synthParams.filterEnvAmount);
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
  // Wave type row
  for (int i = 0; i < 5; i++) {
    bool sel = (i == (int)globalLFO.wave);
    drawButton(5 + i*62, 83, 59, 25, lfoWaveNames[i], sel);
  }

  // Rate slider
  char rateBuf[12];
  sprintf(rateBuf, "%.1fHz", globalLFO.rate);
  drawVerticalSlider(12, 113, 65, 108, "RATE", globalLFO.rate / 20.0f, rateBuf);

  // Depth slider
  char depBuf[8];
  sprintf(depBuf, "%d%%", (int)(globalLFO.depth * 100));
  drawVerticalSlider(85, 113, 65, 108, "DEPTH", globalLFO.depth, depBuf);

  // Enable toggle
  bool en = globalLFO.enabled;
  tft.fillRoundRect(160, 113, 150, 28, 4, en ? THEME_PRIMARY : THEME_BG);
  tft.drawRoundRect(160, 113, 150, 28, 4, THEME_OUTLINE);
  tft.setTextColor(en ? THEME_BG : THEME_PRIMARY, en ? THEME_PRIMARY : THEME_BG);
  tft.drawCentreString(en ? "LFO: ON" : "LFO: OFF", 235, 121, 2);

  // Target: 6 targets in 2 rows of 3 (FILTER/PITCH/AMP | RESON/PW/DETUN)
  for (int i = 0; i < 6; i++) {
    bool sel = (i == (int)globalLFO.target);
    int col = i % 3, row = i / 3;
    int tx = 160 + col * 50, ty = 147 + row * 26;
    tft.fillRoundRect(tx, ty, 47, 23, 3, sel ? THEME_ACCENT : THEME_BG);
    tft.drawRoundRect(tx, ty, 47, 23, 3, THEME_OUTLINE);
    tft.setTextColor(sel ? THEME_BG : THEME_TEXT_DIM, sel ? THEME_ACCENT : THEME_BG);
    tft.drawCentreString(lfoTargetNames[i], tx + 23, ty + 6, 2);
  }
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawCentreString("TARGET", 235, 200, 2);
}

// ─── FX page ─────────────────────────────────────────────────────────────────
// Three sections side-by-side: CHORUS | DELAY | REVERB
// Each section has ON/OFF + 3 horizontal sliders

void zombieSynthDrawFXPage() {
  if (!zombieSynth) return;
  ZombieEffects& fx = zombieSynth->fx;

  // Build chorus display values
  char chRateBuf[12], chDepBuf[12], chMixBuf[12];
  sprintf(chRateBuf, "%.1fHz",  fx.chorus.rate);
  sprintf(chDepBuf, "%.0fms",  fx.chorus.depth * 1000.0f);
  sprintf(chMixBuf, "%d%%",    (int)(fx.chorus.mix * 100));

  char dlTimBuf[12], dlFdbBuf[12], dlMixBuf[12];
  sprintf(dlTimBuf, "%dms",    (int)fx.delay.delayMs);
  sprintf(dlFdbBuf, "%d%%",    (int)(fx.delay.feedback * 100));
  sprintf(dlMixBuf, "%d%%",    (int)(fx.delay.mix * 100));

  char rvRomBuf[12], rvDamBuf[12], rvMixBuf[12];
  sprintf(rvRomBuf, "%d%%",    (int)(fx.reverb.roomSize * 100));
  sprintf(rvDamBuf, "%d%%",    (int)(fx.reverb.damping * 100));
  sprintf(rvMixBuf, "%d%%",    (int)(fx.reverb.mix * 100));

  // Section column widths: 3 equal columns, gap=2, total 320px
  // col0: x=2 w=103, col1: x=107 w=103, col2: x=212 w=106
  const int colX[3] = {2, 107, 212};
  const int colW[3] = {103, 103, 106};
  const int sy0 = 83; // start y

  struct ColDef {
    const char* title;
    bool        on;
    float       s0, s1, s2;
    const char* v0, *v1, *v2;
  } cols[3] = {
    {"CHORUS", fx.chorus.enabled, fx.chorus.rate/8.0f, fx.chorus.depth/0.01f, fx.chorus.mix, chRateBuf, chDepBuf, chMixBuf},
    {"DELAY",  fx.delay.enabled,  fx.delay.delayMs/750.0f, fx.delay.feedback/0.9f, fx.delay.mix, dlTimBuf, dlFdbBuf, dlMixBuf},
    {"REVERB", fx.reverb.enabled, fx.reverb.roomSize, fx.reverb.damping, fx.reverb.mix, rvRomBuf, rvDamBuf, rvMixBuf},
  };

  const char* rowLabels[3][3] = {
    {"RATE","DEPT","MIX "},
    {"TIME","FDBK","MIX "},
    {"ROOM","DAMP","MIX "},
  };

  for (int c = 0; c < 3; c++) {
    int x = colX[c], w = colW[c];
    tft.drawRect(x, sy0-1, w, 156, THEME_OUTLINE);

    // Title + ON button on same row
    tft.setTextColor(THEME_PRIMARY, THEME_BG);
    tft.drawString(cols[c].title, x+3, sy0+3, 2);
    bool on = cols[c].on;
    uint16_t onBg = on ? THEME_ACCENT : THEME_BG;
    tft.fillRoundRect(x + w - 32, sy0+1, 30, 18, 3, onBg);
    tft.drawRoundRect(x + w - 32, sy0+1, 30, 18, 3, THEME_OUTLINE);
    tft.setTextColor(on ? THEME_BG : THEME_TEXT_DIM, onBg);
    tft.drawCentreString(on ? "ON" : "OFF", x + w - 17, sy0 + 4, 2);

    // 3 sliders
    float vals[3] = {cols[c].s0, cols[c].s1, cols[c].s2};
    const char* vtxts[3] = {cols[c].v0, cols[c].v1, cols[c].v2};
    for (int r = 0; r < 3; r++) {
      drawHorzSlider(x+2, sy0 + 24 + r*42, w-4, 36, rowLabels[c][r], vals[r], vtxts[r]);
    }
  }
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
    case 5: zombieSynthDrawFXPage();        break;
  }
  synthParams.needsRedraw = false;
}

// ─── Touch handler ────────────────────────────────────────────────────────────
void zombieSynthHandleTouch() {
  if (!touch.justPressed && !touch.isPressed) return;

  // BACK
  if (touch.justPressed && isButtonPressed(3, 3, 50, 21)) { exitToMenu(); return; }

  // SAVE → presets
  if (touch.justPressed && isButtonPressed(3, 27, 50, 20)) { enterMode(ZOMBIE_PRESETS); return; }

  // Audio output toggle
  if (touch.justPressed && isButtonPressed(259, 30, 58, 17)) {
    audioOutputMode = (AudioOutputMode)((audioOutputMode + 1) % 3);
    SynthEngine* s = getZombieSynth();
    if (s) s->reinitOutput();
    synthParams.needsRedraw = true;
    return;
  }

  // Page tabs
  if (touch.justPressed) {
    for (int i = 0; i < NUM_TABS; i++) {
      if (isButtonPressed(TAB_X[i], TAB_Y, TAB_W, TAB_H)) {
        synthParams.currentPage = i;
        synthParams.needsRedraw = true;
        return;
      }
    }
  }

  bool changed = false;

  switch (synthParams.currentPage) {
    case 0: { // OSC
      if (handleSliderTouch(10, 83, 55, 150, synthParams.osc1Level))   changed = true;
      if (handleSliderTouch(133, 83, 55, 150, synthParams.osc2Level))  changed = true;
      // Detune slider
      if (handleSliderTouch(255, 83, 58, 70, synthParams.osc2Detune)) {
        zombieSynth->setOsc2Detune(synthParams.osc2Detune * 0.02f);
        changed = true;
      }
      // Semitone slider: 0-1 → -12..+12
      float semiNorm = (synthParams.osc2Semitones + 12.0f) / 24.0f;
      if (handleSliderTouch(255, 160, 58, 72, semiNorm)) {
        synthParams.osc2Semitones = semiNorm * 24.0f - 12.0f;
        // Snap to nearest semitone
        synthParams.osc2Semitones = roundf(synthParams.osc2Semitones);
        zombieSynth->setOsc2Semitones(synthParams.osc2Semitones);
        changed = true;
      }
      if (touch.justPressed) {
        if (isButtonPressed(70,  122, 26, 24)) { synthParams.osc1Wave=(synthParams.osc1Wave-1+NUM_WAVES)%NUM_WAVES; zombieSynth->setOsc1Waveform((WaveformType)synthParams.osc1Wave); changed=true; }
        if (isButtonPressed(99,  122, 26, 24)) { synthParams.osc1Wave=(synthParams.osc1Wave+1)%NUM_WAVES;           zombieSynth->setOsc1Waveform((WaveformType)synthParams.osc1Wave); changed=true; }
        if (isButtonPressed(193, 122, 26, 24)) { synthParams.osc2Wave=(synthParams.osc2Wave-1+NUM_WAVES)%NUM_WAVES; zombieSynth->setOsc2Waveform((WaveformType)synthParams.osc2Wave); changed=true; }
        if (isButtonPressed(222, 122, 26, 24)) { synthParams.osc2Wave=(synthParams.osc2Wave+1)%NUM_WAVES;           zombieSynth->setOsc2Waveform((WaveformType)synthParams.osc2Wave); changed=true; }
      }
      break;
    }
    case 1: { // Filter
      if (handleSliderTouch(10,  83, 65, 150, synthParams.filterCutoff))    { zombieSynth->setFilterCutoff(synthParams.filterCutoff);       changed=true; }
      if (handleSliderTouch(80,  83, 65, 150, synthParams.filterResonance)) { zombieSynth->setFilterResonance(synthParams.filterResonance); changed=true; }
      if (handleSliderTouch(150, 83, 65, 150, synthParams.filterEnvAmount)) changed=true;
      if (touch.justPressed) {
        if (isButtonPressed(225,128,40,24)) { synthParams.filterType=(synthParams.filterType-1+4)%4; zombieSynth->setFilterType((FilterType)synthParams.filterType); changed=true; }
        if (isButtonPressed(270,128,40,24)) { synthParams.filterType=(synthParams.filterType+1)%4;   zombieSynth->setFilterType((FilterType)synthParams.filterType); changed=true; }
      }
      break;
    }
    case 2: { // Amp env
      if (handleSliderTouch(10,  83,70,150,synthParams.ampAttack))  { synthParams.ampAttack*=2.0f; zombieSynth->setAmpEnvelope(synthParams.ampAttack,synthParams.ampDecay,synthParams.ampSustain,synthParams.ampRelease); changed=true; }
      if (handleSliderTouch(85,  83,70,150,synthParams.ampDecay))   { zombieSynth->setAmpEnvelope(synthParams.ampAttack,synthParams.ampDecay,synthParams.ampSustain,synthParams.ampRelease); changed=true; }
      if (handleSliderTouch(160, 83,70,150,synthParams.ampSustain)) { zombieSynth->setAmpEnvelope(synthParams.ampAttack,synthParams.ampDecay,synthParams.ampSustain,synthParams.ampRelease); changed=true; }
      if (handleSliderTouch(235, 83,70,150,synthParams.ampRelease)) { zombieSynth->setAmpEnvelope(synthParams.ampAttack,synthParams.ampDecay,synthParams.ampSustain,synthParams.ampRelease); changed=true; }
      break;
    }
    case 3: { // Filter env
      if (handleSliderTouch(10,  83,70,150,synthParams.filterAttack))  { synthParams.filterAttack*=2.0f; zombieSynth->setFilterEnvelope(synthParams.filterAttack,synthParams.filterDecay,synthParams.filterSustain,synthParams.filterRelease); changed=true; }
      if (handleSliderTouch(85,  83,70,150,synthParams.filterDecay))   { zombieSynth->setFilterEnvelope(synthParams.filterAttack,synthParams.filterDecay,synthParams.filterSustain,synthParams.filterRelease); changed=true; }
      if (handleSliderTouch(160, 83,70,150,synthParams.filterSustain)) { zombieSynth->setFilterEnvelope(synthParams.filterAttack,synthParams.filterDecay,synthParams.filterSustain,synthParams.filterRelease); changed=true; }
      if (handleSliderTouch(235, 83,70,150,synthParams.filterRelease)) { zombieSynth->setFilterEnvelope(synthParams.filterAttack,synthParams.filterDecay,synthParams.filterSustain,synthParams.filterRelease); changed=true; }
      break;
    }
    case 4: { // LFO
      if (touch.justPressed) {
        // Wave type
        for (int i = 0; i < 5; i++)
          if (isButtonPressed(5 + i*62, 83, 59, 25)) { globalLFO.wave = (LFOWave)i; changed=true; }
        // Enable toggle
        if (isButtonPressed(160, 113, 150, 28)) { globalLFO.enabled = !globalLFO.enabled; changed=true; }
        // Target (2 rows of 3)
        for (int i = 0; i < 6; i++) {
          int col=i%3, row=i/3;
          int tx=160+col*50, ty=147+row*26;
          if (isButtonPressed(tx, ty, 47, 23)) { globalLFO.target=(LFOTarget)i; changed=true; }
        }
      }
      float rateNorm = globalLFO.rate / 20.0f;
      if (handleSliderTouch(12, 113, 65, 108, rateNorm)) { globalLFO.rate = rateNorm*20.0f; changed=true; }
      if (handleSliderTouch(85, 113, 65, 108, globalLFO.depth)) { globalLFO.enabled=(globalLFO.depth>0.001f); changed=true; }
      break;
    }
    case 5: { // FX
      if (!zombieSynth) break;
      ZombieEffects& fx = zombieSynth->fx;
      const int colX[3] = {2, 107, 212};
      const int colW[3] = {103, 103, 106};
      const int sy0 = 83;

      // ON/OFF toggle for each section
      if (touch.justPressed) {
        for (int c = 0; c < 3; c++) {
          int x = colX[c], w = colW[c];
          if (isButtonPressed(x + w - 32, sy0+1, 30, 18)) {
            if (c==0) fx.chorus.enabled = !fx.chorus.enabled;
            else if (c==1) { fx.delay.enabled = !fx.delay.enabled; }
            else           { fx.reverb.enabled = !fx.reverb.enabled; }
            changed = true;
          }
        }
      }

      // Chorus sliders
      float chRateN = fx.chorus.rate / 8.0f;
      float chDepN  = fx.chorus.depth / 0.01f;
      if (handleHorzSlider(colX[0]+2, sy0+24,    colW[0]-4, 36, chRateN)) { fx.chorus.rate  = chRateN * 8.0f; changed=true; }
      if (handleHorzSlider(colX[0]+2, sy0+24+42, colW[0]-4, 36, chDepN))  { fx.chorus.depth = chDepN * 0.01f; changed=true; }
      if (handleHorzSlider(colX[0]+2, sy0+24+84, colW[0]-4, 36, fx.chorus.mix)) changed=true;

      // Delay sliders
      float dlTimeN = fx.delay.delayMs / 750.0f;
      float dlFdbN  = fx.delay.feedback / 0.9f;
      if (handleHorzSlider(colX[1]+2, sy0+24,    colW[1]-4, 36, dlTimeN)) { fx.delay.delayMs  = dlTimeN * 750.0f; changed=true; }
      if (handleHorzSlider(colX[1]+2, sy0+24+42, colW[1]-4, 36, dlFdbN))  { fx.delay.feedback = dlFdbN * 0.9f; changed=true; }
      if (handleHorzSlider(colX[1]+2, sy0+24+84, colW[1]-4, 36, fx.delay.mix)) changed=true;

      // Reverb sliders
      if (handleHorzSlider(colX[2]+2, sy0+24,    colW[2]-4, 36, fx.reverb.roomSize)) { fx.reverb.updateParams(); changed=true; }
      if (handleHorzSlider(colX[2]+2, sy0+24+42, colW[2]-4, 36, fx.reverb.damping))  { fx.reverb.updateParams(); changed=true; }
      if (handleHorzSlider(colX[2]+2, sy0+24+84, colW[2]-4, 36, fx.reverb.mix)) changed=true;
      break;
    }
  }

  if (changed) synthParams.needsRedraw = true;
}

void zombieSynthUpdate() {
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
