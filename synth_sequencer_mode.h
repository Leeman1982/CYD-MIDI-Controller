#ifndef SYNTH_SEQUENCER_MODE_H
#define SYNTH_SEQUENCER_MODE_H

#include "common_definitions.h"
#include "ui_elements.h"
#include "midi_utils.h"

/*******************************************************************
 Synth Sequencer Mode - Full Featured Synthesizer + Step Sequencer

 Features:
 - 16-step sequencer with note and velocity editing
 - Multiple oscillator types (Saw, Square, Sine, Triangle)
 - Filter with cutoff and resonance
 - ADSR envelope
 - LFO modulation
 - Scale-locked note selection
 - Tempo control (60-200 BPM)
 - Pattern playback with visual feedback
 *******************************************************************/

// Sequencer configuration
#define NUM_STEPS 16
#define NUM_PATTERNS 4

// Oscillator types
enum OscType {
  OSC_SAW,
  OSC_SQUARE,
  OSC_SINE,
  OSC_TRIANGLE
};

// Synth parameters
struct SynthParams {
  OscType oscType;
  int filterCutoff;     // 0-127
  int filterResonance;  // 0-127
  int attack;           // 0-127
  int decay;            // 0-127
  int sustain;          // 0-127
  int release;          // 0-127
  int lfoRate;          // 0-127
  int lfoDepth;         // 0-127
};

// Sequencer step
struct Step {
  bool active;
  int note;      // MIDI note 0-127
  int velocity;  // 0-127
};

// Sequencer state
struct Sequencer {
  Step steps[NUM_STEPS];
  int currentStep;
  bool playing;
  int bpm;
  unsigned long lastStepTime;
  int currentPattern;
};

// UI state
enum SynthUIPage {
  PAGE_SEQUENCER,
  PAGE_SYNTH,
  PAGE_PATTERN
};

// Global state
static Sequencer sequencer;
static SynthParams synthParams;
static SynthUIPage currentPage = PAGE_SEQUENCER;
static int selectedStep = 0;
static int currentScale = 0;
static int baseOctave = 4;
static int lastPlayingNote = -1;

// UI element positions
#define HEADER_HEIGHT 40
#define STEP_GRID_Y 45
#define STEP_WIDTH 18
#define STEP_HEIGHT 60
#define STEP_SPACING 2
#define CONTROL_Y 120
#define BUTTON_Y 200

// Forward declarations
void drawSynthSequencerHeader();
void drawSequencerPage();
void drawSynthPage();
void drawPatternPage();
void updateStepDisplay();
void handleSequencerPageTouch();
void handleSynthPageTouch();
void handlePatternPageTouch();
void playStep(int stepIndex);
void stopCurrentNote();
void sendSynthMIDI(byte cmd, byte note, byte vel);

void initializeSynthSequencerMode() {
  // Initialize sequencer
  sequencer.playing = false;
  sequencer.bpm = 120;
  sequencer.currentStep = 0;
  sequencer.lastStepTime = 0;
  sequencer.currentPattern = 0;

  // Initialize synth parameters
  synthParams.oscType = OSC_SAW;
  synthParams.filterCutoff = 127;
  synthParams.filterResonance = 0;
  synthParams.attack = 10;
  synthParams.decay = 40;
  synthParams.sustain = 100;
  synthParams.release = 50;
  synthParams.lfoRate = 60;
  synthParams.lfoDepth = 0;

  // Initialize steps with a default pattern
  for (int i = 0; i < NUM_STEPS; i++) {
    sequencer.steps[i].active = (i % 4 == 0); // Kick pattern
    sequencer.steps[i].note = 36 + (i % 12);  // C2 + scale
    sequencer.steps[i].velocity = 100;
  }
}

void drawSynthSequencerMode() {
  tft.fillScreen(THEME_BG);
  drawSynthSequencerHeader();

  switch (currentPage) {
    case PAGE_SEQUENCER:
      drawSequencerPage();
      break;
    case PAGE_SYNTH:
      drawSynthPage();
      break;
    case PAGE_PATTERN:
      drawPatternPage();
      break;
  }
}

void drawSynthSequencerHeader() {
  // Header background
  tft.fillRect(0, 0, 320, HEADER_HEIGHT, THEME_SURFACE);

  // Title
  tft.setTextColor(THEME_PRIMARY, THEME_SURFACE);
  tft.drawString("SYNTH SEQUENCER", 5, 5, 2);

  // BPM display
  tft.setTextColor(THEME_TEXT, THEME_SURFACE);
  char bpmStr[16];
  sprintf(bpmStr, "%d BPM", sequencer.bpm);
  tft.drawString(bpmStr, 5, 22, 2);

  // Pattern indicator
  char patStr[16];
  sprintf(patStr, "PAT:%d", sequencer.currentPattern + 1);
  tft.drawString(patStr, 100, 22, 2);

  // Page tabs
  int tabW = 60;
  int tabH = 18;
  int tabY = 5;
  int tabX = 155;

  // SEQ tab
  uint16_t seqColor = (currentPage == PAGE_SEQUENCER) ? THEME_PRIMARY : THEME_SURFACE;
  uint16_t seqTextColor = (currentPage == PAGE_SEQUENCER) ? THEME_BG : THEME_TEXT_DIM;
  tft.fillRoundRect(tabX, tabY, tabW, tabH, 3, seqColor);
  tft.setTextColor(seqTextColor, seqColor);
  tft.drawCentreString("SEQ", tabX + tabW/2, tabY + 4, 2);

  // SYNTH tab
  tabX += tabW + 2;
  uint16_t synthColor = (currentPage == PAGE_SYNTH) ? THEME_PRIMARY : THEME_SURFACE;
  uint16_t synthTextColor = (currentPage == PAGE_SYNTH) ? THEME_BG : THEME_TEXT_DIM;
  tft.fillRoundRect(tabX, tabY, tabW, tabH, 3, synthColor);
  tft.setTextColor(synthTextColor, synthColor);
  tft.drawCentreString("SYNTH", tabX + tabW/2, tabY + 4, 2);

  // BACK button
  drawButton(285, 5, 30, 28, "X", THEME_ERROR, THEME_BG);

  // Play/Stop button
  uint16_t playColor = sequencer.playing ? THEME_SUCCESS : THEME_PRIMARY;
  const char* playText = sequencer.playing ? "||" : ">";
  drawButton(150, 22, 30, 15, playText, playColor, THEME_SURFACE);
}

void drawSequencerPage() {
  // Step grid
  int startX = (320 - (NUM_STEPS * (STEP_WIDTH + STEP_SPACING))) / 2;

  for (int i = 0; i < NUM_STEPS; i++) {
    int x = startX + i * (STEP_WIDTH + STEP_SPACING);
    int y = STEP_GRID_Y;

    // Determine step color
    uint16_t stepColor;
    if (sequencer.playing && i == sequencer.currentStep) {
      stepColor = THEME_WARNING; // Highlight current step
    } else if (sequencer.steps[i].active) {
      stepColor = THEME_PRIMARY;
    } else {
      stepColor = THEME_SURFACE;
    }

    // Draw step
    tft.fillRect(x, y, STEP_WIDTH, STEP_HEIGHT, stepColor);

    // Draw step number
    tft.setTextColor(sequencer.steps[i].active ? THEME_BG : THEME_TEXT_DIM, stepColor);
    char stepNum[3];
    sprintf(stepNum, "%d", (i + 1) % 10);
    tft.drawCentreString(stepNum, x + STEP_WIDTH/2, y + 5, 1);

    // Draw velocity bar if active
    if (sequencer.steps[i].active) {
      int barHeight = (sequencer.steps[i].velocity * (STEP_HEIGHT - 20)) / 127;
      tft.fillRect(x + 2, y + STEP_HEIGHT - 5 - barHeight, STEP_WIDTH - 4, barHeight, THEME_BG);
    }

    // Selection indicator
    if (i == selectedStep) {
      tft.drawRect(x, y, STEP_WIDTH, STEP_HEIGHT, THEME_SECONDARY);
      tft.drawRect(x + 1, y + 1, STEP_WIDTH - 2, STEP_HEIGHT - 2, THEME_SECONDARY);
    }
  }

  // Control section
  tft.setTextColor(THEME_TEXT, THEME_BG);
  tft.drawString("STEP CONTROLS", 10, CONTROL_Y, 2);

  // Selected step info
  if (sequencer.steps[selectedStep].active) {
    char noteStr[32];
    String noteName = getNoteNameFromMIDI(sequencer.steps[selectedStep].note);
    sprintf(noteStr, "Note: %s (%d)", noteName.c_str(), sequencer.steps[selectedStep].note);
    tft.drawString(noteStr, 10, CONTROL_Y + 20, 2);

    char velStr[16];
    sprintf(velStr, "Vel: %d", sequencer.steps[selectedStep].velocity);
    tft.drawString(velStr, 10, CONTROL_Y + 38, 2);
  } else {
    tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
    tft.drawString("Step inactive", 10, CONTROL_Y + 20, 2);
  }

  // Buttons
  drawButton(10, BUTTON_Y, 50, 30, "PREV", THEME_PRIMARY, THEME_BG);
  drawButton(65, BUTTON_Y, 50, 30, "NEXT", THEME_PRIMARY, THEME_BG);
  drawButton(120, BUTTON_Y, 50, 30, "ON/OFF", THEME_ACCENT, THEME_BG);
  drawButton(175, BUTTON_Y, 50, 30, "NOTE+", THEME_SECONDARY, THEME_BG);
  drawButton(230, BUTTON_Y, 50, 30, "NOTE-", THEME_SECONDARY, THEME_BG);

  // BPM controls
  tft.drawString("TEMPO", 200, CONTROL_Y, 2);
  drawButton(200, CONTROL_Y + 20, 35, 30, "-", THEME_PRIMARY, THEME_BG);
  drawButton(240, CONTROL_Y + 20, 35, 30, "+", THEME_PRIMARY, THEME_BG);

  // Clear/Fill
  drawButton(280, CONTROL_Y + 20, 35, 30, "CLR", THEME_ERROR, THEME_BG);
}

void drawSynthPage() {
  int y = STEP_GRID_Y + 5;
  int labelX = 10;
  int valueX = 120;
  int lineHeight = 20;

  tft.setTextColor(THEME_TEXT, THEME_BG);

  // Oscillator section
  tft.drawString("OSCILLATOR", labelX, y, 2);
  y += lineHeight;

  const char* oscNames[] = {"SAW", "SQUARE", "SINE", "TRIANGLE"};
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawString(oscNames[synthParams.oscType], valueX, y, 2);
  drawButton(200, y - 2, 40, 18, "<", THEME_PRIMARY, THEME_BG);
  drawButton(245, y - 2, 40, 18, ">", THEME_PRIMARY, THEME_BG);
  y += lineHeight + 5;

  // Filter section
  tft.setTextColor(THEME_TEXT, THEME_BG);
  tft.drawString("FILTER", labelX, y, 2);
  y += lineHeight;

  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  char cutoffStr[16];
  sprintf(cutoffStr, "Cutoff: %d", synthParams.filterCutoff);
  tft.drawString(cutoffStr, labelX, y, 2);
  drawButton(200, y - 2, 40, 18, "-", THEME_PRIMARY, THEME_BG);
  drawButton(245, y - 2, 40, 18, "+", THEME_PRIMARY, THEME_BG);
  y += lineHeight;

  char resStr[16];
  sprintf(resStr, "Res: %d", synthParams.filterResonance);
  tft.drawString(resStr, labelX, y, 2);
  drawButton(200, y - 2, 40, 18, "-", THEME_PRIMARY, THEME_BG);
  drawButton(245, y - 2, 40, 18, "+", THEME_PRIMARY, THEME_BG);
  y += lineHeight + 5;

  // Envelope section
  tft.setTextColor(THEME_TEXT, THEME_BG);
  tft.drawString("ENVELOPE (ADSR)", labelX, y, 2);
  y += lineHeight;

  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  char adsrStr[32];
  sprintf(adsrStr, "A:%d D:%d S:%d R:%d", synthParams.attack, synthParams.decay,
          synthParams.sustain, synthParams.release);
  tft.drawString(adsrStr, labelX, y, 1);
  y += lineHeight;

  // ADSR controls
  drawButton(10, y, 35, 18, "A-", THEME_ACCENT, THEME_BG);
  drawButton(48, y, 35, 18, "A+", THEME_ACCENT, THEME_BG);
  drawButton(90, y, 35, 18, "D-", THEME_ACCENT, THEME_BG);
  drawButton(128, y, 35, 18, "D+", THEME_ACCENT, THEME_BG);
  drawButton(170, y, 35, 18, "S-", THEME_ACCENT, THEME_BG);
  drawButton(208, y, 35, 18, "S+", THEME_ACCENT, THEME_BG);
  drawButton(250, y, 35, 18, "R-", THEME_ACCENT, THEME_BG);
  y += 20;
  drawButton(250, y, 35, 18, "R+", THEME_ACCENT, THEME_BG);
  y += lineHeight + 5;

  // LFO section
  tft.setTextColor(THEME_TEXT, THEME_BG);
  tft.drawString("LFO MODULATION", labelX, y, 2);
  y += lineHeight;

  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  char lfoStr[32];
  sprintf(lfoStr, "Rate: %d  Depth: %d", synthParams.lfoRate, synthParams.lfoDepth);
  tft.drawString(lfoStr, labelX, y, 2);
  y += lineHeight;

  drawButton(10, y, 55, 18, "RATE-", THEME_SECONDARY, THEME_BG);
  drawButton(68, y, 55, 18, "RATE+", THEME_SECONDARY, THEME_BG);
  drawButton(130, y, 60, 18, "DEPTH-", THEME_SECONDARY, THEME_BG);
  drawButton(193, y, 60, 18, "DEPTH+", THEME_SECONDARY, THEME_BG);
}

void drawPatternPage() {
  int y = STEP_GRID_Y + 5;
  tft.setTextColor(THEME_TEXT, THEME_BG);
  tft.drawString("PATTERN CONTROLS", 10, y, 2);
  y += 25;

  // Scale selection
  tft.drawString("Scale:", 10, y, 2);
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawString(scales[currentScale].name, 80, y, 2);
  drawButton(200, y - 2, 40, 18, "<", THEME_PRIMARY, THEME_BG);
  drawButton(245, y - 2, 40, 18, ">", THEME_PRIMARY, THEME_BG);
  y += 25;

  // Octave selection
  tft.setTextColor(THEME_TEXT, THEME_BG);
  char octStr[16];
  sprintf(octStr, "Octave: %d", baseOctave);
  tft.drawString(octStr, 10, y, 2);
  drawButton(200, y - 2, 40, 18, "-", THEME_PRIMARY, THEME_BG);
  drawButton(245, y - 2, 40, 18, "+", THEME_PRIMARY, THEME_BG);
  y += 30;

  // Pattern operations
  tft.setTextColor(THEME_TEXT, THEME_BG);
  tft.drawString("PATTERN OPERATIONS", 10, y, 2);
  y += 25;

  drawButton(10, y, 70, 25, "CLEAR", THEME_ERROR, THEME_BG);
  drawButton(85, y, 70, 25, "RANDOM", THEME_WARNING, THEME_BG);
  drawButton(160, y, 70, 25, "SHIFT>", THEME_ACCENT, THEME_BG);
  drawButton(235, y, 70, 25, "<SHIFT", THEME_ACCENT, THEME_BG);
  y += 30;

  drawButton(10, y, 70, 25, "INVERT", THEME_ACCENT, THEME_BG);
  drawButton(85, y, 70, 25, "DOUBLE", THEME_ACCENT, THEME_BG);
  drawButton(160, y, 70, 25, "HALF", THEME_ACCENT, THEME_BG);
  y += 30;

  // Copy/Paste
  tft.setTextColor(THEME_TEXT, THEME_BG);
  tft.drawString("PATTERN MEMORY", 10, y, 2);
  y += 25;

  drawButton(10, y, 70, 25, "COPY", THEME_PRIMARY, THEME_BG);
  drawButton(85, y, 70, 25, "PASTE", THEME_PRIMARY, THEME_BG);
}

void handleSynthSequencerMode() {
  // Update sequencer
  if (sequencer.playing) {
    unsigned long stepInterval = (60000 / sequencer.bpm) / 4; // 16th notes
    unsigned long currentTime = millis();

    if (currentTime - sequencer.lastStepTime >= stepInterval) {
      sequencer.lastStepTime = currentTime;

      // Stop previous note
      stopCurrentNote();

      // Play current step
      if (sequencer.steps[sequencer.currentStep].active) {
        playStep(sequencer.currentStep);
      }

      // Advance to next step
      sequencer.currentStep = (sequencer.currentStep + 1) % NUM_STEPS;

      // Update display
      updateStepDisplay();
    }
  }

  // Handle touch input
  if (touch.justPressed) {
    // Check header buttons
    if (isButtonPressed(285, 5, 30, 28)) {
      exitToMenu();
      return;
    }

    // Play/Stop button
    if (isButtonPressed(150, 22, 30, 15)) {
      sequencer.playing = !sequencer.playing;
      if (!sequencer.playing) {
        stopCurrentNote();
        sequencer.currentStep = 0;
      } else {
        sequencer.lastStepTime = millis();
      }
      drawSynthSequencerMode();
      return;
    }

    // Page tabs
    if (isButtonPressed(155, 5, 60, 18)) {
      if (currentPage != PAGE_SEQUENCER) {
        currentPage = PAGE_SEQUENCER;
        drawSynthSequencerMode();
      }
      return;
    }
    if (isButtonPressed(217, 5, 60, 18)) {
      if (currentPage != PAGE_SYNTH) {
        currentPage = PAGE_SYNTH;
        drawSynthSequencerMode();
      }
      return;
    }

    // Page-specific touch handling
    switch (currentPage) {
      case PAGE_SEQUENCER:
        handleSequencerPageTouch();
        break;
      case PAGE_SYNTH:
        handleSynthPageTouch();
        break;
      case PAGE_PATTERN:
        handlePatternPageTouch();
        break;
    }
  }
}

void handleSequencerPageTouch() {
  // Check step grid
  int startX = (320 - (NUM_STEPS * (STEP_WIDTH + STEP_SPACING))) / 2;
  for (int i = 0; i < NUM_STEPS; i++) {
    int x = startX + i * (STEP_WIDTH + STEP_SPACING);
    if (isButtonPressed(x, STEP_GRID_Y, STEP_WIDTH, STEP_HEIGHT)) {
      selectedStep = i;
      drawSequencerPage();
      return;
    }
  }

  // Step navigation
  if (isButtonPressed(10, BUTTON_Y, 50, 30)) {
    selectedStep = (selectedStep - 1 + NUM_STEPS) % NUM_STEPS;
    drawSequencerPage();
    return;
  }
  if (isButtonPressed(65, BUTTON_Y, 50, 30)) {
    selectedStep = (selectedStep + 1) % NUM_STEPS;
    drawSequencerPage();
    return;
  }

  // Toggle step
  if (isButtonPressed(120, BUTTON_Y, 50, 30)) {
    sequencer.steps[selectedStep].active = !sequencer.steps[selectedStep].active;
    drawSequencerPage();
    return;
  }

  // Note adjustment
  if (isButtonPressed(175, BUTTON_Y, 50, 30)) {
    sequencer.steps[selectedStep].note = min(127, sequencer.steps[selectedStep].note + 1);
    drawSequencerPage();
    return;
  }
  if (isButtonPressed(230, BUTTON_Y, 50, 30)) {
    sequencer.steps[selectedStep].note = max(0, sequencer.steps[selectedStep].note - 1);
    drawSequencerPage();
    return;
  }

  // BPM controls
  if (isButtonPressed(200, CONTROL_Y + 20, 35, 30)) {
    sequencer.bpm = max(60, sequencer.bpm - 5);
    drawSynthSequencerMode();
    return;
  }
  if (isButtonPressed(240, CONTROL_Y + 20, 35, 30)) {
    sequencer.bpm = min(200, sequencer.bpm + 5);
    drawSynthSequencerMode();
    return;
  }

  // Clear pattern
  if (isButtonPressed(280, CONTROL_Y + 20, 35, 30)) {
    for (int i = 0; i < NUM_STEPS; i++) {
      sequencer.steps[i].active = false;
    }
    drawSequencerPage();
    return;
  }
}

void handleSynthPageTouch() {
  int y = STEP_GRID_Y + 5 + 20;

  // Oscillator type
  if (isButtonPressed(200, y - 2, 40, 18)) {
    synthParams.oscType = (OscType)((synthParams.oscType - 1 + 4) % 4);
    drawSynthPage();
    return;
  }
  if (isButtonPressed(245, y - 2, 40, 18)) {
    synthParams.oscType = (OscType)((synthParams.oscType + 1) % 4);
    drawSynthPage();
    return;
  }

  y += 40;
  // Filter cutoff
  if (isButtonPressed(200, y - 2, 40, 18)) {
    synthParams.filterCutoff = max(0, synthParams.filterCutoff - 10);
    sendMIDI(0xB0, 74, synthParams.filterCutoff); // CC 74 = Filter cutoff
    drawSynthPage();
    return;
  }
  if (isButtonPressed(245, y - 2, 40, 18)) {
    synthParams.filterCutoff = min(127, synthParams.filterCutoff + 10);
    sendMIDI(0xB0, 74, synthParams.filterCutoff);
    drawSynthPage();
    return;
  }

  y += 20;
  // Filter resonance
  if (isButtonPressed(200, y - 2, 40, 18)) {
    synthParams.filterResonance = max(0, synthParams.filterResonance - 10);
    sendMIDI(0xB0, 71, synthParams.filterResonance); // CC 71 = Resonance
    drawSynthPage();
    return;
  }
  if (isButtonPressed(245, y - 2, 40, 18)) {
    synthParams.filterResonance = min(127, synthParams.filterResonance + 10);
    sendMIDI(0xB0, 71, synthParams.filterResonance);
    drawSynthPage();
    return;
  }

  y += 45;
  // ADSR controls
  if (isButtonPressed(10, y, 35, 18)) {
    synthParams.attack = max(0, synthParams.attack - 5);
    drawSynthPage();
    return;
  }
  if (isButtonPressed(48, y, 35, 18)) {
    synthParams.attack = min(127, synthParams.attack + 5);
    drawSynthPage();
    return;
  }
  if (isButtonPressed(90, y, 35, 18)) {
    synthParams.decay = max(0, synthParams.decay - 5);
    drawSynthPage();
    return;
  }
  if (isButtonPressed(128, y, 35, 18)) {
    synthParams.decay = min(127, synthParams.decay + 5);
    drawSynthPage();
    return;
  }
  if (isButtonPressed(170, y, 35, 18)) {
    synthParams.sustain = max(0, synthParams.sustain - 5);
    drawSynthPage();
    return;
  }
  if (isButtonPressed(208, y, 35, 18)) {
    synthParams.sustain = min(127, synthParams.sustain + 5);
    drawSynthPage();
    return;
  }
  if (isButtonPressed(250, y, 35, 18)) {
    synthParams.release = max(0, synthParams.release - 5);
    drawSynthPage();
    return;
  }
  y += 20;
  if (isButtonPressed(250, y, 35, 18)) {
    synthParams.release = min(127, synthParams.release + 5);
    drawSynthPage();
    return;
  }

  y += 45;
  // LFO controls
  if (isButtonPressed(10, y, 55, 18)) {
    synthParams.lfoRate = max(0, synthParams.lfoRate - 10);
    drawSynthPage();
    return;
  }
  if (isButtonPressed(68, y, 55, 18)) {
    synthParams.lfoRate = min(127, synthParams.lfoRate + 10);
    drawSynthPage();
    return;
  }
  if (isButtonPressed(130, y, 60, 18)) {
    synthParams.lfoDepth = max(0, synthParams.lfoDepth - 10);
    sendMIDI(0xB0, 1, synthParams.lfoDepth); // CC 1 = Mod wheel
    drawSynthPage();
    return;
  }
  if (isButtonPressed(193, y, 60, 18)) {
    synthParams.lfoDepth = min(127, synthParams.lfoDepth + 10);
    sendMIDI(0xB0, 1, synthParams.lfoDepth);
    drawSynthPage();
    return;
  }
}

void handlePatternPageTouch() {
  int y = STEP_GRID_Y + 30;

  // Scale selection
  if (isButtonPressed(200, y - 2, 40, 18)) {
    currentScale = (currentScale - 1 + NUM_SCALES) % NUM_SCALES;
    drawPatternPage();
    return;
  }
  if (isButtonPressed(245, y - 2, 40, 18)) {
    currentScale = (currentScale + 1) % NUM_SCALES;
    drawPatternPage();
    return;
  }

  y += 25;
  // Octave
  if (isButtonPressed(200, y - 2, 40, 18)) {
    baseOctave = max(0, baseOctave - 1);
    drawPatternPage();
    return;
  }
  if (isButtonPressed(245, y - 2, 40, 18)) {
    baseOctave = min(8, baseOctave + 1);
    drawPatternPage();
    return;
  }

  y += 55;
  // Pattern operations
  if (isButtonPressed(10, y, 70, 25)) { // CLEAR
    for (int i = 0; i < NUM_STEPS; i++) {
      sequencer.steps[i].active = false;
    }
    drawSynthSequencerMode();
    return;
  }
  if (isButtonPressed(85, y, 70, 25)) { // RANDOM
    for (int i = 0; i < NUM_STEPS; i++) {
      sequencer.steps[i].active = random(2);
      sequencer.steps[i].note = getNoteInScale(currentScale, random(8), baseOctave);
      sequencer.steps[i].velocity = 80 + random(47);
    }
    drawSynthSequencerMode();
    return;
  }
  if (isButtonPressed(160, y, 70, 25)) { // SHIFT>
    Step temp = sequencer.steps[NUM_STEPS - 1];
    for (int i = NUM_STEPS - 1; i > 0; i--) {
      sequencer.steps[i] = sequencer.steps[i - 1];
    }
    sequencer.steps[0] = temp;
    drawSynthSequencerMode();
    return;
  }
  if (isButtonPressed(235, y, 70, 25)) { // <SHIFT
    Step temp = sequencer.steps[0];
    for (int i = 0; i < NUM_STEPS - 1; i++) {
      sequencer.steps[i] = sequencer.steps[i + 1];
    }
    sequencer.steps[NUM_STEPS - 1] = temp;
    drawSynthSequencerMode();
    return;
  }

  y += 30;
  if (isButtonPressed(10, y, 70, 25)) { // INVERT
    for (int i = 0; i < NUM_STEPS; i++) {
      sequencer.steps[i].active = !sequencer.steps[i].active;
    }
    drawSynthSequencerMode();
    return;
  }
}

void updateStepDisplay() {
  int startX = (320 - (NUM_STEPS * (STEP_WIDTH + STEP_SPACING))) / 2;

  // Redraw all steps to update current step highlight
  for (int i = 0; i < NUM_STEPS; i++) {
    int x = startX + i * (STEP_WIDTH + STEP_SPACING);
    int y = STEP_GRID_Y;

    uint16_t stepColor;
    if (sequencer.playing && i == sequencer.currentStep) {
      stepColor = THEME_WARNING;
    } else if (sequencer.steps[i].active) {
      stepColor = THEME_PRIMARY;
    } else {
      stepColor = THEME_SURFACE;
    }

    tft.fillRect(x, y, STEP_WIDTH, STEP_HEIGHT, stepColor);

    tft.setTextColor(sequencer.steps[i].active ? THEME_BG : THEME_TEXT_DIM, stepColor);
    char stepNum[3];
    sprintf(stepNum, "%d", (i + 1) % 10);
    tft.drawCentreString(stepNum, x + STEP_WIDTH/2, y + 5, 1);

    if (sequencer.steps[i].active) {
      int barHeight = (sequencer.steps[i].velocity * (STEP_HEIGHT - 20)) / 127;
      tft.fillRect(x + 2, y + STEP_HEIGHT - 5 - barHeight, STEP_WIDTH - 4, barHeight, THEME_BG);
    }

    if (i == selectedStep) {
      tft.drawRect(x, y, STEP_WIDTH, STEP_HEIGHT, THEME_SECONDARY);
      tft.drawRect(x + 1, y + 1, STEP_WIDTH - 2, STEP_HEIGHT - 2, THEME_SECONDARY);
    }
  }
}

void playStep(int stepIndex) {
  if (stepIndex < 0 || stepIndex >= NUM_STEPS) return;

  Step& step = sequencer.steps[stepIndex];
  if (step.active) {
    sendSynthMIDI(0x90, step.note, step.velocity);
    lastPlayingNote = step.note;
  }
}

void stopCurrentNote() {
  if (lastPlayingNote >= 0) {
    sendSynthMIDI(0x80, lastPlayingNote, 0);
    lastPlayingNote = -1;
  }
}

void sendSynthMIDI(byte cmd, byte note, byte vel) {
  // Send MIDI with synth parameters as CCs
  if (deviceConnected && pCharacteristic) {
    // Send note
    sendMIDI(cmd, note, vel);

    // Send synth parameters as MIDI CCs (only on note on)
    if (cmd == 0x90 && vel > 0) {
      // Could send ADSR and other params as CCs here
      sendMIDI(0xB0, 73, synthParams.attack);      // CC 73 = Attack
      sendMIDI(0xB0, 75, synthParams.decay);       // CC 75 = Decay
      sendMIDI(0xB0, 70, synthParams.sustain);     // CC 70 = Sustain
      sendMIDI(0xB0, 72, synthParams.release);     // CC 72 = Release
      sendMIDI(0xB0, 74, synthParams.filterCutoff);// CC 74 = Filter cutoff
      sendMIDI(0xB0, 71, synthParams.filterResonance); // CC 71 = Resonance
    }
  }
}

#endif
