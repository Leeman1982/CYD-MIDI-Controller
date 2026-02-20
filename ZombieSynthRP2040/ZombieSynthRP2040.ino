/*******************************************************************
 *  ZOMBI SS PROPHET SYNTHESIZER v3 — RP2040 Dual-Core Edition
 *
 *  Full port of the ESP32 Zombie Synth V2 to RP2040 with:
 *    Core 0: UI (OLED + encoder + buttons), MIDI, LFO
 *    Core 1: Audio synthesis engine, I2S output (PCM5102 DAC)
 *
 *  Hardware:
 *    MCU:     RP2040 (Raspberry Pi Pico / Pico W)
 *    Audio:   PCM5102 I2S DAC (GPIO 20/21/22)
 *    Display: 1.3" SH1106 128x64 OLED I2C (GPIO 4/5)
 *    Input:   EC11 rotary encoder (GPIO 2/3/6) + 2 buttons (GPIO 7/8)
 *    MIDI:    5-pin DIN MIDI IN/OUT on UART0 (GPIO 0/1)
 *
 *  Board package: Earle Philhower's arduino-pico
 *  Libraries:     U8g2, I2S (built-in with arduino-pico)
 *
 *  Features ported from ESP32 Zombie Synth V2:
 *    - 8-voice polyBLEP synthesis (saw/square/tri/sine/pulse/noise/supersaw)
 *    - State variable filter (LP/HP/BP/Notch)
 *    - Dual ADSR envelopes (amp + filter)
 *    - Effects chain: Chorus → Ping-pong Delay → Freeverb Reverb
 *    - LFO with 5 waveforms and 6 targets
 *    - 50-pattern arpeggiator
 *    - 4-track 16-step sequencer with polyrhythm
 *    - Chord pad (8 types × 12 roots)
 *    - 10 factory + 10 user presets (EEPROM)
 *    - Full MIDI IN (note/CC/pitchbend/aftertouch/clock)
 *    - MIDI OUT for sequencer external routing
 *******************************************************************/

#include <I2S.h>
#include "config.h"
#include "synth_engine.h"
#include "zombie_lfo.h"
#include "arpeggiator_patterns.h"
#include "zombie_step_sequencer.h"
#include "zombie_presets.h"
#include "midi_handler.h"
#include "input_handler.h"
#include "oled_ui.h"
#include "ui_synth.h"
#include "ui_arp.h"
#include "ui_seq.h"
#include "ui_presets.h"
#include "ui_chord.h"

// ═══════════════════════════════════════════════════════════════════════════════
// Global Objects
// ═══════════════════════════════════════════════════════════════════════════════

SynthEngine     synthEngine;
SynthParams     synthParams;
LFOEngine       globalLFO;
Arpeggiator     arpeggiator;
ZombieSequencer sequencer;
PresetManager   presetMgr;
MIDIHandler     midiHandler;
InputHandler    inputHandler;

I2S             i2sOut(OUTPUT);

// Core synchronization — Core 1 waits for Core 0 to finish init
volatile bool   core0InitDone = false;

AppMode         currentMode = MODE_MENU;
int             lastPlayedMidiNote = -1;
bool            arpRunning = false;

// Menu items
#define NUM_MENU_ITEMS 5
static const char* menuItems[] = {"SYNTH","ARPEGGIATOR","SEQUENCER","PRESETS","CHORD PAD"};
static int menuSelected = 0;
static int menuScroll = 0;

// Audio buffer (interleaved stereo: L,R,L,R...)
static int16_t audioBuffer[BUFFER_SIZE * 2];

// Arp state
static int  arpLastNote = -1;
static unsigned long arpNoteOffTime = 0;

// LFO tick timer
static unsigned long lastLFOTick = 0;

// UI refresh timer
static unsigned long lastUIRefresh = 0;
#define UI_REFRESH_MS 33   // ~30 fps

// ═══════════════════════════════════════════════════════════════════════════════
// MIDI Callbacks (Core 0)
// ═══════════════════════════════════════════════════════════════════════════════

void onMIDINoteOn(uint8_t ch, uint8_t note, uint8_t vel) {
  lastPlayedMidiNote = note;

  if (currentMode == MODE_ARP && arpRunning) {
    arpeggiator.noteOn(note);
  } else {
    synthEngine.noteOn(note, vel);
  }

  if (sequencer.getIsRecording()) {
    sequencer.recordNote(note, vel);
  }
}

void onMIDINoteOff(uint8_t ch, uint8_t note, uint8_t vel) {
  if (currentMode == MODE_ARP && arpRunning) {
    arpeggiator.noteOff(note);
  } else {
    synthEngine.noteOff(note);
  }
}

void onMIDICC(uint8_t ch, uint8_t cc, uint8_t val) {
  float fval = val / 127.0f;
  switch (cc) {
    case 1:  globalLFO.depth = fval; break;         // Mod wheel → LFO depth
    case 7:  synthEngine.setMasterVolume(fval); break; // Volume
    case 74: synthEngine.setFilterCutoff(fval); synthParams.filterCutoff = fval; break;
    case 71: synthEngine.setFilterResonance(fval); synthParams.filterResonance = fval; break;
    case 73: synthParams.ampAttack = fval * 2.0f;    // Attack
             synthEngine.setAmpEnvelope(synthParams.ampAttack, synthParams.ampDecay,
                                         synthParams.ampSustain, synthParams.ampRelease);
             break;
    case 75: synthParams.ampDecay = fval * 2.0f;     // Decay
             synthEngine.setAmpEnvelope(synthParams.ampAttack, synthParams.ampDecay,
                                         synthParams.ampSustain, synthParams.ampRelease);
             break;
    case 70: synthParams.ampSustain = fval;           // Sustain
             synthEngine.setAmpEnvelope(synthParams.ampAttack, synthParams.ampDecay,
                                         synthParams.ampSustain, synthParams.ampRelease);
             break;
    case 72: synthParams.ampRelease = fval * 2.0f;    // Release
             synthEngine.setAmpEnvelope(synthParams.ampAttack, synthParams.ampDecay,
                                         synthParams.ampSustain, synthParams.ampRelease);
             break;
    case 120: case 123:
      synthEngine.allNotesOff(); break;               // All notes off
  }
}

void onMIDIPitchBend(uint8_t ch, int16_t bend) {
  synthEngine.setPitchBend(bend);
}

void onMIDIAftertouch(uint8_t ch, uint8_t val) {
  synthEngine.setChannelAftertouch(val);
}

// ── Sequencer Callbacks ─────────────────────────────────────────────────────
void seqNoteOn(int trackIdx, int noteNum, int vel) {
  SequencerTrack* tr = sequencer.getTrack(trackIdx);
  if (!tr) return;

  // Internal synth
  if (tr->midiOutput == SEQ_OUT_INTERNAL || tr->midiOutput == SEQ_OUT_BOTH) {
    synthEngine.noteOn(noteNum, vel);
    lastPlayedMidiNote = noteNum;
  }
  // External MIDI
  if (tr->midiOutput == SEQ_OUT_EXTERNAL || tr->midiOutput == SEQ_OUT_BOTH) {
    midiHandler.sendNoteOn(tr->midiChannel - 1, noteNum, vel);
  }
}

void seqNoteOff(int trackIdx, int noteNum) {
  SequencerTrack* tr = sequencer.getTrack(trackIdx);
  if (!tr) return;

  if (tr->midiOutput == SEQ_OUT_INTERNAL || tr->midiOutput == SEQ_OUT_BOTH)
    synthEngine.noteOff(noteNum);
  if (tr->midiOutput == SEQ_OUT_EXTERNAL || tr->midiOutput == SEQ_OUT_BOTH)
    midiHandler.sendNoteOff(tr->midiChannel - 1, noteNum);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Menu Drawing & Input
// ═══════════════════════════════════════════════════════════════════════════════

void drawMenu() {
  OledUI::clear();

  // Title — y is baseline in U8g2, FONT_LARGE ascent ~10px
  OledUI::drawCentered("ZOMBI SS", 12, FONT_LARGE);
  OledUI::drawCentered("PROPHET SYNTH v3", 22, FONT_SMALL);

  // Separator
  display.drawHLine(0, 24, 128);

  // Menu list
  display.setFont(FONT_MEDIUM);
  if (menuSelected < menuScroll) menuScroll = menuSelected;
  if (menuSelected >= menuScroll + 4) menuScroll = menuSelected - 3;

  for (int i = 0; i < 4 && (i + menuScroll) < NUM_MENU_ITEMS; i++) {
    int idx = i + menuScroll;
    int y = 26 + i * LINE_H;

    if (idx == menuSelected) {
      display.drawBox(0, y, 128, LINE_H);
      display.setDrawColor(0);
      display.drawStr(4, y + LINE_H - 2, menuItems[idx]);
      display.setDrawColor(1);
    } else {
      display.drawStr(4, y + LINE_H - 2, menuItems[idx]);
    }
  }

  OledUI::send();
}

void enterMode(AppMode mode) {
  currentMode = mode;
  switch (mode) {
    case MODE_SYNTH:   UISynth::enter();   break;
    case MODE_ARP:     UIArp::enter();     break;
    case MODE_SEQ:     UISeq::enter();     break;
    case MODE_PRESETS: UIPresets::enter();  break;
    case MODE_CHORD:   UIChord::enter();   break;
    default: break;
  }
}

bool handleMenuInput(InputEvent evt) {
  switch (evt) {
    case EVT_ENC_CW:
      if (menuSelected < NUM_MENU_ITEMS - 1) menuSelected++;
      break;
    case EVT_ENC_CCW:
      if (menuSelected > 0) menuSelected--;
      break;
    case EVT_ENC_PRESS:
    case EVT_CONFIRM_PRESS:
      enterMode((AppMode)(menuSelected + 1)); // +1 because MODE_MENU=0
      break;
    default: break;
  }
  return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Core 0 — Setup & Loop (UI, MIDI, LFO)
// ═══════════════════════════════════════════════════════════════════════════════

void setup() {
  // LED heartbeat — blink 3x to confirm code is running
  pinMode(LED_BUILTIN, OUTPUT);
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
  }

  // USB serial for debug — wait a moment for Serial Monitor to attach
  Serial.begin(115200);
  delay(500);
  Serial.println(F(""));
  Serial.println(F("========================================"));
  Serial.println(F("  ZOMBI SS PROPHET SYNTH v3 — RP2040"));
  Serial.println(F("========================================"));
  Serial.println(F("  If OLED is blank, change OLED_DRIVER"));
  Serial.println(F("  in config.h (0=SH1106, 1=SSD1306)"));
  Serial.println(F("========================================"));

  // OLED — init FIRST so we can show splash while rest initializes
  Serial.println(F("[INIT] OLED display..."));
  OledUI::init();
  OledUI::drawSplash();

  // MIDI
  Serial.println(F("[INIT] MIDI handler..."));
  midiHandler.init();
  midiHandler.setNoteOnCallback(onMIDINoteOn);
  midiHandler.setNoteOffCallback(onMIDINoteOff);
  midiHandler.setCCCallback(onMIDICC);
  midiHandler.setPitchBendCallback(onMIDIPitchBend);
  midiHandler.setAftertouchCallback(onMIDIAftertouch);

  // Input (encoder + buttons)
  Serial.println(F("[INIT] Input handler..."));
  inputHandler.init();

  // Synth engine (DSP init, effects alloc)
  Serial.println(F("[INIT] Synth engine + effects..."));
  synthEngine.init();

  // Default synth params (load factory preset 0)
  Serial.println(F("[INIT] Preset manager..."));
  presetMgr.init();
  UIPresets::applyPatch(factoryPresets[0]);

  // Sequencer callbacks
  Serial.println(F("[INIT] Sequencer..."));
  sequencer.setCallbacks(seqNoteOn, seqNoteOff);
  sequencer.init();

  // LFO
  globalLFO.enabled = false;
  globalLFO.depth = 0.0f;

  lastLFOTick = millis();
  lastUIRefresh = millis();

  // Signal Core 1 that initialization is complete
  core0InitDone = true;

  Serial.println(F("[INIT] All systems ready. Entering main loop."));
  Serial.print(F("[INIT] Free heap: "));
  Serial.print(rp2040.getFreeHeap());
  Serial.println(F(" bytes"));
}

void loop() {
  unsigned long now = millis();

  // ── MIDI Input ──────────────────────────────────────────────────────────
  midiHandler.update();

  // ── Input polling ───────────────────────────────────────────────────────
  inputHandler.update();

  // ── Arpeggiator update ──────────────────────────────────────────────────
  if (currentMode == MODE_ARP && arpRunning) {
    int arpNote = arpeggiator.update(now);
    if (arpNote >= 0) {
      // Note off previous
      if (arpLastNote >= 0) synthEngine.noteOff(arpLastNote);
      synthEngine.noteOn(arpNote, 100);
      lastPlayedMidiNote = arpNote;
      arpLastNote = arpNote;
      arpNoteOffTime = now + (unsigned long)(
        (60000.0f / arpeggiator.getBPM()) / 4.0f * arpeggiator.getGateLength() / 100.0f);
    }
    // Gate off
    if (arpLastNote >= 0 && now >= arpNoteOffTime) {
      synthEngine.noteOff(arpLastNote);
      arpLastNote = -1;
    }
  }

  // ── Sequencer update ────────────────────────────────────────────────────
  if (currentMode == MODE_SEQ) {
    sequencer.update(now);
  }

  // ── LFO tick (200 Hz) ──────────────────────────────────────────────────
  if (now - lastLFOTick >= 5) {
    lastLFOTick = now;
    globalLFO.tick();
    if (globalLFO.enabled) {
      float out = globalLFO.output;
      switch (globalLFO.target) {
        case LFO_TARGET_FILTER:    synthEngine.setLFOFilterMod(out);    break;
        case LFO_TARGET_PITCH:     synthEngine.setLFOPitchMod(out);     break;
        case LFO_TARGET_AMP:       synthEngine.setLFOAmpMod(out);       break;
        case LFO_TARGET_RESONANCE: synthEngine.setLFOResonanceMod(out); break;
        case LFO_TARGET_PW:        synthEngine.setLFOPWMod(out);        break;
        case LFO_TARGET_DETUNE:    synthEngine.setLFODetuneMod(out);    break;
      }
    }
  }

  // ── UI update (~30 fps) ─────────────────────────────────────────────────
  if (now - lastUIRefresh >= UI_REFRESH_MS) {
    lastUIRefresh = now;

    // Process input events
    InputEvent evt;
    while ((evt = inputHandler.getEvent()) != EVT_NONE) {
      bool stayInMode = true;

      switch (currentMode) {
        case MODE_MENU:    stayInMode = handleMenuInput(evt); break;
        case MODE_SYNTH:   stayInMode = UISynth::handleInput(evt); break;
        case MODE_ARP:     stayInMode = UIArp::handleInput(evt); break;
        case MODE_SEQ:     stayInMode = UISeq::handleInput(evt); break;
        case MODE_PRESETS: stayInMode = UIPresets::handleInput(evt); break;
        case MODE_CHORD:   stayInMode = UIChord::handleInput(evt); break;
      }

      if (!stayInMode) {
        currentMode = MODE_MENU;
      }
    }

    // Draw current mode
    switch (currentMode) {
      case MODE_MENU:    drawMenu(); break;
      case MODE_SYNTH:   UISynth::draw(); break;
      case MODE_ARP:     UIArp::draw(); break;
      case MODE_SEQ:     UISeq::draw(); break;
      case MODE_PRESETS: UIPresets::draw(); break;
      case MODE_CHORD:   UIChord::draw(); break;
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Core 1 — Audio Synthesis & I2S Output
// ═══════════════════════════════════════════════════════════════════════════════

void setup1() {
  // Wait for Core 0 to finish initializing synth engine and effects
  while (!core0InitDone) {
    delay(1);
  }

  // Configure I2S for PCM5102 DAC
  i2sOut.setBCLK(I2S_BCK_PIN);
  i2sOut.setDATA(I2S_DATA_PIN);
  // WS (LRCK) is automatically BCK+1 = GPIO 21
  i2sOut.setBitsPerSample(16);
  i2sOut.setBuffers(8, BUFFER_SIZE);  // 8 DMA buffers of BUFFER_SIZE frames
  i2sOut.begin(SAMPLE_RATE);
}

void loop1() {
  // Render a block of audio
  synthEngine.renderBlock(audioBuffer, BUFFER_SIZE);

  // Write interleaved stereo samples to I2S
  // i2sOut.write() blocks when DMA buffers are full, providing natural
  // backpressure at the sample rate — this is the audio clock.
  for (int i = 0; i < BUFFER_SIZE; i++) {
    i2sOut.write(audioBuffer[i * 2]);      // Left
    i2sOut.write(audioBuffer[i * 2 + 1]);  // Right
  }
}
