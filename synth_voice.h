#ifndef SYNTH_VOICE_H
#define SYNTH_VOICE_H

#include "synth_config.h"
#include "polyblep_osc.h"
#include "adsr_envelope.h"
#include "filter.h"

// ============================================================================
// Synth Voice
// ============================================================================
// Single voice with dual oscillators + sub, filter, and envelope
// ============================================================================

class SynthVoice {
private:
  PolyBLEPOscillator osc1;
  PolyBLEPOscillator osc2;
  PolyBLEPOscillator subOsc;
  ADSREnvelope ampEnv;
  ADSREnvelope filterEnv;
  StateVariableFilter filter;

  uint8_t currentNote;
  uint8_t velocity;
  bool active;

  // Voice parameters
  float osc1Level;
  float osc2Level;
  float subLevel;
  float osc2Detune;      // In cents
  float filterEnvAmount;
  float pan;

public:
  SynthVoice() :
    currentNote(0),
    velocity(0),
    active(false),
    osc1Level(0.7f),
    osc2Level(0.7f),
    subLevel(0.5f),
    osc2Detune(0.0f),
    filterEnvAmount(0.5f),
    pan(0.5f) {}

  void init(float sampleRate) {
    osc1.setSampleRate(sampleRate);
    osc2.setSampleRate(sampleRate);
    subOsc.setSampleRate(sampleRate);
    ampEnv.setSampleRate(sampleRate);
    filterEnv.setSampleRate(sampleRate);
    filter.setSampleRate(sampleRate);

    // Sub oscillator is always sine wave
    subOsc.setWaveform(OSC_SINE);
  }

  void noteOn(uint8_t note, uint8_t vel) {
    currentNote = note;
    velocity = vel;
    active = true;

    // Convert MIDI note to frequency
    float freq = 440.0f * pow(2.0f, (note - 69) / 12.0f);

    // Set oscillator frequencies
    osc1.setFrequency(freq);
    osc2.setFrequency(freq * pow(2.0f, osc2Detune / 1200.0f)); // Detune in cents
    subOsc.setFrequency(freq * 0.5f); // Sub is one octave down

    // Trigger envelopes
    ampEnv.noteOn();
    filterEnv.noteOn();

    // Reset oscillator phases for consistent attack
    osc1.reset();
    osc2.reset();
    subOsc.reset();
  }

  void noteOff() {
    ampEnv.noteOff();
    filterEnv.noteOff();
  }

  bool isActive() const {
    return active && ampEnv.isActive();
  }

  uint8_t getNote() const {
    return currentNote;
  }

  float process() {
    if (!active) return 0.0f;

    // Generate oscillator outputs
    float osc1Out = osc1.process() * osc1Level;
    float osc2Out = osc2.process() * osc2Level;
    float subOut = subOsc.process() * subLevel;

    // Mix oscillators
    float mixed = (osc1Out + osc2Out + subOut) / 3.0f;

    // Apply filter with envelope modulation
    float filterEnvValue = filterEnv.process();
    float modulatedCutoff = filter.getCutoff() * (1.0f + filterEnvValue * filterEnvAmount);
    filter.setCutoff(modulatedCutoff);
    float filtered = filter.process(mixed);

    // Apply amplitude envelope
    float ampEnvValue = ampEnv.process();
    float output = filtered * ampEnvValue;

    // Apply velocity sensitivity
    output *= (velocity / 127.0f);

    // Update active state
    if (!ampEnv.isActive()) {
      active = false;
    }

    return output;
  }

  // Parameter setters
  void setOsc1Waveform(OscillatorType type) { osc1.setWaveform(type); }
  void setOsc2Waveform(OscillatorType type) { osc2.setWaveform(type); }
  void setOsc1Level(float level) { osc1Level = constrain(level, 0.0f, 1.0f); }
  void setOsc2Level(float level) { osc2Level = constrain(level, 0.0f, 1.0f); }
  void setSubLevel(float level) { subLevel = constrain(level, 0.0f, 1.0f); }
  void setOsc2Detune(float cents) { osc2Detune = constrain(cents, -100.0f, 100.0f); }
  void setPulseWidth(float pw) { osc1.setPulseWidth(pw); osc2.setPulseWidth(pw); }

  void setFilterCutoff(float freq) { filter.setCutoff(freq); }
  void setFilterResonance(float res) { filter.setResonance(res); }
  void setFilterType(FilterType type) { filter.setType(type); }
  void setFilterEnvAmount(float amount) { filterEnvAmount = constrain(amount, 0.0f, 1.0f); }

  void setAmpAttack(float seconds) { ampEnv.setAttack(seconds); }
  void setAmpDecay(float seconds) { ampEnv.setDecay(seconds); }
  void setAmpSustain(float level) { ampEnv.setSustain(level); }
  void setAmpRelease(float seconds) { ampEnv.setRelease(seconds); }

  void setFilterAttack(float seconds) { filterEnv.setAttack(seconds); }
  void setFilterDecay(float seconds) { filterEnv.setDecay(seconds); }
  void setFilterSustain(float level) { filterEnv.setSustain(level); }
  void setFilterRelease(float seconds) { filterEnv.setRelease(seconds); }

  void setPan(float p) { pan = constrain(p, 0.0f, 1.0f); }
  float getPan() const { return pan; }
};

#endif // SYNTH_VOICE_H
