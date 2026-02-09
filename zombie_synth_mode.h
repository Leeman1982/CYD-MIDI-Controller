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
  int currentPage;
  bool needsRedraw;
  int activeSlider;  // -1 = none, 0-7 = slider index
};

static ZombieSynthParams synthParams;

const char* waveNames[] = {"SAW", "SQR", "TRI", "SIN", "PLS"};
const char* filterNames[] = {"LP", "HP", "BP", "NOTCH"};

void zombieSynthInit() {
  if (zombieSynth == NULL) {
    zombieSynth = new SynthEngine();
    zombieSynth->init();
  }

  synthParams.osc1Wave = WAVE_SAW;
  synthParams.osc1Level = 0.5f;
  synthParams.osc2Wave = WAVE_SAW;
  synthParams.osc2Level = 0.5f;

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
  synthParams.needsRedraw = true;
  synthParams.activeSlider = -1;

  // Apply settings
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
  // ZOMBIE SS header - bold angular style
  tft.fillRect(0, 0, 320, 50, THEME_BG);
  tft.fillRect(2, 2, 316, 46, THEME_BG);
  tft.drawRect(0, 0, 320, 50, THEME_OUTLINE);
  tft.drawRect(1, 1, 318, 48, THEME_OUTLINE);

  // ZOMBIE SS in large bold font
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawString("ZOMBIE SS", 95, 8, 6);  // Font 6 = large 48px

  // PROPHET SYNTH subtitle
  tft.setTextColor(THEME_ACCENT, THEME_BG);
  tft.drawString("PROPHET SYNTH", 90, 35, 2);

  // BACK button - top left
  tft.fillRoundRect(5, 5, 55, 20, 4, THEME_PRIMARY);
  tft.drawRoundRect(5, 5, 55, 20, 4, THEME_OUTLINE);
  tft.setTextColor(THEME_BG, THEME_PRIMARY);
  tft.drawString("BACK", 15, 8, 2);

  // Page indicators
  const char* pageNames[] = {"OSC", "FLTR", "AMP", "F.ENV"};
  for (int i = 0; i < 4; i++) {
    int x = 10 + i * 75;
    uint16_t color = (i == synthParams.currentPage) ? THEME_PRIMARY : THEME_TEXT_DIM;
    uint16_t bgColor = (i == synthParams.currentPage) ? THEME_PRIMARY : THEME_BG;
    uint16_t txtColor = (i == synthParams.currentPage) ? THEME_BG : color;

    tft.fillRoundRect(x, 55, 70, 22, 4, bgColor);
    tft.drawRoundRect(x, 55, 70, 22, 4, THEME_OUTLINE);
    tft.setTextColor(txtColor, bgColor);
    tft.drawCentreString(pageNames[i], x + 35, 60, 2);
  }

  // Voice count - right side
  if (zombieSynth) {
    int voices = zombieSynth->getActiveVoiceCount();
    char buf[20];
    sprintf(buf, "%d/8", voices);
    tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
    tft.drawRightString(buf, 315, 60, 2);
  }
}

void drawVerticalSlider(int x, int y, int w, int h, const char* label, float value, const char* valueText = NULL) {
  // Slider background
  tft.fillRoundRect(x, y, w, h, 4, THEME_BG);
  tft.drawRoundRect(x, y, w, h, 4, THEME_OUTLINE);

  // Label at top
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString(label, x + w / 2, y + 5, 2);

  // Slider track
  int trackX = x + w / 2 - 8;
  int trackY = y + 25;
  int trackW = 16;
  int trackH = h - 50;

  tft.fillRect(trackX, trackY, trackW, trackH, THEME_BG);
  tft.drawRect(trackX, trackY, trackW, trackH, THEME_OUTLINE);

  // Fill bar from bottom
  int fillH = (int)(trackH * value);
  if (fillH > 0) {
    tft.fillRect(trackX + 1, trackY + trackH - fillH, trackW - 2, fillH, THEME_PRIMARY);
  }

  // Value text at bottom
  tft.setTextColor(THEME_ACCENT, THEME_BG);
  if (valueText) {
    tft.drawCentreString(valueText, x + w / 2, y + h - 15, 2);
  } else {
    char buf[10];
    sprintf(buf, "%d", (int)(value * 100));
    tft.drawCentreString(buf, x + w / 2, y + h - 15, 2);
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
  // OSC 1
  drawVerticalSlider(10, 85, 65, 150, "OSC1", synthParams.osc1Level);

  // OSC1 wave selection
  drawButton(80, 95, 50, 25, waveNames[synthParams.osc1Wave], true);
  drawButton(80, 125, 23, 25, "<", false);
  drawButton(107, 125, 23, 25, ">", false);

  // OSC 2
  drawVerticalSlider(140, 85, 65, 150, "OSC2", synthParams.osc2Level);

  // OSC2 wave selection
  drawButton(210, 95, 50, 25, waveNames[synthParams.osc2Wave], true);
  drawButton(210, 125, 23, 25, "<", false);
  drawButton(237, 125, 23, 25, ">", false);

  // Master Volume
  drawVerticalSlider(265, 85, 50, 150, "VOL", synthParams.masterVolume);
}

void zombieSynthDrawFilterPage() {
  // Cutoff
  drawVerticalSlider(10, 85, 65, 150, "CUTOFF", synthParams.filterCutoff);

  // Resonance
  drawVerticalSlider(80, 85, 65, 150, "RESO", synthParams.filterResonance);

  // Envelope Amount
  drawVerticalSlider(150, 85, 65, 150, "ENV", synthParams.filterEnvAmount);

  // Filter Type
  drawButton(225, 95, 85, 30, filterNames[synthParams.filterType], true);
  drawButton(225, 130, 40, 25, "<", false);
  drawButton(270, 130, 40, 25, ">", false);

  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawCentreString("TYPE", 267, 165, 2);
}

void zombieSynthDrawAmpEnvPage() {
  char buf[20];

  // Attack
  sprintf(buf, "%.2fs", synthParams.ampAttack);
  drawVerticalSlider(10, 85, 70, 150, "ATK", synthParams.ampAttack / 2.0f, buf);

  // Decay
  sprintf(buf, "%.2fs", synthParams.ampDecay);
  drawVerticalSlider(85, 85, 70, 150, "DEC", synthParams.ampDecay, buf);

  // Sustain
  drawVerticalSlider(160, 85, 70, 150, "SUS", synthParams.ampSustain);

  // Release
  sprintf(buf, "%.2fs", synthParams.ampRelease);
  drawVerticalSlider(235, 85, 70, 150, "REL", synthParams.ampRelease, buf);
}

void zombieSynthDrawFilterEnvPage() {
  char buf[20];

  // Attack
  sprintf(buf, "%.2fs", synthParams.filterAttack);
  drawVerticalSlider(10, 85, 70, 150, "ATK", synthParams.filterAttack / 2.0f, buf);

  // Decay
  sprintf(buf, "%.2fs", synthParams.filterDecay);
  drawVerticalSlider(85, 85, 70, 150, "DEC", synthParams.filterDecay, buf);

  // Sustain
  drawVerticalSlider(160, 85, 70, 150, "SUS", synthParams.filterSustain);

  // Release
  sprintf(buf, "%.2fs", synthParams.filterRelease);
  drawVerticalSlider(235, 85, 70, 150, "REL", synthParams.filterRelease, buf);
}

void zombieSynthDraw() {
  if (!synthParams.needsRedraw) return;

  drawZombieHeader();

  // Clear content area
  tft.fillRect(0, 82, 320, 158, THEME_BG);

  switch (synthParams.currentPage) {
    case 0: zombieSynthDrawOscPage(); break;
    case 1: zombieSynthDrawFilterPage(); break;
    case 2: zombieSynthDrawAmpEnvPage(); break;
    case 3: zombieSynthDrawFilterEnvPage(); break;
  }

  synthParams.needsRedraw = false;
}

// Handle slider touch with drag
bool handleSliderTouch(int sliderX, int sliderY, int sliderW, int sliderH, float& value) {
  if (touch.isPressed && touch.x >= sliderX && touch.x <= sliderX + sliderW &&
      touch.y >= sliderY && touch.y <= sliderY + sliderH) {

    // Calculate value from Y position
    int trackY = sliderY + 25;
    int trackH = sliderH - 50;
    int relY = touch.y - trackY;
    value = 1.0f - (float)relY / (float)trackH;
    value = constrain(value, 0.0f, 1.0f);

    return true;
  }
  return false;
}

void zombieSynthHandleTouch() {
  if (!touch.justPressed && !touch.isPressed) return;

  // BACK button
  if (touch.justPressed && isButtonPressed(5, 5, 55, 20)) {
    exitToMenu();
    return;
  }

  // Page tabs
  if (touch.justPressed) {
    for (int i = 0; i < 4; i++) {
      int x = 10 + i * 75;
      if (isButtonPressed(x, 55, 70, 22)) {
        synthParams.currentPage = i;
        synthParams.needsRedraw = true;
        return;
      }
    }
  }

  // Page-specific controls
  bool changed = false;

  switch (synthParams.currentPage) {
    case 0: { // OSC page
      // OSC1 level slider
      if (handleSliderTouch(10, 85, 65, 150, synthParams.osc1Level)) {
        changed = true;
      }

      // OSC2 level slider
      if (handleSliderTouch(140, 85, 65, 150, synthParams.osc2Level)) {
        changed = true;
      }

      // Master volume slider
      if (handleSliderTouch(265, 85, 50, 150, synthParams.masterVolume)) {
        zombieSynth->setMasterVolume(synthParams.masterVolume);
        changed = true;
      }

      // OSC1 wave buttons
      if (touch.justPressed) {
        if (isButtonPressed(80, 125, 23, 25)) {
          synthParams.osc1Wave = (synthParams.osc1Wave - 1 + 5) % 5;
          zombieSynth->setOsc1Waveform((WaveformType)synthParams.osc1Wave);
          changed = true;
        }
        if (isButtonPressed(107, 125, 23, 25)) {
          synthParams.osc1Wave = (synthParams.osc1Wave + 1) % 5;
          zombieSynth->setOsc1Waveform((WaveformType)synthParams.osc1Wave);
          changed = true;
        }

        // OSC2 wave buttons
        if (isButtonPressed(210, 125, 23, 25)) {
          synthParams.osc2Wave = (synthParams.osc2Wave - 1 + 5) % 5;
          zombieSynth->setOsc2Waveform((WaveformType)synthParams.osc2Wave);
          changed = true;
        }
        if (isButtonPressed(237, 125, 23, 25)) {
          synthParams.osc2Wave = (synthParams.osc2Wave + 1) % 5;
          zombieSynth->setOsc2Waveform((WaveformType)synthParams.osc2Wave);
          changed = true;
        }
      }
      break;
    }

    case 1: { // Filter page
      // Cutoff slider
      if (handleSliderTouch(10, 85, 65, 150, synthParams.filterCutoff)) {
        zombieSynth->setFilterCutoff(synthParams.filterCutoff);
        changed = true;
      }

      // Resonance slider
      if (handleSliderTouch(80, 85, 65, 150, synthParams.filterResonance)) {
        zombieSynth->setFilterResonance(synthParams.filterResonance);
        changed = true;
      }

      // Envelope amount slider
      if (handleSliderTouch(150, 85, 65, 150, synthParams.filterEnvAmount)) {
        changed = true;
      }

      // Filter type buttons
      if (touch.justPressed) {
        if (isButtonPressed(225, 130, 40, 25)) {
          synthParams.filterType = (synthParams.filterType - 1 + 4) % 4;
          zombieSynth->setFilterType((FilterType)synthParams.filterType);
          changed = true;
        }
        if (isButtonPressed(270, 130, 40, 25)) {
          synthParams.filterType = (synthParams.filterType + 1) % 4;
          zombieSynth->setFilterType((FilterType)synthParams.filterType);
          changed = true;
        }
      }
      break;
    }

    case 2: { // Amp Envelope page
      // Attack
      if (handleSliderTouch(10, 85, 70, 150, synthParams.ampAttack)) {
        synthParams.ampAttack *= 2.0f;  // Scale to 0-2 seconds
        zombieSynth->setAmpEnvelope(synthParams.ampAttack, synthParams.ampDecay,
                                     synthParams.ampSustain, synthParams.ampRelease);
        changed = true;
      }

      // Decay
      if (handleSliderTouch(85, 85, 70, 150, synthParams.ampDecay)) {
        zombieSynth->setAmpEnvelope(synthParams.ampAttack, synthParams.ampDecay,
                                     synthParams.ampSustain, synthParams.ampRelease);
        changed = true;
      }

      // Sustain
      if (handleSliderTouch(160, 85, 70, 150, synthParams.ampSustain)) {
        zombieSynth->setAmpEnvelope(synthParams.ampAttack, synthParams.ampDecay,
                                     synthParams.ampSustain, synthParams.ampRelease);
        changed = true;
      }

      // Release
      if (handleSliderTouch(235, 85, 70, 150, synthParams.ampRelease)) {
        zombieSynth->setAmpEnvelope(synthParams.ampAttack, synthParams.ampDecay,
                                     synthParams.ampSustain, synthParams.ampRelease);
        changed = true;
      }
      break;
    }

    case 3: { // Filter Envelope page
      // Attack
      if (handleSliderTouch(10, 85, 70, 150, synthParams.filterAttack)) {
        synthParams.filterAttack *= 2.0f;
        zombieSynth->setFilterEnvelope(synthParams.filterAttack, synthParams.filterDecay,
                                        synthParams.filterSustain, synthParams.filterRelease);
        changed = true;
      }

      // Decay
      if (handleSliderTouch(85, 85, 70, 150, synthParams.filterDecay)) {
        zombieSynth->setFilterEnvelope(synthParams.filterAttack, synthParams.filterDecay,
                                        synthParams.filterSustain, synthParams.filterRelease);
        changed = true;
      }

      // Sustain
      if (handleSliderTouch(160, 85, 70, 150, synthParams.filterSustain)) {
        zombieSynth->setFilterEnvelope(synthParams.filterAttack, synthParams.filterDecay,
                                        synthParams.filterSustain, synthParams.filterRelease);
        changed = true;
      }

      // Release
      if (handleSliderTouch(235, 85, 70, 150, synthParams.filterRelease)) {
        zombieSynth->setFilterEnvelope(synthParams.filterAttack, synthParams.filterDecay,
                                        synthParams.filterSustain, synthParams.filterRelease);
        changed = true;
      }
      break;
    }
  }

  if (changed) {
    synthParams.needsRedraw = true;
  }
}

void zombieSynthUpdate() {
  // Audio processing happens in separate task
  // Just update voice count display periodically
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 100) {
    lastUpdate = millis();
    synthParams.needsRedraw = true;
  }
}

SynthEngine* getZombieSynth() {
  return zombieSynth;
}

#endif
