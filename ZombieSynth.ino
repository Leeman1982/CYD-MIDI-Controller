/*******************************************************************
 ZOMBI SS PROPHET-8 SYNTHESIZER  v3
 Prophet-8 inspired polyBLEP synth for ESP32 CYD

 v3 Features:
 - 8-voice polyphony with polyBLEP oscillators
 - 7 waveforms: SAW / SQR / TRI / SIN / PUL / NOISE / SUPERSAW
 - State variable filter with ADSR envelopes + channel aftertouch
 - Pitch bend (±2 semitones, MIDI E0 message)
 - LFO (6 targets: FILTER/PITCH/AMP/RESONANCE/PW/DETUNE)
 - FX chain: Chorus → Ping-Pong Delay → Freeverb Reverb
 - 50-pattern arpeggiator with BPM ±1/±10 controls
 - Commercial-grade 16-step sequencer (per-step note/vel/gate/prob/tie)
 - Per-track: MIDI ch, output routing (INT/EXT/BOTH), polyrhythm
 - MIDI clock sync: BPM derived from incoming 24PPQN clock (GPIO 35)
 - Presets: 10 factory + 10 user (NVS persistent)
 - On-screen QWERTY for preset naming
 - Chord Pad: 8 chord types × 12 roots
 - Audio output selection: PCM5052 / Internal DAC / Speaker
 - PCM5052 external I2S DAC (GPIO 22/27/17)
 - Internal DAC → SC8002B amp → onboard speaker header (GPIO 26)
 - SD card framework (GPIO 5/18/19/23)
 *******************************************************************/

#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <TFT_eSPI.h>
#include <SD.h>

// Core engine and input
#include "synth_engine.h"
#include "midi_input.h"
#include "midi_output.h"
#include "arpeggiator_patterns.h"
#include "zombie_step_sequencer.h"

// v2 support files (included before mode files)
#include "zombie_lfo.h"
#include "zombie_presets.h"
#include "zombie_keyboard_input.h"

// UI elements and mode files (synth_mode first – defines synthParams + getZombieSynth)
#include "ui_elements.h"
#include "zombie_synth_mode.h"
#include "zombie_arp_mode.h"
#include "zombie_seq_mode.h"
#include "zombie_presets_mode.h"
#include "zombie_chord_pad.h"

// ── Hardware pins ──────────────────────────────────────────────────────────
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

// ── Global objects ─────────────────────────────────────────────────────────
SPIClass touchSPI = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft = TFT_eSPI();

MIDIInput  midiInput;
MIDIOutput midiOut;
TouchState touch;
AppMode    currentMode = MENU;

// Audio output mode (referenced by synth_engine.h extern)
// Default: SPEAKER (Internal DAC → SC8002B → P4 header).
// Switch to PCM5052 via Synth → OUT button when using external DAC.
AudioOutputMode audioOutputMode = AUDIO_SPEAKER;

// SD card — onboard slot uses VSPI default pins (free from touch/display)
// SD_CS=GPIO5  SD_SCK=GPIO18  SD_MISO=GPIO19  SD_MOSI=GPIO23
#define SD_CS_PIN 5
SPIClass sdSPI(VSPI);
bool sdCardAvailable = false;

// Global LFO shared across all modes
LFOEngine globalLFO;

// Last played MIDI note (for note-name corner display)
int lastPlayedMidiNote = -1;

// Audio task handle
TaskHandle_t audioTaskHandle = NULL;

// ── MIDI callbacks ─────────────────────────────────────────────────────────
void onMIDINoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
  lastPlayedMidiNote = note;

  SynthEngine*     synth = getZombieSynth();
  Arpeggiator*     arp   = getZombieArp();
  ZombieSequencer* seq   = getZombieSeq();

  // Live recording: feed note into sequencer when recording is active
  if (seq && seq->getIsRecording() && seq->getIsPlaying()) {
    seq->recordNote(note, velocity);
  } else {
    if (synth) synth->noteOn(note, velocity);
    if (arp)   arp->noteOn(note);
  }
}

void onMIDINoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
  SynthEngine* synth = getZombieSynth();
  Arpeggiator* arp   = getZombieArp();

  if (synth) synth->noteOff(note);
  if (arp)   arp->noteOff(note);
}

void onMIDICC(uint8_t channel, uint8_t cc, uint8_t value) {
  SynthEngine*     synth = getZombieSynth();
  Arpeggiator*     arp   = getZombieArp();
  ZombieSequencer* seq   = getZombieSeq();

  float v = value / 127.0f;

  switch (cc) {
    // ── Filter ──────────────────────────────────────────────────────────
    case 74: if (synth) { synth->setFilterCutoff(v);    synthParams.filterCutoff    = v; synthParams.needsRedraw = true; } break;
    case 71: if (synth) { synth->setFilterResonance(v); synthParams.filterResonance = v; synthParams.needsRedraw = true; } break;

    // ── Volume ──────────────────────────────────────────────────────────
    case 7:  if (synth) { synth->setMasterVolume(v); synthParams.masterVolume = v; synthParams.needsRedraw = true; } break;
    case 12: synthParams.osc1Level = v; synthParams.needsRedraw = true; break;
    case 13: synthParams.osc2Level = v; synthParams.needsRedraw = true; break;

    // ── Amp Envelope ────────────────────────────────────────────────────
    case 73: synthParams.ampAttack  = v*2.0f; if (synth) synth->setAmpEnvelope(synthParams.ampAttack, synthParams.ampDecay, synthParams.ampSustain, synthParams.ampRelease); synthParams.needsRedraw=true; break;
    case 75: synthParams.ampDecay   = v*2.0f; if (synth) synth->setAmpEnvelope(synthParams.ampAttack, synthParams.ampDecay, synthParams.ampSustain, synthParams.ampRelease); synthParams.needsRedraw=true; break;
    case 70: synthParams.ampSustain = v;      if (synth) synth->setAmpEnvelope(synthParams.ampAttack, synthParams.ampDecay, synthParams.ampSustain, synthParams.ampRelease); synthParams.needsRedraw=true; break;
    case 72: synthParams.ampRelease = v*2.0f; if (synth) synth->setAmpEnvelope(synthParams.ampAttack, synthParams.ampDecay, synthParams.ampSustain, synthParams.ampRelease); synthParams.needsRedraw=true; break;

    // ── Filter Envelope ─────────────────────────────────────────────────
    case 76: synthParams.filterAttack  = v*2.0f; if (synth) synth->setFilterEnvelope(synthParams.filterAttack, synthParams.filterDecay, synthParams.filterSustain, synthParams.filterRelease); synthParams.needsRedraw=true; break;
    case 77: synthParams.filterDecay   = v*2.0f; if (synth) synth->setFilterEnvelope(synthParams.filterAttack, synthParams.filterDecay, synthParams.filterSustain, synthParams.filterRelease); synthParams.needsRedraw=true; break;
    case 78: synthParams.filterSustain = v;      if (synth) synth->setFilterEnvelope(synthParams.filterAttack, synthParams.filterDecay, synthParams.filterSustain, synthParams.filterRelease); synthParams.needsRedraw=true; break;
    case 79: synthParams.filterRelease = v*2.0f; if (synth) synth->setFilterEnvelope(synthParams.filterAttack, synthParams.filterDecay, synthParams.filterSustain, synthParams.filterRelease); synthParams.needsRedraw=true; break;

    // ── Arpeggiator ─────────────────────────────────────────────────────
    case 80: if (arp) arp->setBPM(30.0f + v*270.0f);                        break;
    case 81: if (arp) arp->setPattern((ArpPattern)(int)(v * 49.0f));         break;
    case 82: if (arp) arp->setOctaveRange(1 + (int)(v * 3.0f));              break;
    case 83: if (arp) arp->setGateLength(10 + (int)(v * 90.0f));             break;

    // ── Sequencer ───────────────────────────────────────────────────────
    case 85: if (seq) seq->setBPM(40.0f + v * 260.0f);   break;
    case 86: if (seq) seq->setSwing(50 + (int)(v * 25)); break;

    // ── LFO (CC 87=rate, CC 88=depth) ───────────────────────────────────
    case 87: globalLFO.rate  = v * 20.0f;                             break;
    case 88: globalLFO.depth = v; globalLFO.enabled = (v > 0.01f);   break;
  }
}

void onMIDIPitchBend(uint8_t channel, int16_t bend) {
  SynthEngine* synth = getZombieSynth();
  if (synth) synth->setPitchBend(bend);
}

void onMIDIAftertouch(uint8_t channel, uint8_t val) {
  SynthEngine* synth = getZombieSynth();
  if (synth) synth->setChannelAftertouch(val);
}

void onMIDIPolyAT(uint8_t channel, uint8_t note, uint8_t val) {
  // Polyphonic aftertouch: treat as channel AT for simplicity
  // (full per-voice AT would require per-note voice lookup)
  SynthEngine* synth = getZombieSynth();
  if (synth) synth->setChannelAftertouch(val);
}

// MIDI clock sync — called on every 0xF8 tick from MIDI IN
// Sync sequencer and/or arp BPM when external clock is received
static bool midiClockSyncEnabled = false;  // Can be toggled from a UI setting
static uint8_t midiClockTickCount = 0;

void onMIDIClock() {
  if (!midiClockSyncEnabled) return;
  midiClockTickCount++;
  // Sync every 24 ticks (= 1 quarter note @ 24PPQN)
  if (midiClockTickCount >= 24) {
    midiClockTickCount = 0;
    float bpm = midiInput.getClockBPM();
    if (bpm > 20.0f && bpm < 400.0f) {
      ZombieSequencer* seq = getZombieSeq();
      Arpeggiator*     arp = getZombieArp();
      if (seq) seq->setBPM(bpm);
      if (arp) arp->setBPM(bpm);
    }
  }
}

void onMIDIStart() {
  ZombieSequencer* seq = getZombieSeq();
  Arpeggiator*     arp = getZombieArp();
  midiClockTickCount = 0;
  if (seq && !seq->getIsPlaying()) seq->play();
}

void onMIDIStop() {
  ZombieSequencer* seq = getZombieSeq();
  if (seq && seq->getIsPlaying()) seq->stop();
}

void onMIDIContinue() {
  ZombieSequencer* seq = getZombieSeq();
  if (seq && !seq->getIsPlaying()) seq->play();
}

// ── Audio task (Core 0) ────────────────────────────────────────────────────
void audioTask(void* parameter) {
  while (true) {
    SynthEngine* synth = getZombieSynth();
    if (synth) synth->processAudio();
    // No vTaskDelay — i2s_write(portMAX_DELAY) blocks until the DMA ring
    // buffer has room, providing natural backpressure at exactly 44100 Hz.
    // Adding any delay here risks DMA underruns and audio glitches.
  }
}

// ── Menu ───────────────────────────────────────────────────────────────────
struct AppIcon {
  const char* name;
  const char* symbol;
  AppMode     mode;
};

static const AppIcon apps[] = {
  {"SYNTH",   "SS",  ZOMBIE_SYNTH},
  {"ARP",     "ARP", ZOMBIE_ARP},
  {"SEQ",     "SEQ", ZOMBIE_SEQ},
  {"PRESETS", "PRE", ZOMBIE_PRESETS},
  {"CHORD",   "CHD", ZOMBIE_CHORD},
};
static const int NUM_APPS = 5;

// Layout: row0 = icons 0-2 (3 wide), row1 = icons 3-4 (2 wide, centred)
static const int ICON_W = 88, ICON_H = 58, ICON_GAP = 8;

static int iconX(int i) {
  if (i < 3) return (320 - 3*(ICON_W+ICON_GAP) + ICON_GAP) / 2 + (i % 3)*(ICON_W+ICON_GAP);
  else        return (320 - 2*(ICON_W+ICON_GAP) + ICON_GAP) / 2 + (i - 3)*(ICON_W+ICON_GAP);
}
static int iconY(int i) {
  return 85 + (i / 3) * (ICON_H + ICON_GAP);
}

void drawMenu() {
  tft.fillScreen(THEME_BG);

  // Header
  tft.drawRect(0, 0, 320, 62, THEME_OUTLINE);
  tft.drawRect(1, 1, 318, 60, THEME_OUTLINE);
  tft.drawRect(2, 2, 316, 58, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString("ZOMBI SS", 160, 8, 4);
  tft.setTextColor(THEME_ACCENT, THEME_BG);
  tft.drawCentreString("PROPHET SYNTHESIZER  v3", 160, 38, 2);

  // Status line
  SynthEngine* synth = getZombieSynth();
  if (synth) {
    char buf[20];
    sprintf(buf, "VOICES:%d/8", synth->getActiveVoiceCount());
    tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
    tft.drawRightString(buf, 314, 68, 2);
  }
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawString("v3.0", 6, 68, 2);

  // App icons
  for (int i = 0; i < NUM_APPS; i++) {
    int x = iconX(i), y = iconY(i);
    tft.fillRoundRect(x, y, ICON_W, ICON_H, 8, THEME_BG);
    tft.drawRoundRect(x, y, ICON_W, ICON_H, 8, THEME_OUTLINE);
    tft.drawRoundRect(x+1, y+1, ICON_W-2, ICON_H-2, 7, THEME_OUTLINE);
    tft.setTextColor(THEME_PRIMARY, THEME_BG);
    tft.drawCentreString(apps[i].symbol, x + ICON_W/2, y + 8,  4);
    tft.drawCentreString(apps[i].name,   x + ICON_W/2, y + 40, 2);
  }

  // SD / audio output status line at bottom
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  char statusLine[60];
  snprintf(statusLine, sizeof(statusLine), "SD:%s  OUT:%s",
           sdCardAvailable ? "OK" : "--",
           audioOutNames[audioOutputMode]);
  tft.drawCentreString(statusLine, 160, 228, 2);
}

void enterMode(AppMode mode) {
  currentMode = mode;
  tft.fillScreen(THEME_BG);
  switch (mode) {
    case ZOMBIE_SYNTH:   zombieSynthInit();   break;
    case ZOMBIE_ARP:     zombieArpInit();     break;
    case ZOMBIE_SEQ:     zombieSeqInit();     break;
    case ZOMBIE_PRESETS: zombiePresetsInit(); break;
    case ZOMBIE_CHORD:   zombieChordInit();   break;
    default:             drawMenu();          break;
  }
}

void exitToMenu() {
  SynthEngine*     synth = getZombieSynth();
  Arpeggiator*     arp   = getZombieArp();
  ZombieSequencer* seq   = getZombieSeq();
  if (synth) synth->allNotesOff();
  if (arp)   arp->allNotesOff();
  if (seq) {
    if (seq->getIsPlaying()) midiOut.stop();  // MIDI stop to external devices
    seq->stop();
    // All notes off on all MIDI channels used by tracks
    for (int t = 0; t < MAX_SEQ_TRACKS; t++) {
      SequencerTrack* tr = seq->getTrack(t);
      if (tr && (tr->midiOutput == SEQ_OUT_EXTERNAL || tr->midiOutput == SEQ_OUT_BOTH))
        midiOut.allNotesOff(tr->midiChannel - 1);
    }
  }
  chordAllOff();
  enterMode(MENU);
}

// ── SD card SPI helpers ────────────────────────────────────────────────────
// The onboard SD slot uses VSPI default pins (GPIO 18/19/23/5).
// Touch SPI also uses VSPI but remapped to GPIO 25/32/33/39.
// Both cannot run simultaneously on VSPI — swap as needed.
//
void sdBeginAccess() {
  // Pause audio, release touch VSPI, configure VSPI for SD
  if (audioTaskHandle) vTaskSuspend(audioTaskHandle);
  touchSPI.end();
  sdSPI.begin(18, 19, 23, SD_CS_PIN);  // SCK MISO MOSI CS
}
void sdEndAccess() {
  // Release SD VSPI, restore touch VSPI, resume audio
  sdSPI.end();
  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  if (audioTaskHandle) vTaskResume(audioTaskHandle);
}

// ── Setup ──────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("ZOMBI SS v3 — initializing");

  // ── SD card FIRST (before VSPI is remapped for touch) ───────────────────
  // After this block, sdSPI.end() frees the VSPI for touch remapping.
  sdSPI.begin(18, 19, 23, SD_CS_PIN);   // VSPI default: SCK MISO MOSI CS
  if (SD.begin(SD_CS_PIN, sdSPI)) {
    sdCardAvailable = true;
    if (!SD.exists("/ZOMBIESS")) SD.mkdir("/ZOMBIESS");
    Serial.printf("SD card OK (%llu MB)\n", SD.cardSize() / (1024*1024));
  } else {
    Serial.println("No SD card");
  }
  sdSPI.end();  // Free VSPI so touch can remap it below

  // ── Touch SPI (VSPI remapped to GPIO 25/39/32/33) ───────────────────────
  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  // Display initialization
  tft.init();
  tft.setRotation(1);
  tft.invertDisplay(true);   // Required for most CYD boards

  // CRITICAL: Enable backlight on GPIO 21
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);

  tft.fillScreen(THEME_BG);

  // Splash screen
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString("ZOMBI SS", 160, 75, 4);
  tft.setTextColor(THEME_ACCENT, THEME_BG);
  tft.drawCentreString("PROPHET SYNTHESIZER v3", 160, 112, 2);
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawCentreString(sdCardAvailable ? "SD card ready" : "No SD card", 160, 130, 2);
  tft.drawCentreString("Initializing audio...", 160, 148, 2);
  delay(800);

  // Init synth engine
  zombieSynthInit();
  Serial.println("Synth OK");

  // Init MIDI OUT
  tft.fillRect(0, 148, 320, 16, THEME_BG);
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawCentreString("Initializing MIDI OUT...", 160, 148, 2);
  midiOut.init();
  Serial.println("MIDI OUT OK");

  // Init MIDI IN
  tft.fillRect(0, 148, 320, 16, THEME_BG);
  tft.drawCentreString("Initializing MIDI IN...", 160, 148, 2);
  midiInput.init();
  midiInput.setNoteOnCallback(onMIDINoteOn);
  midiInput.setNoteOffCallback(onMIDINoteOff);
  midiInput.setCCCallback(onMIDICC);
  midiInput.setPitchBendCallback(onMIDIPitchBend);
  midiInput.setAftertouchCallback(onMIDIAftertouch);
  midiInput.setPolyATCallback(onMIDIPolyAT);
  midiInput.setClockCallback(onMIDIClock);
  midiInput.setStartCallback(onMIDIStart);
  midiInput.setStopCallback(onMIDIStop);
  midiInput.setContinueCallback(onMIDIContinue);
  Serial.println("MIDI OK");

  // Audio task on Core 0 (priority 24)
  tft.fillRect(0, 148, 320, 16, THEME_BG);
  tft.drawCentreString("Starting audio task...", 160, 148, 2);
  xTaskCreatePinnedToCore(audioTask, "AudioTask", 8192, NULL, 24, &audioTaskHandle, 0);
  Serial.println("Audio task on Core 0");

  delay(400);
  enterMode(MENU);
  Serial.println("Ready!");
}

// ── Main loop ──────────────────────────────────────────────────────────────
void loop() {
  // Update touch state
  updateTouch();

  // Process incoming MIDI
  midiInput.update();

  // ── LFO tick at 200 Hz ──────────────────────────────────────────────
  static unsigned long lastLfoTick = 0;
  if (millis() - lastLfoTick >= 5) {
    lastLfoTick = millis();
    globalLFO.tick();

    SynthEngine* synth = getZombieSynth();
    if (synth && globalLFO.enabled) {
      float out = globalLFO.output; // -1..+1 already depth-scaled
      switch (globalLFO.target) {
        case LFO_TARGET_FILTER:
          synth->setLFOFilterMod(out);
          break;
        case LFO_TARGET_PITCH:
          synth->setLFOPitchMod(out);
          break;
        case LFO_TARGET_AMP:
          synth->setLFOAmpMod(out);
          break;
        case LFO_TARGET_RESONANCE:
          synth->setLFOResonanceMod(out);
          break;
        case LFO_TARGET_PW:
          synth->setLFOPWMod(out);
          break;
        case LFO_TARGET_DETUNE:
          synth->setLFODetuneMod(out);
          break;
      }
    } else if (synth) {
      // LFO disabled: reset all LFO mods to neutral
      synth->setLFOFilterMod(0.0f);
      synth->setLFOPitchMod(0.0f);
      synth->setLFOAmpMod(0.0f);
    }
  }

  // ── Mode dispatch ────────────────────────────────────────────────────
  switch (currentMode) {
    case MENU:
      if (touch.justPressed) {
        for (int i = 0; i < NUM_APPS; i++) {
          if (isButtonPressed(iconX(i), iconY(i), ICON_W, ICON_H)) {
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

    case ZOMBIE_PRESETS:
      zombiePresetsDraw();
      zombiePresetsHandleTouch();
      zombiePresetsUpdate();
      break;

    case ZOMBIE_CHORD:
      zombieChordDraw();
      zombieChordHandleTouch();
      zombieChordUpdate();
      break;
  }

  delay(20);  // ~50 Hz UI refresh rate
}
