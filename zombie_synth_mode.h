#ifndef ZOMBIE_SYNTH_MODE_H
#define ZOMBIE_SYNTH_MODE_H

#include "common_definitions.h"
#include "ui_elements.h"
#include "synth_engine.h"

// ZOMBIE SS Prophet-8 Style Synthesizer UI
// Black background, red text, white outlines

static SynthEngine* zombieSynth = NULL;

struct ZombieSynthParams {
  // Oscillator 1
  int osc1Wave;
  float osc1Level;

  // Oscillator 2
  int osc2Wave;
  float osc2Level;
  float osc2Detune;
  int osc2Semitones;

  // Filter
  int filterType;
  float filterCutoff;
  float filterResonance;
  float filterEnvAmount;

  // Amp Envelope
  float ampAttack;
  float ampDecay;
  float ampSustain;
  float ampRelease;

  // Filter Envelope
  float filterAttack;
  float filterDecay;
  float filterSustain;
  float filterRelease;

  // Master
  float masterVolume;

  // UI state
  int currentPage;  // 0=OSC, 1=FILTER, 2=AMP ENV, 3=FILTER ENV
};

static ZombieSynthParams synthParams;

const char* waveNames[] = {"SAW", "SQR", "TRI", "SIN", "PLS"};
const char* filterNames[] = {"LP", "HP", "BP", "NOTCH"};

void zombieSynthInit() {
  if (zombieSynth == NULL) {
    zombieSynth = new SynthEngine();
    zombieSynth->init();
  }

  // Initialize parameters
  synthParams.osc1Wave = WAVE_SAW;
  synthParams.osc1Level = 0.5f;
  synthParams.osc2Wave = WAVE_SAW;
  synthParams.osc2Level = 0.5f;
  synthParams.osc2Detune = 0.005f;
  synthParams.osc2Semitones = 0;

  synthParams.filterType = FILTER_LOWPASS;
  synthParams.filterCutoff = 0.8f;
  synthParams.filterResonance = 0.3f;
  synthParams.filterEnvAmount = 0.5f;

  synthParams.ampAttack = 0.01f;
  synthParams.ampDecay = 0.3f;
  synthParams.ampSustain = 0.7f;
  synthParams.ampRelease = 0.5f;

  synthParams.filterAttack = 0.01f;
  synthParams.filterDecay = 0.3f;
  synthParams.filterSustain = 0.5f;
  synthParams.filterRelease = 0.3f;

  synthParams.masterVolume = 0.7f;
  synthParams.currentPage = 0;

  // Apply initial settings
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

void drawZombieHeader() {
  // ZOMBIE SS header with German WW2 style font (angular, bold)
  tft.fillRect(0, 0, 320, 50, THEME_BG);
  tft.drawRect(0, 0, 320, 50, THEME_OUTLINE);
  tft.drawRect(1, 1, 318, 48, THEME_OUTLINE);

  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.setTextSize(2);
  tft.drawCentreString("ZOMBIE SS", 160, 10, 4);

  tft.setTextColor(THEME_ACCENT, THEME_BG);
  tft.setTextSize(1);
  tft.drawCentreString("PROPHET SYNTH", 160, 32, 2);

  // Page indicators
  const char* pageNames[] = {"OSC", "FLTR", "AMP", "F.ENV"};
  for (int i = 0; i < 4; i++) {
    int x = 20 + i * 70;
    uint16_t color = (i == synthParams.currentPage) ? THEME_PRIMARY : THEME_TEXT_DIM;
    tft.fillRoundRect(x, 55, 60, 20, 4, THEME_BG);
    tft.drawRoundRect(x, 55, 60, 20, 4, color);
    tft.setTextColor(color, THEME_BG);
    tft.drawCentreString(pageNames[i], x + 30, 59, 2);
  }

  // Voice count
  if (zombieSynth) {
    int voices = zombieSynth->getActiveVoiceCount();
    tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
    char buf[20];
    sprintf(buf, "VOICES:%d", voices);
    tft.drawString(buf, 230, 80, 2);
  }
}

void drawSlider(int x, int y, int w, int h, const char* label, float value, const char* valueText = NULL) {
  // Draw slider with ZOMBIE theme
  tft.fillRoundRect(x, y, w, h, 4, THEME_BG);
  tft.drawRoundRect(x, y, w, h, 4, THEME_OUTLINE);

  // Label
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString(label, x + w / 2, y + 5, 2);

  // Slider track
  int trackY = y + 25;
  int trackH = h - 50;
  tft.drawRect(x + w / 2 - 10, trackY, 20, trackH, THEME_OUTLINE);

  // Slider fill
  int fillH = (int)(trackH * value);
  tft.fillRect(x + w / 2 - 9, trackY + trackH - fillH, 18, fillH, THEME_PRIMARY);

  // Value text
  tft.setTextColor(THEME_ACCENT, THEME_BG);
  if (valueText) {
    tft.drawCentreString(valueText, x + w / 2, y + h - 18, 2);
  } else {
    char buf[10];
    sprintf(buf, "%d", (int)(value * 100));
    tft.drawCentreString(buf, x + w / 2, y + h - 18, 2);
  }
}

void drawButton(int x, int y, int w, int h, const char* text, bool selected = false) {
  uint16_t bgColor = selected ? THEME_PRIMARY : THEME_BG;
  uint16_t textColor = selected ? THEME_BG : THEME_PRIMARY;

  tft.fillRoundRect(x, y, w, h, 4, bgColor);
  tft.drawRoundRect(x, y, w, h, 4, THEME_OUTLINE);
  tft.setTextColor(textColor, bgColor);
  tft.drawCentreString(text, x + w / 2, y + h / 2 - 8, 2);
}

void zombieSynthDrawOscPage() {
  tft.fillRect(0, 95, 320, 145, THEME_BG);

  // OSC 1
  drawSlider(10, 100, 70, 140, "OSC1", synthParams.osc1Level);
  drawButton(85, 110, 50, 30, waveNames[synthParams.osc1Wave], true);
  drawButton(85, 145, 50, 30, "<", false);
  drawButton(85, 180, 50, 30, ">", false);

  // OSC 2
  drawSlider(150, 100, 70, 140, "OSC2", synthParams.osc2Level);
  drawButton(225, 110, 50, 30, waveNames[synthParams.osc2Wave], true);
  drawButton(225, 145, 50, 30, "<", false);
  drawButton(225, 180, 50, 30, ">", false);

  // Master Volume
  drawSlider(245, 100, 65, 140, "VOL", synthParams.masterVolume);
}

void zombieSynthDrawFilterPage() {
  tft.fillRect(0, 95, 320, 145, THEME_BG);

  // Cutoff
  drawSlider(10, 100, 70, 140, "CUTOFF", synthParams.filterCutoff);

  // Resonance
  drawSlider(85, 100, 70, 140, "RESO", synthParams.filterResonance);

  // Envelope Amount
  drawSlider(160, 100, 70, 140, "ENV", synthParams.filterEnvAmount);

  // Filter Type
  drawButton(240, 110, 70, 30, filterNames[synthParams.filterType], true);
  drawButton(240, 145, 35, 30, "<", false);
  drawButton(275, 145, 35, 30, ">", false);

  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawCentreString("FILTER TYPE", 275, 185, 2);
}

void zombieSynthDrawAmpEnvPage() {
  tft.fillRect(0, 95, 320, 145, THEME_BG);

  char buf[20];

  // Attack
  sprintf(buf, "%.2fs", synthParams.ampAttack);
  drawSlider(10, 100, 70, 140, "ATK", synthParams.ampAttack * 2.0f, buf);

  // Decay
  sprintf(buf, "%.2fs", synthParams.ampDecay);
  drawSlider(85, 100, 70, 140, "DEC", synthParams.ampDecay, buf);

  // Sustain
  drawSlider(160, 100, 70, 140, "SUS", synthParams.ampSustain);

  // Release
  sprintf(buf, "%.2fs", synthParams.ampRelease);
  drawSlider(235, 100, 70, 140, "REL", synthParams.ampRelease, buf);
}

void zombieSynthDrawFilterEnvPage() {
  tft.fillRect(0, 95, 320, 145, THEME_BG);

  char buf[20];

  // Attack
  sprintf(buf, "%.2fs", synthParams.filterAttack);
  drawSlider(10, 100, 70, 140, "ATK", synthParams.filterAttack * 2.0f, buf);

  // Decay
  sprintf(buf, "%.2fs", synthParams.filterDecay);
  drawSlider(85, 100, 70, 140, "DEC", synthParams.filterDecay, buf);

  // Sustain
  drawSlider(160, 100, 70, 140, "SUS", synthParams.filterSustain);

  // Release
  sprintf(buf, "%.2fs", synthParams.filterRelease);
  drawSlider(235, 100, 70, 140, "REL", synthParams.filterRelease, buf);
}

void zombieSynthDraw() {
  static int lastPage = -1;

  drawZombieHeader();

  if (lastPage != synthParams.currentPage) {
    lastPage = synthParams.currentPage;
    tft.fillRect(0, 95, 320, 145, THEME_BG);
  }

  switch (synthParams.currentPage) {
    case 0: zombieSynthDrawOscPage(); break;
    case 1: zombieSynthDrawFilterPage(); break;
    case 2: zombieSynthDrawAmpEnvPage(); break;
    case 3: zombieSynthDrawFilterEnvPage(); break;
  }
}

void zombieSynthHandleTouch() {
  if (!touch.justPressed) return;

  // Check page tabs
  for (int i = 0; i < 4; i++) {
    int x = 20 + i * 70;
    if (isButtonPressed(x, 55, 60, 20)) {
      synthParams.currentPage = i;
      return;
    }
  }

  // Page-specific touch handling
  switch (synthParams.currentPage) {
    case 0: { // OSC page
      // OSC1 wave buttons
      if (isButtonPressed(85, 145, 50, 30)) {
        synthParams.osc1Wave = (synthParams.osc1Wave - 1 + 5) % 5;
        zombieSynth->setOsc1Waveform((WaveformType)synthParams.osc1Wave);
      }
      if (isButtonPressed(85, 180, 50, 30)) {
        synthParams.osc1Wave = (synthParams.osc1Wave + 1) % 5;
        zombieSynth->setOsc1Waveform((WaveformType)synthParams.osc1Wave);
      }

      // OSC2 wave buttons
      if (isButtonPressed(225, 145, 50, 30)) {
        synthParams.osc2Wave = (synthParams.osc2Wave - 1 + 5) % 5;
        zombieSynth->setOsc2Waveform((WaveformType)synthParams.osc2Wave);
      }
      if (isButtonPressed(225, 180, 50, 30)) {
        synthParams.osc2Wave = (synthParams.osc2Wave + 1) % 5;
        zombieSynth->setOsc2Waveform((WaveformType)synthParams.osc2Wave);
      }
      break;
    }

    case 1: { // Filter page
      // Filter type buttons
      if (isButtonPressed(240, 145, 35, 30)) {
        synthParams.filterType = (synthParams.filterType - 1 + 4) % 4;
        zombieSynth->setFilterType((FilterType)synthParams.filterType);
      }
      if (isButtonPressed(275, 145, 35, 30)) {
        synthParams.filterType = (synthParams.filterType + 1) % 4;
        zombieSynth->setFilterType((FilterType)synthParams.filterType);
      }
      break;
    }
  }
}

void zombieSynthUpdate() {
  // Process audio in background (should be in separate task)
  if (zombieSynth) {
    zombieSynth->processAudio();
  }
}

SynthEngine* getZombieSynth() {
  return zombieSynth;
}

#endif
