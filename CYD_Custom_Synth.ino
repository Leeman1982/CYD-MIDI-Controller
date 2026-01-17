// ============================================================================
// CYD Custom Virtual Analogue Synthesizer
// ============================================================================
// 6-voice polyphonic synthesizer with:
// - Dual PolyBLEP oscillators + sub oscillator
// - State variable filter (LP/HP/BP/Notch)
// - ADSR envelopes for amp and filter
// - Full-featured arpeggiator with 100 patterns
// - Hardware MIDI input (5-pin DIN)
// - I2S audio output to PCM5102A DAC
// - Comprehensive touchscreen UI
// ============================================================================

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

#include "synth_config.h"
#include "synth_engine.h"
#include "audio_output.h"
#include "midi_input.h"
#include "synth_ui.h"

// ============================================================================
// Hardware Setup
// ============================================================================

TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(33, 36); // CS=33, IRQ=36

// Synth components
SynthEngine synthEngine;
AudioOutput audioOutput(&synthEngine);
MIDIInput midiInput(&synthEngine);

// Touch state
TouchState touch = {false, false, false, 0, 0};
bool lastTouchState = false;

// UI state
UIPage currentPage = PAGE_OSC;
const int NUM_PAGES = 6;

// UI interaction tracking
int selectedKnob = -1;
int lastTouchX = 0;
int lastTouchY = 0;

// ============================================================================
// Page Definitions
// ============================================================================

struct KnobControl {
  int x, y, radius;
  String label;
  float* valuePtr;
  float minVal, maxVal;
  bool isLog;
  int id;
};

struct SliderControl {
  int x, y, width, height;
  String label;
  float* valuePtr;
  float minVal, maxVal;
  int id;
};

struct ButtonControl {
  int x, y, width, height;
  String label;
  int id;
};

// ============================================================================
// Setup
// ============================================================================

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nCYD Custom Synthesizer Starting...");

  // Initialize display
  tft.init();
  tft.setRotation(1); // Landscape
  tft.fillScreen(COLOR_BG);

  // Initialize touchscreen
  SPI.begin(25, 39, 32, 33); // CLK, MISO, MOSI, CS for touch
  ts.begin();
  ts.setRotation(1);

  // Backlight on
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);

  // Splash screen
  tft.setTextColor(COLOR_PRIMARY);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("CYD SYNTH", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 20, 4);
  tft.setTextColor(COLOR_TEXT);
  tft.drawString("Initializing...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 20, 2);

  // Initialize synth engine
  synthEngine.init();
  Serial.println("Synth engine initialized");

  // Initialize MIDI input
  midiInput.init();

  // Initialize audio output
  if (!audioOutput.init()) {
    tft.setTextColor(COLOR_ERROR);
    tft.drawString("Audio Init Failed!", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 40, 2);
    while (1) delay(100);
  }

  // Start audio task on Core 1
  audioOutput.start();

  delay(1000);

  // Draw initial UI
  drawCurrentPage();

  Serial.println("Setup complete!");
}

// ============================================================================
// Main Loop (Core 0 - UI and MIDI)
// ============================================================================

void loop() {
  updateTouch();
  midiInput.process();
  handleCurrentPage();
  delay(20); // ~50 FPS
}

// ============================================================================
// Touch Handling
// ============================================================================

void updateTouch() {
  bool isTouched = ts.touched();

  if (isTouched) {
    TS_Point p = ts.getPoint();

    // Map touch coordinates to screen coordinates
    touch.x = map(p.x, 200, 3700, 0, SCREEN_WIDTH);
    touch.y = map(p.y, 240, 3800, 0, SCREEN_HEIGHT);

    touch.x = constrain(touch.x, 0, SCREEN_WIDTH - 1);
    touch.y = constrain(touch.y, 0, SCREEN_HEIGHT - 1);
  }

  touch.justPressed = isTouched && !lastTouchState;
  touch.justReleased = !isTouched && lastTouchState;
  touch.isPressed = isTouched;

  lastTouchState = isTouched;
}

// ============================================================================
// Page Drawing
// ============================================================================

void drawCurrentPage() {
  SynthUI::clearScreen();

  switch (currentPage) {
    case PAGE_OSC:
      drawOscPage();
      break;
    case PAGE_FILTER:
      drawFilterPage();
      break;
    case PAGE_ENV:
      drawEnvPage();
      break;
    case PAGE_ARP:
      drawArpPage();
      break;
    case PAGE_LFO:
      drawLFOPage();
      break;
    case PAGE_SETTINGS:
      drawSettingsPage();
      break;
  }
}

// ============================================================================
// Page 1: Oscillators
// ============================================================================

void drawOscPage() {
  SynthUI::drawHeader("OSCILLATORS", 0, NUM_PAGES);
  SynthUI::drawNavButtons();

  // Osc 1 waveform selector
  tft.setTextColor(COLOR_TEXT);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("OSC 1", 10, 40, 2);

  for (int i = 0; i < 4; i++) {
    OscillatorType type = (OscillatorType)i;
    SynthUI::drawWaveformSelector(10 + i * 45, 60, 40, 30, type, false);
  }

  // Osc 2 waveform selector
  tft.drawString("OSC 2", 10, 100, 2);

  for (int i = 0; i < 4; i++) {
    OscillatorType type = (OscillatorType)i;
    SynthUI::drawWaveformSelector(10 + i * 45, 120, 40, 30, type, false);
  }

  // Knobs
  SynthUI::drawKnob(220, 80, 30, "OSC1", synthEngine.getOsc1Level(),
                    SynthUI::formatValue(synthEngine.getOsc1Level() * 100, 0) + "%");

  SynthUI::drawKnob(280, 80, 30, "OSC2", synthEngine.getOsc2Level(),
                    SynthUI::formatValue(synthEngine.getOsc2Level() * 100, 0) + "%");

  SynthUI::drawKnob(220, 160, 30, "SUB", synthEngine.getSubLevel(),
                    SynthUI::formatValue(synthEngine.getSubLevel() * 100, 0) + "%");

  SynthUI::drawKnob(280, 160, 30, "DETUNE", synthEngine.getOsc2Detune() / 100.0f,
                    SynthUI::formatValue(synthEngine.getOsc2Detune(), 1) + "c");

  // Instructions
  tft.setTextColor(COLOR_TEXT_DIM);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Touch waveforms to select - Drag knobs to adjust", SCREEN_WIDTH / 2, 220, 1);
}

void handleOscPage() {
  if (touch.justPressed) {
    // Check nav buttons
    if (SynthUI::isInRect(touch.x, touch.y, 0, 0, 40, 30)) {
      currentPage = (UIPage)((currentPage - 1 + NUM_PAGES) % NUM_PAGES);
      drawCurrentPage();
      return;
    }
    if (SynthUI::isInRect(touch.x, touch.y, SCREEN_WIDTH - 40, 0, 40, 30)) {
      currentPage = (UIPage)((currentPage + 1) % NUM_PAGES);
      drawCurrentPage();
      return;
    }

    // Check waveform selectors for OSC1
    for (int i = 0; i < 4; i++) {
      if (SynthUI::isInRect(touch.x, touch.y, 10 + i * 45, 60, 40, 30)) {
        synthEngine.setOsc1Waveform((OscillatorType)i);
        drawOscPage();
        return;
      }
    }

    // Check waveform selectors for OSC2
    for (int i = 0; i < 4; i++) {
      if (SynthUI::isInRect(touch.x, touch.y, 10 + i * 45, 120, 40, 30)) {
        synthEngine.setOsc2Waveform((OscillatorType)i);
        drawOscPage();
        return;
      }
    }

    // Check knobs
    if (SynthUI::isInCircle(touch.x, touch.y, 220, 80, 30)) selectedKnob = 0;
    else if (SynthUI::isInCircle(touch.x, touch.y, 280, 80, 30)) selectedKnob = 1;
    else if (SynthUI::isInCircle(touch.x, touch.y, 220, 160, 30)) selectedKnob = 2;
    else if (SynthUI::isInCircle(touch.x, touch.y, 280, 160, 30)) selectedKnob = 3;

    lastTouchX = touch.x;
    lastTouchY = touch.y;
  }

  if (touch.isPressed && selectedKnob >= 0) {
    int deltaY = lastTouchY - touch.y;
    float change = deltaY * 0.01f;

    switch (selectedKnob) {
      case 0: {
        float newVal = constrain(synthEngine.getOsc1Level() + change, 0.0f, 1.0f);
        synthEngine.setOsc1Level(newVal);
        break;
      }
      case 1: {
        float newVal = constrain(synthEngine.getOsc2Level() + change, 0.0f, 1.0f);
        synthEngine.setOsc2Level(newVal);
        break;
      }
      case 2: {
        float newVal = constrain(synthEngine.getSubLevel() + change, 0.0f, 1.0f);
        synthEngine.setSubLevel(newVal);
        break;
      }
      case 3: {
        float newVal = constrain(synthEngine.getOsc2Detune() + change * 100.0f, -100.0f, 100.0f);
        synthEngine.setOsc2Detune(newVal);
        break;
      }
    }

    lastTouchX = touch.x;
    lastTouchY = touch.y;
    drawOscPage();
  }

  if (touch.justReleased) {
    selectedKnob = -1;
  }
}

// ============================================================================
// Page 2: Filter
// ============================================================================

void drawFilterPage() {
  SynthUI::drawHeader("FILTER", 1, NUM_PAGES);
  SynthUI::drawNavButtons();

  // Filter type selector
  tft.setTextColor(COLOR_TEXT);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("TYPE", 10, 40, 2);

  const char* filterNames[] = {"LP", "HP", "BP", "NOTCH"};
  for (int i = 0; i < 4; i++) {
    SynthUI::drawButton(10 + i * 60, 60, 55, 30, filterNames[i], false, COLOR_SECONDARY);
  }

  // Knobs
  float cutoffNorm = (synthEngine.getFilterCutoff() - 20.0f) / 10000.0f;
  SynthUI::drawKnob(80, 130, 35, "CUTOFF", cutoffNorm,
                    SynthUI::formatFreq(synthEngine.getFilterCutoff()));

  float resNorm = synthEngine.getFilterResonance() / 10.0f;
  SynthUI::drawKnob(180, 130, 35, "RES", resNorm,
                    SynthUI::formatValue(resNorm * 100, 0) + "%");

  SynthUI::drawKnob(80, 210, 30, "ENV", 0.5f, "50%");

  // Filter visualization (placeholder)
  tft.drawRoundRect(220, 60, 90, 160, 4, COLOR_TEXT_DIM);
  tft.setTextColor(COLOR_TEXT_DIM);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("FREQ", 265, 140, 2);
}

void handleFilterPage() {
  if (touch.justPressed) {
    // Nav buttons
    if (SynthUI::isInRect(touch.x, touch.y, 0, 0, 40, 30)) {
      currentPage = (UIPage)((currentPage - 1 + NUM_PAGES) % NUM_PAGES);
      drawCurrentPage();
      return;
    }
    if (SynthUI::isInRect(touch.x, touch.y, SCREEN_WIDTH - 40, 0, 40, 30)) {
      currentPage = (UIPage)((currentPage + 1) % NUM_PAGES);
      drawCurrentPage();
      return;
    }

    // Filter type buttons
    for (int i = 0; i < 4; i++) {
      if (SynthUI::isInRect(touch.x, touch.y, 10 + i * 60, 60, 55, 30)) {
        synthEngine.setFilterType((FilterType)i);
        drawFilterPage();
        return;
      }
    }

    // Knobs
    if (SynthUI::isInCircle(touch.x, touch.y, 80, 130, 35)) selectedKnob = 0;
    else if (SynthUI::isInCircle(touch.x, touch.y, 180, 130, 35)) selectedKnob = 1;
    else if (SynthUI::isInCircle(touch.x, touch.y, 80, 210, 30)) selectedKnob = 2;

    lastTouchX = touch.x;
    lastTouchY = touch.y;
  }

  if (touch.isPressed && selectedKnob >= 0) {
    int deltaY = lastTouchY - touch.y;
    float change = deltaY * 0.01f;

    switch (selectedKnob) {
      case 0: {
        float currentNorm = (synthEngine.getFilterCutoff() - 20.0f) / 10000.0f;
        float newNorm = constrain(currentNorm + change, 0.0f, 1.0f);
        synthEngine.setFilterCutoff(20.0f + newNorm * 10000.0f);
        break;
      }
      case 1: {
        float currentNorm = synthEngine.getFilterResonance() / 10.0f;
        float newNorm = constrain(currentNorm + change, 0.05f, 1.0f);
        synthEngine.setFilterResonance(newNorm * 10.0f);
        break;
      }
      case 2: {
        float newVal = constrain(0.5f + change, 0.0f, 1.0f);
        synthEngine.setFilterEnvAmount(newVal);
        break;
      }
    }

    lastTouchX = touch.x;
    lastTouchY = touch.y;
    drawFilterPage();
  }

  if (touch.justReleased) {
    selectedKnob = -1;
  }
}

// ============================================================================
// Page 3: Envelope
// ============================================================================

void drawEnvPage() {
  SynthUI::drawHeader("ENVELOPE", 2, NUM_PAGES);
  SynthUI::drawNavButtons();

  // Envelope visualization
  SynthUI::drawEnvelope(10, 40, 300, 80, 0.01f, 0.1f, 0.7f, 0.3f);

  // Knobs
  SynthUI::drawKnob(50, 160, 30, "ATK", 0.1f, "10ms");
  SynthUI::drawKnob(120, 160, 30, "DEC", 0.2f, "200ms");
  SynthUI::drawKnob(190, 160, 30, "SUS", 0.7f, "70%");
  SynthUI::drawKnob(260, 160, 30, "REL", 0.3f, "300ms");
}

void handleEnvPage() {
  if (touch.justPressed) {
    if (SynthUI::isInRect(touch.x, touch.y, 0, 0, 40, 30)) {
      currentPage = (UIPage)((currentPage - 1 + NUM_PAGES) % NUM_PAGES);
      drawCurrentPage();
      return;
    }
    if (SynthUI::isInRect(touch.x, touch.y, SCREEN_WIDTH - 40, 0, 40, 30)) {
      currentPage = (UIPage)((currentPage + 1) % NUM_PAGES);
      drawCurrentPage();
      return;
    }

    if (SynthUI::isInCircle(touch.x, touch.y, 50, 160, 30)) selectedKnob = 0;
    else if (SynthUI::isInCircle(touch.x, touch.y, 120, 160, 30)) selectedKnob = 1;
    else if (SynthUI::isInCircle(touch.x, touch.y, 190, 160, 30)) selectedKnob = 2;
    else if (SynthUI::isInCircle(touch.x, touch.y, 260, 160, 30)) selectedKnob = 3;

    lastTouchX = touch.x;
    lastTouchY = touch.y;
  }

  if (touch.isPressed && selectedKnob >= 0) {
    int deltaY = lastTouchY - touch.y;
    float change = deltaY * 0.01f;

    switch (selectedKnob) {
      case 0: synthEngine.setAmpAttack(constrain(0.01f + change * 2.0f, 0.001f, 2.0f)); break;
      case 1: synthEngine.setAmpDecay(constrain(0.1f + change * 2.0f, 0.001f, 2.0f)); break;
      case 2: synthEngine.setAmpSustain(constrain(0.7f + change, 0.0f, 1.0f)); break;
      case 3: synthEngine.setAmpRelease(constrain(0.3f + change * 3.0f, 0.001f, 3.0f)); break;
    }

    lastTouchX = touch.x;
    lastTouchY = touch.y;
    drawEnvPage();
  }

  if (touch.justReleased) {
    selectedKnob = -1;
  }
}

// ============================================================================
// Page 4: Arpeggiator
// ============================================================================

bool arpEnabled = false;
int arpPattern = 0;

void drawArpPage() {
  SynthUI::drawHeader("ARPEGGIATOR", 3, NUM_PAGES);
  SynthUI::drawNavButtons();

  // Enable button
  SynthUI::drawButton(10, 40, 80, 30, arpEnabled ? "ON" : "OFF", arpEnabled, COLOR_SUCCESS);

  // Pattern selector
  tft.setTextColor(COLOR_TEXT);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("PATTERN", 10, 80, 2);

  SynthUI::drawButton(10, 100, 40, 30, "<", false, COLOR_SECONDARY);
  tft.setTextColor(COLOR_PRIMARY);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(String(arpPattern), 75, 115, 4);
  SynthUI::drawButton(100, 100, 40, 30, ">", false, COLOR_SECONDARY);

  // Mode selector
  tft.setTextColor(COLOR_TEXT);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("MODE", 160, 40, 2);

  const char* modeNames[] = {"UP", "DOWN", "U/D", "RND", "PTN"};
  for (int i = 0; i < 5; i++) {
    SynthUI::drawButton(160 + (i % 3) * 50, 60 + (i / 3) * 35, 45, 30, modeNames[i], false, COLOR_SECONDARY);
  }

  // Knobs
  SynthUI::drawKnob(60, 180, 30, "TEMPO", (120.0f - 30.0f) / 270.0f, "120");
  SynthUI::drawKnob(150, 180, 30, "GATE", 0.75f, "75%");
  SynthUI::drawKnob(240, 180, 30, "SWING", 0.5f, "50%");
}

void handleArpPage() {
  if (touch.justPressed) {
    if (SynthUI::isInRect(touch.x, touch.y, 0, 0, 40, 30)) {
      currentPage = (UIPage)((currentPage - 1 + NUM_PAGES) % NUM_PAGES);
      drawCurrentPage();
      return;
    }
    if (SynthUI::isInRect(touch.x, touch.y, SCREEN_WIDTH - 40, 0, 40, 30)) {
      currentPage = (UIPage)((currentPage + 1) % NUM_PAGES);
      drawCurrentPage();
      return;
    }

    // Enable button
    if (SynthUI::isInRect(touch.x, touch.y, 10, 40, 80, 30)) {
      arpEnabled = !arpEnabled;
      synthEngine.getArpeggiator().setEnabled(arpEnabled);
      drawArpPage();
      return;
    }

    // Pattern buttons
    if (SynthUI::isInRect(touch.x, touch.y, 10, 100, 40, 30)) {
      arpPattern = (arpPattern - 1 + 100) % 100;
      synthEngine.getArpeggiator().setPattern(arpPattern);
      drawArpPage();
      return;
    }
    if (SynthUI::isInRect(touch.x, touch.y, 100, 100, 40, 30)) {
      arpPattern = (arpPattern + 1) % 100;
      synthEngine.getArpeggiator().setPattern(arpPattern);
      drawArpPage();
      return;
    }

    // Mode buttons
    for (int i = 0; i < 5; i++) {
      if (SynthUI::isInRect(touch.x, touch.y, 160 + (i % 3) * 50, 60 + (i / 3) * 35, 45, 30)) {
        synthEngine.getArpeggiator().setMode((ArpMode)i);
        drawArpPage();
        return;
      }
    }

    // Knobs
    if (SynthUI::isInCircle(touch.x, touch.y, 60, 180, 30)) selectedKnob = 0;
    else if (SynthUI::isInCircle(touch.x, touch.y, 150, 180, 30)) selectedKnob = 1;
    else if (SynthUI::isInCircle(touch.x, touch.y, 240, 180, 30)) selectedKnob = 2;

    lastTouchX = touch.x;
    lastTouchY = touch.y;
  }

  if (touch.isPressed && selectedKnob >= 0) {
    int deltaY = lastTouchY - touch.y;
    float change = deltaY * 0.01f;

    switch (selectedKnob) {
      case 0: {
        float tempo = synthEngine.getArpeggiator().getTempo() + change * 50.0f;
        synthEngine.getArpeggiator().setTempo(constrain(tempo, 30.0f, 300.0f));
        break;
      }
      case 1: {
        float gate = synthEngine.getArpeggiator().getGateLength() + change;
        synthEngine.getArpeggiator().setGateLength(constrain(gate, 0.1f, 1.0f));
        break;
      }
      case 2: {
        float swing = synthEngine.getArpeggiator().getSwing() + change;
        synthEngine.getArpeggiator().setSwing(constrain(swing, 0.0f, 1.0f));
        break;
      }
    }

    lastTouchX = touch.x;
    lastTouchY = touch.y;
    drawArpPage();
  }

  if (touch.justReleased) {
    selectedKnob = -1;
  }
}

// ============================================================================
// Page 5: LFO
// ============================================================================

void drawLFOPage() {
  SynthUI::drawHeader("LFO", 4, NUM_PAGES);
  SynthUI::drawNavButtons();

  tft.setTextColor(COLOR_TEXT);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("LFO Controls", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 4);
}

void handleLFOPage() {
  if (touch.justPressed) {
    if (SynthUI::isInRect(touch.x, touch.y, 0, 0, 40, 30)) {
      currentPage = (UIPage)((currentPage - 1 + NUM_PAGES) % NUM_PAGES);
      drawCurrentPage();
      return;
    }
    if (SynthUI::isInRect(touch.x, touch.y, SCREEN_WIDTH - 40, 0, 40, 30)) {
      currentPage = (UIPage)((currentPage + 1) % NUM_PAGES);
      drawCurrentPage();
      return;
    }
  }
}

// ============================================================================
// Page 6: Settings
// ============================================================================

void drawSettingsPage() {
  SynthUI::drawHeader("SETTINGS", 5, NUM_PAGES);
  SynthUI::drawNavButtons();

  tft.setTextColor(COLOR_TEXT);
  tft.setTextDatum(TL_DATUM);

  tft.drawString("MIDI Channel: Omni", 10, 50, 2);
  tft.drawString("Polyphony: 6 voices", 10, 75, 2);
  tft.drawString("Sample Rate: 44.1kHz", 10, 100, 2);

  tft.setTextColor(COLOR_PRIMARY);
  tft.drawString("CYD Custom Synth v1.0", 10, 140, 2);
  tft.setTextColor(COLOR_TEXT_DIM);
  tft.drawString("Hardware MIDI: GPIO16", 10, 165, 1);
  tft.drawString("I2S Audio: GPIO26/22/27", 10, 180, 1);

  SynthUI::drawButton(10, 200, 120, 30, "PANIC (All Off)", false, COLOR_ERROR);
}

void handleSettingsPage() {
  if (touch.justPressed) {
    if (SynthUI::isInRect(touch.x, touch.y, 0, 0, 40, 30)) {
      currentPage = (UIPage)((currentPage - 1 + NUM_PAGES) % NUM_PAGES);
      drawCurrentPage();
      return;
    }
    if (SynthUI::isInRect(touch.x, touch.y, SCREEN_WIDTH - 40, 0, 40, 30)) {
      currentPage = (UIPage)((currentPage + 1) % NUM_PAGES);
      drawCurrentPage();
      return;
    }

    // Panic button
    if (SynthUI::isInRect(touch.x, touch.y, 10, 200, 120, 30)) {
      synthEngine.allNotesOff();
      tft.fillRect(10, 200, 120, 30, COLOR_ERROR);
      delay(100);
      drawSettingsPage();
    }
  }
}

// ============================================================================
// Page Handler Dispatcher
// ============================================================================

void handleCurrentPage() {
  switch (currentPage) {
    case PAGE_OSC:
      handleOscPage();
      break;
    case PAGE_FILTER:
      handleFilterPage();
      break;
    case PAGE_ENV:
      handleEnvPage();
      break;
    case PAGE_ARP:
      handleArpPage();
      break;
    case PAGE_LFO:
      handleLFOPage();
      break;
    case PAGE_SETTINGS:
      handleSettingsPage();
      break;
  }
}
