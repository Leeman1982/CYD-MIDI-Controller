/*******************************************************************
 ZOMBI SS SYNTHESIZER - OLED EDITION
 Compact synth for 128x64 I2C OLED with rotary encoder control

 Hardware Requirements:
 - ESP32 (any board with I2S DAC capability)
 - 128x64 I2C OLED Display (ST7567S controller)
 - Rotary encoder with integrated push button
 - 4 momentary switches (MODE, PLAY, OCT-, OCT+)
 - Audio output via I2S DAC or internal DAC

 Features:
 - 8-voice polyphony with polyBLEP oscillators
 - State variable filter with envelope
 - Full ADSR envelopes for amp and filter
 - 50-pattern arpeggiator
 - 16-step sequencer
 - USB and 5-pin DIN MIDI input
 - Compact UI optimized for monochrome OLED

 Pin Configuration (adjust as needed):
 - I2C SDA: GPIO 21 (OLED)
 - I2C SCL: GPIO 22 (OLED)
 - Encoder A: GPIO 34
 - Encoder B: GPIO 35
 - Encoder SW: GPIO 32
 - Button MODE: GPIO 33
 - Button PLAY: GPIO 25
 - Button OCT-: GPIO 26
 - Button OCT+: GPIO 27
 - I2S BCK: GPIO 22 (if using external DAC)
 - I2S WS: GPIO 27 (if using external DAC)
 - I2S DATA: GPIO 17 (if using external DAC)
 - MIDI IN: GPIO 16 (UART2 RX)
 *******************************************************************/

#include <Wire.h>
#include "synth_engine.h"
#include "midi_input.h"
#include "arpeggiator_patterns.h"
#include "zombie_step_sequencer.h"
#include "zombie_encoder.h"
#include "zombie_oled_ui.h"

// Global objects
ZombieOLEDUI ui;
RotaryEncoder encoder;
ButtonManager buttons;
MIDIInput midiInput;

// Audio task handle
TaskHandle_t audioTaskHandle = NULL;

// Synth instances (singleton pattern)
static SynthEngine* zombieSynth = nullptr;
static Arpeggiator* zombieArp = nullptr;
static ZombieSequencer* zombieSeq = nullptr;

// Current octave offset
int currentOctave = 0;

// Synth parameter values (for UI display)
struct SynthParams {
  // Oscillator
  int osc1Wave;     // 0-4 (SAW, SQR, TRI, SIN, PUL)
  int osc2Wave;
  float osc2Detune; // 0.0-0.02
  int osc2Semi;     // -12 to +12

  // Filter
  float filterCutoff;    // 0.0-1.0
  float filterResonance; // 0.0-1.0
  int filterType;        // 0-3 (LP, HP, BP, NOTCH)
  float filterEnvAmt;    // 0.0-1.0

  // Amp envelope
  float ampAttack;   // 0.001-2.0
  float ampDecay;    // 0.001-2.0
  float ampSustain;  // 0.0-1.0
  float ampRelease;  // 0.001-3.0

  // Filter envelope
  float filtAttack;
  float filtDecay;
  float filtSustain;
  float filtRelease;

  // LFO
  float lfoRate;     // 0.1-20.0 Hz
  float lfoDepth;    // 0.0-1.0
  int lfoTarget;     // 0-2 (FILTER, PITCH, AMP)
  int lfoWave;       // 0-3 (SIN, TRI, SQR, SAW)

  // Master
  float masterVolume; // 0.0-1.0
  int fineTune;       // -50 to +50 cents
};

SynthParams params = {
  // Oscillator defaults
  .osc1Wave = 0,      // SAW
  .osc2Wave = 0,      // SAW
  .osc2Detune = 0.005f,
  .osc2Semi = 0,

  // Filter defaults
  .filterCutoff = 0.5f,
  .filterResonance = 0.3f,
  .filterType = 0,    // LOWPASS
  .filterEnvAmt = 0.5f,

  // Amp envelope defaults
  .ampAttack = 0.01f,
  .ampDecay = 0.3f,
  .ampSustain = 0.7f,
  .ampRelease = 0.5f,

  // Filter envelope defaults
  .filtAttack = 0.01f,
  .filtDecay = 0.3f,
  .filtSustain = 0.5f,
  .filtRelease = 0.3f,

  // LFO defaults
  .lfoRate = 5.0f,
  .lfoDepth = 0.0f,
  .lfoTarget = 0,
  .lfoWave = 0,

  // Master defaults
  .masterVolume = 0.5f,
  .fineTune = 0
};

// Arpeggiator state
int arpBPM = 120;
int arpPattern = 0;
bool arpPlaying = false;

// Sequencer state
int seqBPM = 120;
int seqCurrentStep = 0;
bool seqPlaying = false;

// Preset state
int currentPreset = 0;
const char* presetNames[] = {
  "INIT",
  "BASS",
  "LEAD",
  "PAD",
  "PLUCK",
  "ARPEGGIO",
  "ORGAN",
  "STRINGS"
};

// Getter functions for singleton access
SynthEngine* getZombieSynth() { return zombieSynth; }
Arpeggiator* getZombieArp() { return zombieArp; }
ZombieSequencer* getZombieSeq() { return zombieSeq; }

// MIDI callbacks
void onMIDINoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
  int transposedNote = note + (currentOctave * 12);
  transposedNote = constrain(transposedNote, 0, 127);

  if (zombieSynth) {
    zombieSynth->noteOn(transposedNote, velocity);
  }

  if (zombieArp && arpPlaying) {
    zombieArp->noteOn(transposedNote);
  }
}

void onMIDINoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
  int transposedNote = note + (currentOctave * 12);
  transposedNote = constrain(transposedNote, 0, 127);

  if (zombieSynth) {
    zombieSynth->noteOff(transposedNote);
  }

  if (zombieArp && arpPlaying) {
    zombieArp->noteOff(transposedNote);
  }
}

void onMIDICC(uint8_t channel, uint8_t cc, uint8_t value) {
  if (!zombieSynth) return;

  float normalizedValue = value / 127.0f;

  switch (cc) {
    case 74: // Filter cutoff
      params.filterCutoff = normalizedValue;
      zombieSynth->setFilterCutoff(normalizedValue);
      break;
    case 71: // Filter resonance
      params.filterResonance = normalizedValue;
      zombieSynth->setFilterResonance(normalizedValue);
      break;
    case 7: // Master volume
      params.masterVolume = normalizedValue;
      zombieSynth->setMasterVolume(normalizedValue);
      break;
  }
}

void onMIDIPitchBend(uint8_t channel, int16_t bend) {
  // Pitch bend can be implemented if needed
}

// Audio processing task (runs on Core 0)
void audioTask(void* parameter) {
  while (true) {
    if (zombieSynth) {
      zombieSynth->processAudio();
    }
  }
}

// Initialize synth engine
void initSynth() {
  if (!zombieSynth) {
    zombieSynth = new SynthEngine();
    zombieSynth->init();
  }

  // Apply default parameters
  zombieSynth->setOsc1Waveform((WaveformType)params.osc1Wave);
  zombieSynth->setOsc2Waveform((WaveformType)params.osc2Wave);
  zombieSynth->setFilterCutoff(params.filterCutoff);
  zombieSynth->setFilterResonance(params.filterResonance);
  zombieSynth->setFilterType((FilterType)params.filterType);
  zombieSynth->setFilterEnvAmount(params.filterEnvAmt);
  zombieSynth->setAmpEnvelope(params.ampAttack, params.ampDecay, params.ampSustain, params.ampRelease);
  zombieSynth->setFilterEnvelope(params.filtAttack, params.filtDecay, params.filtSustain, params.filtRelease);
  zombieSynth->setMasterVolume(params.masterVolume);
}

// Initialize arpeggiator
void initArp() {
  if (!zombieArp) {
    zombieArp = new Arpeggiator();
    zombieArp->init(zombieSynth);
    zombieArp->setBPM(arpBPM);
    zombieArp->setPattern(arpPattern);
  }
}

// Initialize sequencer
void initSeq() {
  if (!zombieSeq) {
    zombieSeq = new ZombieSequencer();
    zombieSeq->init();
    zombieSeq->setBPM(seqBPM);
  }
}

// Update current parameter based on encoder input
void updateParameter(int delta) {
  SynthPage page = ui.getPage();
  int param = ui.getParamSelection();

  switch (page) {
    case PAGE_OSC:
      switch (param) {
        case 0: // OSC1 waveform
          params.osc1Wave = constrain(params.osc1Wave + delta, 0, 4);
          zombieSynth->setOsc1Waveform((WaveformType)params.osc1Wave);
          break;
        case 1: // OSC2 waveform
          params.osc2Wave = constrain(params.osc2Wave + delta, 0, 4);
          zombieSynth->setOsc2Waveform((WaveformType)params.osc2Wave);
          break;
        case 2: // OSC2 detune
          params.osc2Detune = constrain(params.osc2Detune + delta * 0.0001f, 0.0f, 0.02f);
          break;
        case 3: // OSC2 semitones
          params.osc2Semi = constrain(params.osc2Semi + delta, -12, 12);
          break;
      }
      break;

    case PAGE_FILTER:
      switch (param) {
        case 0: // Cutoff
          params.filterCutoff = constrain(params.filterCutoff + delta * 0.01f, 0.0f, 1.0f);
          zombieSynth->setFilterCutoff(params.filterCutoff);
          break;
        case 1: // Resonance
          params.filterResonance = constrain(params.filterResonance + delta * 0.01f, 0.0f, 1.0f);
          zombieSynth->setFilterResonance(params.filterResonance);
          break;
        case 2: // Type
          params.filterType = constrain(params.filterType + delta, 0, 3);
          zombieSynth->setFilterType((FilterType)params.filterType);
          break;
        case 3: // Env amount
          params.filterEnvAmt = constrain(params.filterEnvAmt + delta * 0.01f, 0.0f, 1.0f);
          zombieSynth->setFilterEnvAmount(params.filterEnvAmt);
          break;
      }
      break;

    case PAGE_ENV_AMP:
      switch (param) {
        case 0: // Attack
          params.ampAttack = constrain(params.ampAttack + delta * 0.01f, 0.001f, 2.0f);
          zombieSynth->setAmpEnvelope(params.ampAttack, params.ampDecay, params.ampSustain, params.ampRelease);
          break;
        case 1: // Decay
          params.ampDecay = constrain(params.ampDecay + delta * 0.01f, 0.001f, 2.0f);
          zombieSynth->setAmpEnvelope(params.ampAttack, params.ampDecay, params.ampSustain, params.ampRelease);
          break;
        case 2: // Sustain
          params.ampSustain = constrain(params.ampSustain + delta * 0.01f, 0.0f, 1.0f);
          zombieSynth->setAmpEnvelope(params.ampAttack, params.ampDecay, params.ampSustain, params.ampRelease);
          break;
        case 3: // Release
          params.ampRelease = constrain(params.ampRelease + delta * 0.01f, 0.001f, 3.0f);
          zombieSynth->setAmpEnvelope(params.ampAttack, params.ampDecay, params.ampSustain, params.ampRelease);
          break;
      }
      break;

    case PAGE_ENV_FILT:
      switch (param) {
        case 0: // Attack
          params.filtAttack = constrain(params.filtAttack + delta * 0.01f, 0.001f, 2.0f);
          zombieSynth->setFilterEnvelope(params.filtAttack, params.filtDecay, params.filtSustain, params.filtRelease);
          break;
        case 1: // Decay
          params.filtDecay = constrain(params.filtDecay + delta * 0.01f, 0.001f, 2.0f);
          zombieSynth->setFilterEnvelope(params.filtAttack, params.filtDecay, params.filtSustain, params.filtRelease);
          break;
        case 2: // Sustain
          params.filtSustain = constrain(params.filtSustain + delta * 0.01f, 0.0f, 1.0f);
          zombieSynth->setFilterEnvelope(params.filtAttack, params.filtDecay, params.filtSustain, params.filtRelease);
          break;
        case 3: // Release
          params.filtRelease = constrain(params.filtRelease + delta * 0.01f, 0.001f, 3.0f);
          zombieSynth->setFilterEnvelope(params.filtAttack, params.filtDecay, params.filtSustain, params.filtRelease);
          break;
      }
      break;

    case PAGE_LFO:
      switch (param) {
        case 0: // Rate
          params.lfoRate = constrain(params.lfoRate + delta * 0.1f, 0.1f, 20.0f);
          break;
        case 1: // Depth
          params.lfoDepth = constrain(params.lfoDepth + delta * 0.01f, 0.0f, 1.0f);
          break;
        case 2: // Target
          params.lfoTarget = constrain(params.lfoTarget + delta, 0, 2);
          break;
        case 3: // Waveform
          params.lfoWave = constrain(params.lfoWave + delta, 0, 3);
          break;
      }
      break;

    case PAGE_MASTER:
      switch (param) {
        case 0: // Volume
          params.masterVolume = constrain(params.masterVolume + delta * 0.01f, 0.0f, 1.0f);
          zombieSynth->setMasterVolume(params.masterVolume);
          break;
        case 1: // Voices (read-only, just display)
          break;
        case 2: // Octave
          currentOctave = constrain(currentOctave + delta, -2, 2);
          break;
        case 3: // Fine tune
          params.fineTune = constrain(params.fineTune + delta, -50, 50);
          break;
      }
      break;
  }

  ui.setNeedsRedraw(true);
}

void setup() {
  Serial.begin(115200);
  Serial.println("ZOMBI SS OLED Synthesizer");
  Serial.println("Initializing...");

  // Initialize I2C for OLED
  Wire.begin();

  // Initialize OLED UI
  ui.init();
  delay(2000); // Show splash screen

  // Initialize encoder and buttons
  encoder.init();
  buttons.init();
  Serial.println("Input initialized");

  // Initialize synth engine
  initSynth();
  Serial.println("Synth engine initialized");

  // Initialize arpeggiator
  initArp();
  Serial.println("Arpeggiator initialized");

  // Initialize sequencer
  initSeq();
  Serial.println("Sequencer initialized");

  // Initialize MIDI input
  midiInput.init();
  midiInput.setNoteOnCallback(onMIDINoteOn);
  midiInput.setNoteOffCallback(onMIDINoteOff);
  midiInput.setCCCallback(onMIDICC);
  midiInput.setPitchBendCallback(onMIDIPitchBend);
  Serial.println("MIDI input initialized");

  // Start audio processing task on Core 0
  xTaskCreatePinnedToCore(
    audioTask,
    "AudioTask",
    8192,
    NULL,
    24, // High priority
    &audioTaskHandle,
    0   // Core 0
  );
  Serial.println("Audio task started on Core 0");

  // Enter menu
  ui.setMode(OLED_MENU);
  ui.drawMenu();

  Serial.println("Ready!");
}

void loop() {
  // Update inputs
  encoder.update();
  buttons.update();
  midiInput.update();

  // Update arpeggiator if playing
  if (arpPlaying && zombieArp) {
    zombieArp->update();
  }

  // Update sequencer if playing
  if (seqPlaying && zombieSeq) {
    zombieSeq->update();
  }

  // Get encoder delta
  int encoderDelta = encoder.getDelta();

  // Handle mode-specific input
  OLEDMode mode = ui.getMode();

  switch (mode) {
    case OLED_MENU:
      // Encoder navigates menu
      if (encoderDelta != 0) {
        if (encoderDelta > 0) ui.menuDown();
        else ui.menuUp();
        ui.drawMenu();
      }

      // Encoder button selects menu item
      if (buttons.isEncoderPressed()) {
        int selection = ui.getMenuSelection();
        switch (selection) {
          case 0: ui.setMode(OLED_SYNTH); break;
          case 1: ui.setMode(OLED_ARP); break;
          case 2: ui.setMode(OLED_SEQ); break;
          case 3: ui.setMode(OLED_PRESET); break;
        }
      }
      break;

    case OLED_SYNTH:
      // MODE button changes page
      if (buttons.isModePressed()) {
        ui.pageNext();
      }

      // Encoder with MODE held changes parameter selection
      if (buttons.isModeHeld() && encoderDelta != 0) {
        if (encoderDelta > 0) ui.paramNext();
        else ui.paramPrev();
      }
      // Encoder alone changes parameter value
      else if (encoderDelta != 0) {
        updateParameter(encoderDelta);
      }

      // Octave buttons
      if (buttons.isOctDownPressed()) {
        currentOctave = constrain(currentOctave - 1, -2, 2);
        ui.setNeedsRedraw(true);
      }
      if (buttons.isOctUpPressed()) {
        currentOctave = constrain(currentOctave + 1, -2, 2);
        ui.setNeedsRedraw(true);
      }

      // Draw if needed
      if (ui.needsUpdate()) {
        ui.drawSynthMode(zombieSynth->getActiveVoiceCount(), params.masterVolume);
        ui.setNeedsRedraw(false);
      }

      // Encoder button returns to menu
      if (buttons.isEncoderPressed()) {
        ui.setMode(OLED_MENU);
        ui.drawMenu();
      }
      break;

    case OLED_ARP:
      // Encoder changes BPM
      if (encoderDelta != 0) {
        arpBPM = constrain(arpBPM + encoderDelta, 40, 240);
        if (zombieArp) zombieArp->setBPM(arpBPM);
        ui.setNeedsRedraw(true);
      }

      // MODE button changes pattern
      if (buttons.isModePressed()) {
        arpPattern = (arpPattern + 1) % 50;
        if (zombieArp) zombieArp->setPattern(arpPattern);
        ui.setNeedsRedraw(true);
      }

      // PLAY button toggles playback
      if (buttons.isPlayPressed()) {
        arpPlaying = !arpPlaying;
        if (!arpPlaying && zombieArp) {
          zombieArp->allNotesOff();
        }
        ui.setNeedsRedraw(true);
      }

      // Draw if needed
      if (ui.needsUpdate()) {
        ui.drawArpMode(arpPlaying, arpBPM, arpPattern);
        ui.setNeedsRedraw(false);
      }

      // Encoder button returns to menu
      if (buttons.isEncoderPressed()) {
        ui.setMode(OLED_MENU);
        ui.drawMenu();
      }
      break;

    case OLED_SEQ:
      // Encoder changes BPM
      if (encoderDelta != 0) {
        seqBPM = constrain(seqBPM + encoderDelta, 40, 240);
        if (zombieSeq) zombieSeq->setBPM(seqBPM);
        ui.setNeedsRedraw(true);
      }

      // PLAY button toggles playback
      if (buttons.isPlayPressed()) {
        seqPlaying = !seqPlaying;
        if (seqPlaying && zombieSeq) {
          zombieSeq->start();
        } else if (zombieSeq) {
          zombieSeq->stop();
        }
        ui.setNeedsRedraw(true);
      }

      // Draw if needed
      if (ui.needsUpdate()) {
        ui.drawSeqMode(seqPlaying, seqBPM, seqCurrentStep);
        ui.setNeedsRedraw(false);
      }

      // Encoder button returns to menu
      if (buttons.isEncoderPressed()) {
        ui.setMode(OLED_MENU);
        ui.drawMenu();
      }
      break;

    case OLED_PRESET:
      // Encoder selects preset
      if (encoderDelta != 0) {
        currentPreset = constrain(currentPreset + encoderDelta, 0, 7);
        ui.setNeedsRedraw(true);
      }

      // Encoder button loads preset
      if (buttons.isEncoderPressed()) {
        // Load preset (implementation would go here)
        Serial.print("Loading preset: ");
        Serial.println(presetNames[currentPreset]);
      }

      // Draw if needed
      if (ui.needsUpdate()) {
        ui.drawPresetMode(currentPreset, presetNames[currentPreset]);
        ui.setNeedsRedraw(false);
      }

      // MODE button returns to menu
      if (buttons.isModePressed()) {
        ui.setMode(OLED_MENU);
        ui.drawMenu();
      }
      break;
  }

  delay(20); // 50 Hz UI refresh
}
