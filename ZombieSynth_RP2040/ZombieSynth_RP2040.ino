/*******************************************************************
 ZOMBIE SS PROPHET SYNTHESIZER – RP2040 Port
 Prophet-8 inspired polyBLEP synth for Raspberry Pi Pico

 Hardware:
   - RP2040 dual-core Cortex-M0+ @ 250 MHz (overclocked)
   - 1.3" SH1106 128×64 OLED (I2C on GPIO 4/5)
   - 4×4 matrix keypad (16 buttons, rows GPIO 6-9, cols GPIO 10-13)
   - PCM5102A I2S DAC (GPIO 20=DIN, 21=BCLK, 22=LRCLK)
   - MIDI IN/OUT (GPIO 0=TX, 1=RX, UART0 @ 31250 baud)

 Architecture:
   Core 0: UI rendering, button input, MIDI processing, LFO tick
   Core 1: Audio synthesis, effects chain, I2S output

 Features (all ported from CYD/ESP32 version):
   - 6-voice polyphony with polyBLEP oscillators (7 waveforms)
   - State variable filter with dual ADSR envelopes
   - Pitch bend, aftertouch, LFO (6 targets)
   - FX chain: Chorus → Ping-Pong Delay → Freeverb Reverb
   - 50-pattern arpeggiator
   - 4-track 16-step sequencer (per-step note/vel/gate/prob/tie)
   - Chord pad (8 types × 12 roots)
   - 10 factory + 10 user presets (LittleFS persistent)
   - Full MIDI CC mapping
   - Graphical OLED UI with 16-button 4×4 matrix navigation
 *******************************************************************/

#include <Arduino.h>
#include <I2S.h>
#include <LittleFS.h>

// Engine and modules
#include "config.h"
#include "synth_engine.h"
#include "zombie_effects.h"
#include "zombie_lfo.h"
#include "zombie_presets.h"
#include "arpeggiator_patterns.h"
#include "zombie_step_sequencer.h"
#include "midi_handler.h"
#include "button_input.h"
#include "oled_ui.h"

// ── Global Objects ──────────────────────────────────────────────────────────
SynthEngine    synth;
ZombieEffects  fx;
Arpeggiator    arp;
ZombieSequencer seq;
LFOEngine      globalLFO;
PresetManager  presetMgr;
MIDIInput      midiInput;
MIDIOutput     midiOut;
ButtonInput    buttons;

AppMode        currentMode = MODE_MENU;
int            lastPlayedMidiNote = -1;

// I2S output object (arduino-pico built-in)
I2S            i2sOut(OUTPUT);

// Audio buffer (stereo interleaved int16)
int16_t        audioBuf[BUFFER_SIZE * 2];

// ── Inter-core event queue ──────────────────────────────────────────────────
// Core 0 writes MIDI events, Core 1 reads them to drive the synth engine.
volatile MidiEvent midiQueue[MIDI_QUEUE_SIZE];
volatile uint8_t   mqHead = 0;  // Written by Core 0
volatile uint8_t   mqTail = 0;  // Read by Core 1

static inline void mqPush(uint8_t type, uint8_t d1, uint8_t d2, uint8_t d3 = 0) {
  uint8_t next = (mqHead + 1) & MIDI_QUEUE_MASK;
  if (next != mqTail) {  // Not full
    midiQueue[mqHead] = {type, d1, d2, d3};
    __dmb();  // Memory barrier
    mqHead = next;
  }
}

static inline bool mqPop(MidiEvent& ev) {
  if (mqTail == mqHead) return false;
  ev = midiQueue[mqTail];
  __dmb();
  mqTail = (mqTail + 1) & MIDI_QUEUE_MASK;
  return true;
}

// ── Preset load/save helpers (called from oled_ui.h) ────────────────────────
void loadPresetToSynth(int slot) {
  const SynthPatch* p = presetMgr.getPatch(slot);
  if (!p) return;

  synth.setOsc1Waveform((WaveformType)p->osc1Wave);
  synth.setOsc1Level(p->osc1Level);
  synth.setOsc2Waveform((WaveformType)p->osc2Wave);
  synth.setOsc2Level(p->osc2Level);
  synth.setOsc2Detune(p->osc2Detune);
  synth.setFilterType((FilterType)p->filterType);
  synth.setFilterCutoff(p->filterCutoff);
  synth.setFilterResonance(p->filterResonance);
  synth.setFilterEnvAmount(p->filterEnvAmount);
  synth.setAmpEnvelope(p->ampAttack, p->ampDecay, p->ampSustain, p->ampRelease);
  synth.setFilterEnvelope(p->filterAttack, p->filterDecay, p->filterSustain, p->filterRelease);
  synth.setMasterVolume(p->masterVolume);

  globalLFO.wave    = (LFOWave)p->lfoWave;
  globalLFO.rate    = p->lfoRate;
  globalLFO.depth   = p->lfoDepth;
  globalLFO.target  = (LFOTarget)p->lfoTarget;
  globalLFO.enabled = (p->lfoDepth > 0.01f);
}

void saveCurrentToPreset(int slot) {
  SynthPatch p;
  // Build patch name from slot number
  char name[PRESET_NAME_LEN + 1];
  snprintf(name, sizeof(name), "USER %d", slot - NUM_FACTORY);
  presetMgr.buildPatchFromParams(p, name);

  p.osc1Wave       = (int)synth.getOsc1Wave();
  p.osc1Level      = synth.getOsc1Level();
  p.osc2Wave       = (int)synth.getOsc2Wave();
  p.osc2Level      = synth.getOsc2Level();
  p.osc2Detune     = synth.getOsc2Detune();
  p.filterType     = (int)synth.getFilterType();
  p.filterCutoff   = synth.getFilterCutoff();
  p.filterResonance= synth.getFilterResonance();
  p.filterEnvAmount= synth.getFilterEnvAmount();
  p.ampAttack      = synth.getAmpAttack();
  p.ampDecay       = synth.getAmpDecay();
  p.ampSustain     = synth.getAmpSustain();
  p.ampRelease     = synth.getAmpRelease();
  p.filterAttack   = synth.getFilterAttack();
  p.filterDecay    = synth.getFilterDecay();
  p.filterSustain  = synth.getFilterSustain();
  p.filterRelease  = synth.getFilterRelease();
  p.lfoWave        = (int)globalLFO.wave;
  p.lfoRate        = globalLFO.rate;
  p.lfoDepth       = globalLFO.depth;
  p.lfoTarget      = (int)globalLFO.target;
  p.masterVolume   = synth.getMasterVolume();

  presetMgr.saveUserPreset(slot, p);
}

// ── MIDI Callbacks (run on Core 0, push events to Core 1 queue) ─────────────
void onMIDINoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
  lastPlayedMidiNote = note;

  if (seq.getIsRecording() && seq.getIsPlaying()) {
    seq.recordNote(note, velocity);
  } else {
    mqPush(EVENT_NOTE_ON, note, velocity);
    arp.noteOn(note);
  }
}

void onMIDINoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
  mqPush(EVENT_NOTE_OFF, note, 0);
  arp.noteOff(note);
}

void onMIDICC(uint8_t channel, uint8_t cc, uint8_t value) {
  float v = value / 127.0f;

  switch (cc) {
    // Filter
    case 74: synth.setFilterCutoff(v);    break;
    case 71: synth.setFilterResonance(v); break;

    // Volume / levels
    case 7:  synth.setMasterVolume(v);    break;
    case 12: synth.setOsc1Level(v);       break;
    case 13: synth.setOsc2Level(v);       break;

    // Amp Envelope
    case 73: synth.setAmpEnvelope(v * 2.0f, synth.getAmpDecay(), synth.getAmpSustain(), synth.getAmpRelease()); break;
    case 75: synth.setAmpEnvelope(synth.getAmpAttack(), v * 2.0f, synth.getAmpSustain(), synth.getAmpRelease()); break;
    case 70: synth.setAmpEnvelope(synth.getAmpAttack(), synth.getAmpDecay(), v, synth.getAmpRelease()); break;
    case 72: synth.setAmpEnvelope(synth.getAmpAttack(), synth.getAmpDecay(), synth.getAmpSustain(), v * 2.0f); break;

    // Filter Envelope
    case 76: synth.setFilterEnvelope(v * 2.0f, synth.getFilterDecay(), synth.getFilterSustain(), synth.getFilterRelease()); break;
    case 77: synth.setFilterEnvelope(synth.getFilterAttack(), v * 2.0f, synth.getFilterSustain(), synth.getFilterRelease()); break;
    case 78: synth.setFilterEnvelope(synth.getFilterAttack(), synth.getFilterDecay(), v, synth.getFilterRelease()); break;
    case 79: synth.setFilterEnvelope(synth.getFilterAttack(), synth.getFilterDecay(), synth.getFilterSustain(), v * 2.0f); break;

    // Arpeggiator
    case 80: arp.setBPM(30.0f + v * 270.0f);                    break;
    case 81: arp.setPattern((ArpPattern)(int)(v * 49.0f));       break;
    case 82: arp.setOctaveRange(1 + (int)(v * 3.0f));            break;
    case 83: arp.setGateLength(10 + (int)(v * 90.0f));           break;

    // Sequencer
    case 85: seq.setBPM(40.0f + v * 260.0f);                    break;
    case 86: seq.setSwing(50 + (int)(v * 25));                   break;

    // LFO
    case 87: globalLFO.rate  = v * 20.0f;                       break;
    case 88: globalLFO.depth = v; globalLFO.enabled = (v > 0.01f); break;
  }
}

void onMIDIPitchBend(uint8_t channel, int16_t bend) {
  synth.setPitchBend(bend);
}

void onMIDIAftertouch(uint8_t channel, uint8_t val) {
  synth.setChannelAftertouch(val);
}

void onMIDIPolyAT(uint8_t channel, uint8_t note, uint8_t val) {
  synth.setChannelAftertouch(val);
}

// MIDI clock sync
static bool    midiClockSyncEnabled = false;
static uint8_t midiClockTickCount   = 0;

void onMIDIClock() {
  if (!midiClockSyncEnabled) return;
  midiClockTickCount++;
  if (midiClockTickCount >= 24) {
    midiClockTickCount = 0;
    float bpm = midiInput.getClockBPM();
    if (bpm > 20.0f && bpm < 400.0f) {
      seq.setBPM(bpm);
      arp.setBPM(bpm);
    }
  }
}

void onMIDIStart() {
  midiClockTickCount = 0;
  if (!seq.getIsPlaying()) seq.play();
}

void onMIDIStop() {
  if (seq.getIsPlaying()) seq.stop();
}

void onMIDIContinue() {
  if (!seq.getIsPlaying()) seq.play();
}

// ── Sequencer note callbacks ────────────────────────────────────────────────
void seqNoteOnHandler(int trackIdx, int noteNum, int vel) {
  SequencerTrack* trk = seq.getTrack(trackIdx);
  if (!trk) return;

  bool playInternal = (trk->midiOutput == SEQ_OUT_INTERNAL || trk->midiOutput == SEQ_OUT_BOTH);
  bool playExternal = (trk->midiOutput == SEQ_OUT_EXTERNAL || trk->midiOutput == SEQ_OUT_BOTH);

  if (playInternal) {
    // Load track's patch before playing
    loadPresetToSynth(trk->soundPatchIdx);
    mqPush(EVENT_NOTE_ON, noteNum, vel);
  }
  if (playExternal) {
    midiOut.noteOn(trk->midiChannel - 1, noteNum, vel);
  }
  lastPlayedMidiNote = noteNum;
}

void seqNoteOffHandler(int trackIdx, int noteNum) {
  SequencerTrack* trk = seq.getTrack(trackIdx);
  if (!trk) return;

  bool playInternal = (trk->midiOutput == SEQ_OUT_INTERNAL || trk->midiOutput == SEQ_OUT_BOTH);
  bool playExternal = (trk->midiOutput == SEQ_OUT_EXTERNAL || trk->midiOutput == SEQ_OUT_BOTH);

  if (playInternal) mqPush(EVENT_NOTE_OFF, noteNum, 0);
  if (playExternal) midiOut.noteOff(trk->midiChannel - 1, noteNum);
}

// ── Exit to menu (called from oled_ui.h) ────────────────────────────────────
void exitToMenu() {
  synth.allNotesOff();
  arp.allNotesOff();
  if (seq.getIsPlaying()) {
    midiOut.stop();
    seq.stop();
    for (int t = 0; t < MAX_SEQ_TRACKS; t++) {
      SequencerTrack* tr = seq.getTrack(t);
      if (tr && (tr->midiOutput == SEQ_OUT_EXTERNAL || tr->midiOutput == SEQ_OUT_BOTH))
        midiOut.allNotesOff(tr->midiChannel - 1);
    }
  }
  currentMode = MODE_MENU;
  uiResetCursor();
}

// ═════════════════════════════════════════════════════════════════════════════
// CORE 1 – Audio Synthesis (setup1 / loop1)
// ═════════════════════════════════════════════════════════════════════════════
void setup1() {
  // I2S setup on Core 1
  i2sOut.setBCLK(I2S_BCLK_PIN);
  i2sOut.setDATA(I2S_DATA_PIN);
  i2sOut.setBitsPerSample(16);
  i2sOut.setBuffers(4, BUFFER_SIZE);
  i2sOut.begin(SAMPLE_RATE);
}

void loop1() {
  // Process inter-core MIDI events
  MidiEvent ev;
  while (mqPop(ev)) {
    switch (ev.type) {
      case EVENT_NOTE_ON:    synth.noteOn(ev.data1, ev.data2);  break;
      case EVENT_NOTE_OFF:   synth.noteOff(ev.data1);           break;
      case EVENT_ALL_OFF:    synth.allNotesOff();                break;
      case EVENT_PITCH_BEND: {
        int16_t bend = ((int16_t)ev.data1 << 8) | ev.data2;
        synth.setPitchBend(bend);
        break;
      }
      case EVENT_AFTERTOUCH: synth.setChannelAftertouch(ev.data1); break;
    }
  }

  // Render audio block
  synth.renderBlock(audioBuf, BUFFER_SIZE);

  // Apply effects chain (in-place stereo processing)
  for (int i = 0; i < BUFFER_SIZE; i++) {
    float inL = audioBuf[i * 2]     / 32768.0f;
    float inR = audioBuf[i * 2 + 1] / 32768.0f;
    float outL, outR;
    fx.process(inL, inR, outL, outR);
    audioBuf[i * 2]     = (int16_t)(constrain(outL, -1.0f, 1.0f) * 32767.0f);
    audioBuf[i * 2 + 1] = (int16_t)(constrain(outR, -1.0f, 1.0f) * 32767.0f);
  }

  // Write to I2S (blocks until DMA buffer has space)
  i2sOut.write((const uint8_t*)audioBuf, BUFFER_SIZE * 4);
}

// ═════════════════════════════════════════════════════════════════════════════
// CORE 0 – UI, MIDI, Arp/Seq (setup / loop)
// ═════════════════════════════════════════════════════════════════════════════
void setup() {
  // Overclock to 250 MHz
  set_sys_clock_khz(RP2040_CLOCK_MHZ * 1000, true);

  Serial.begin(115200);
  Serial.println("ZOMBIE SS RP2040 – initializing");

  // Init MIDI (Serial1: UART0)
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
  midiOut.init();
  Serial.println("MIDI OK");

  // Init synth engine
  synth.init();
  fx.init();
  Serial.println("Synth OK");

  // Init presets (LittleFS)
  presetMgr.init();
  loadPresetToSynth(0);  // Load PROPHET PAD as default
  Serial.println("Presets OK");

  // Init sequencer
  seq.setSynthEngine(&synth);
  seq.setCallbacks(seqNoteOnHandler, seqNoteOffHandler);
  seq.init();
  Serial.println("Sequencer OK");

  // Init buttons
  buttons.init();
  Serial.println("Buttons OK");

  // Init OLED display and UI
  uiInit();
  Serial.println("Display OK");

  Serial.println("Ready!");
}

void loop() {
  // Read buttons
  buttons.update();

  // Process incoming MIDI
  midiInput.update();

  // LFO tick at 200 Hz
  static unsigned long lastLfoTick = 0;
  unsigned long now = millis();
  if (now - lastLfoTick >= 5) {
    lastLfoTick = now;
    globalLFO.tick();

    if (globalLFO.enabled) {
      float out = globalLFO.output;
      switch (globalLFO.target) {
        case LFO_TARGET_FILTER:    synth.setLFOFilterMod(out);    break;
        case LFO_TARGET_PITCH:     synth.setLFOPitchMod(out);     break;
        case LFO_TARGET_AMP:       synth.setLFOAmpMod(out);       break;
        case LFO_TARGET_RESONANCE: synth.setLFOResonanceMod(out); break;
        case LFO_TARGET_PW:        synth.setLFOPWMod(out);        break;
        case LFO_TARGET_DETUNE:    synth.setLFODetuneMod(out);    break;
      }
    } else {
      synth.setLFOFilterMod(0.0f);
      synth.setLFOPitchMod(0.0f);
      synth.setLFOAmpMod(0.0f);
    }
  }

  // Arpeggiator update
  int arpNote = arp.update(now);
  if (arpNote >= 0) {
    mqPush(EVENT_NOTE_ON, arpNote, 100);
    lastPlayedMidiNote = arpNote;
  }

  // Sequencer update
  seq.update(now);

  // UI update (draw + input handling)
  uiUpdate();

  // ~50 Hz UI refresh
  delay(20);
}
