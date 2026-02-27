/*******************************************************************
 ZOMBIE SS PROPHET-8 SYNTHESIZER
 Prophet-8 inspired polyBLEP synth for ESP32 CYD

 Features:
 - 8-voice polyphony with polyBLEP oscillators
 - State variable filter with envelope
 - Full ADSR envelopes
 - 50-pattern arpeggiator
 - 16-step sequencer with 4 tracks
 - USB and 5-pin DIN MIDI input
 - PCM5052 DAC output (I2S)
 - ZOMBIE SS themed UI (black/red/white)
 *******************************************************************/

#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <TFT_eSPI.h>

// Include all mode files
#include "synth_engine.h"
#include "midi_input.h"
#include "arpeggiator_patterns.h"
#include "zombie_step_sequencer.h"
#include "zombie_synth_mode.h"
#include "zombie_arp_mode.h"
#include "zombie_seq_mode.h"
#include "ui_elements.h"

// Hardware setup
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// Global objects
// Touch uses VSPI with custom pins (25, 32, 33, 39)
SPIClass touchSPI = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft = TFT_eSPI();

// MIDI input
MIDIInput midiInput;

// Touch state
TouchState touch;

// App state
AppMode currentMode = MENU;

// Audio task handle
TaskHandle_t audioTaskHandle = NULL;

// MIDI callbacks
void onMIDINoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
  SynthEngine* synth = getZombieSynth();
  if (synth) {
    synth->noteOn(note, velocity);
  }

  Arpeggiator* arp = getZombieArp();
  if (arp) {
    arp->noteOn(note);
  }
}

void onMIDINoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
  SynthEngine* synth = getZombieSynth();
  if (synth) {
    synth->noteOff(note);
  }

  Arpeggiator* arp = getZombieArp();
  if (arp) {
    arp->noteOff(note);
  }
}

void onMIDICC(uint8_t channel, uint8_t cc, uint8_t value) {
  SynthEngine* synth = getZombieSynth();
  if (!synth) return;

  float normalizedValue = value / 127.0f;

  switch (cc) {
    case 74: // Filter cutoff
      synth->setFilterCutoff(normalizedValue);
      break;
    case 71: // Filter resonance
      synth->setFilterResonance(normalizedValue);
      break;
    case 7: // Master volume
      synth->setMasterVolume(normalizedValue);
      break;
  }
}

void onMIDIPitchBend(uint8_t channel, int16_t bend) {
  // Pitch bend handling can be added here
}

// Audio processing task (runs on Core 0)
void audioTask(void* parameter) {
  while (true) {
    SynthEngine* synth = getZombieSynth();
    if (synth) {
      synth->processAudio();  // i2s_write(portMAX_DELAY) provides natural backpressure
    }
    // No vTaskDelay here — adding any delay risks DMA underruns.
    // i2s_write with portMAX_DELAY blocks until DMA has room, self-pacing the loop.
  }
}

// Menu system
struct AppIcon {
  String name;
  String symbol;
  uint16_t color;
  AppMode mode;
};

AppIcon apps[] = {
  {"SYNTH", "♪♪", THEME_PRIMARY, ZOMBIE_SYNTH},
  {"ARP", "↗↗", THEME_PRIMARY, ZOMBIE_ARP},
  {"SEQ", "▣▣", THEME_PRIMARY, ZOMBIE_SEQ}
};

int numApps = 3;

void drawMenu() {
  tft.fillScreen(THEME_BG);

  // ZOMBIE SS Header
  tft.fillRect(0, 0, 320, 60, THEME_BG);
  tft.drawRect(0, 0, 320, 60, THEME_OUTLINE);
  tft.drawRect(1, 1, 318, 58, THEME_OUTLINE);
  tft.drawRect(2, 2, 316, 56, THEME_OUTLINE);

  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.setTextSize(2);
  tft.drawCentreString("ZOMBIE SS", 160, 10, 4);

  tft.setTextColor(THEME_ACCENT, THEME_BG);
  tft.setTextSize(1);
  tft.drawCentreString("PROPHET SYNTHESIZER", 160, 38, 2);

  // Version
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawString("v1.0", 10, 70, 2);

  // Voice count
  SynthEngine* synth = getZombieSynth();
  if (synth) {
    char buf[20];
    sprintf(buf, "VOICES:%d/8", synth->getActiveVoiceCount());
    tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
    tft.drawRightString(buf, 310, 70, 2);
  }

  // App icons grid
  int iconsPerRow = 3;
  int iconW = 90;
  int iconH = 60;
  int spacing = 10;
  int startX = (320 - (iconsPerRow * iconW + (iconsPerRow - 1) * spacing)) / 2;
  int startY = 100;

  for (int i = 0; i < numApps; i++) {
    int row = i / iconsPerRow;
    int col = i % iconsPerRow;
    int x = startX + col * (iconW + spacing);
    int y = startY + row * (iconH + spacing);

    // Draw icon background
    tft.fillRoundRect(x, y, iconW, iconH, 8, THEME_BG);
    tft.drawRoundRect(x, y, iconW, iconH, 8, THEME_OUTLINE);
    tft.drawRoundRect(x + 1, y + 1, iconW - 2, iconH - 2, 7, THEME_OUTLINE);

    // Draw symbol
    tft.setTextColor(apps[i].color, THEME_BG);
    tft.setTextSize(2);
    tft.drawCentreString(apps[i].symbol, x + iconW / 2, y + 10, 4);

    // Draw name
    tft.setTextColor(THEME_PRIMARY, THEME_BG);
    tft.setTextSize(1);
    tft.drawCentreString(apps[i].name, x + iconW / 2, y + 42, 2);
  }

  // Instructions
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawCentreString("TAP TO SELECT MODE", 160, 220, 2);
}

void enterMode(AppMode mode) {
  currentMode = mode;
  tft.fillScreen(THEME_BG);

  switch (mode) {
    case ZOMBIE_SYNTH:
      zombieSynthInit();
      break;
    case ZOMBIE_ARP:
      zombieArpInit();
      break;
    case ZOMBIE_SEQ:
      zombieSeqInit();
      break;
    case MENU:
      drawMenu();
      break;
  }
}

void exitToMenu() {
  // Stop all notes
  SynthEngine* synth = getZombieSynth();
  if (synth) {
    synth->allNotesOff();
  }

  Arpeggiator* arp = getZombieArp();
  if (arp) {
    arp->allNotesOff();
  }

  ZombieSequencer* seq = getZombieSeq();
  if (seq) {
    seq->stop();
  }

  enterMode(MENU);
}

void setup() {
  Serial.begin(115200);
  Serial.println("ZOMBI SS Prophet Synthesizer");
  Serial.println("Initializing...");

  // Initialize SPI for touch - VSPI with custom pins
  Serial.println("Initializing touch screen...");
  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSPI);
  ts.setRotation(1); // Landscape mode
  Serial.println("Touch screen initialized");

  // Initialize display
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(THEME_BG);

  // Splash screen
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.setTextSize(2);
  tft.drawCentreString("ZOMBIE SS", 160, 80, 4);
  tft.setTextColor(THEME_ACCENT, THEME_BG);
  tft.drawCentreString("PROPHET SYNTHESIZER", 160, 120, 2);
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawCentreString("Initializing audio...", 160, 150, 2);

  delay(1000);

  // Initialize synth engine
  zombieSynthInit();
  Serial.println("Synth engine initialized");

  // Initialize MIDI input
  tft.drawCentreString("Initializing MIDI...    ", 160, 150, 2);
  midiInput.init();
  midiInput.setNoteOnCallback(onMIDINoteOn);
  midiInput.setNoteOffCallback(onMIDINoteOff);
  midiInput.setCCCallback(onMIDICC);
  midiInput.setPitchBendCallback(onMIDIPitchBend);
  Serial.println("MIDI input initialized");

  // Start audio processing task on Core 0
  tft.drawCentreString("Starting audio task...", 160, 150, 2);
  xTaskCreatePinnedToCore(
    audioTask,          // Task function
    "AudioTask",        // Task name
    8192,               // Stack size
    NULL,               // Parameters
    24,                 // Priority (high)
    &audioTaskHandle,   // Task handle
    0                   // Core 0
  );
  Serial.println("Audio task started on Core 0");

  delay(500);

  // Enter menu
  enterMode(MENU);
  Serial.println("Ready!");
}

void loop() {
  // Update touch state
  updateTouch();

  // Update MIDI input
  midiInput.update();

  // Handle back button (universal)
  if (currentMode != MENU && touch.justPressed && isButtonPressed(10, 10, 50, 25)) {
    exitToMenu();
    return;
  }

  // Mode-specific logic
  switch (currentMode) {
    case MENU:
      if (touch.justPressed) {
        // Check app icon touches
        int iconsPerRow = 3;
        int iconW = 90;
        int iconH = 60;
        int spacing = 10;
        int startX = (320 - (iconsPerRow * iconW + (iconsPerRow - 1) * spacing)) / 2;
        int startY = 100;

        for (int i = 0; i < numApps; i++) {
          int row = i / iconsPerRow;
          int col = i % iconsPerRow;
          int x = startX + col * (iconW + spacing);
          int y = startY + row * (iconH + spacing);

          if (isButtonPressed(x, y, iconW, iconH)) {
            enterMode(apps[i].mode);
            break;
          }
        }
      }
      break;

    case ZOMBIE_SYNTH:
      zombieSynthDraw();
      zombieSynthHandleTouch();
      zombieSynthUpdate();
      break;

    case ZOMBIE_ARP:
      zombieArpDraw();
      zombieArpHandleTouch();
      zombieArpUpdate();
      break;

    case ZOMBIE_SEQ:
      zombieSeqDraw();
      zombieSeqHandleTouch();
      zombieSeqUpdate();
      break;
  }

  delay(20); // 50 Hz UI refresh
}
