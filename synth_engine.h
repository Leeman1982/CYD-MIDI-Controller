#ifndef SYNTH_ENGINE_H
#define SYNTH_ENGINE_H

#include "synth_config.h"
#include "synth_voice.h"
#include "arpeggiator.h"
#include <driver/i2s.h>

// ============================================================================
// Synth Engine
// ============================================================================
// 6-voice polyphonic synthesizer engine with arpeggiator
// Manages voice allocation and audio rendering
// ============================================================================

class SynthEngine {
private:
  SynthVoice voices[MAX_VOICES];
  Arpeggiator arp;
  float sampleRate;
  bool initialized;

  // LFO for modulation
  float lfoPhase;
  float lfoRate;
  float lfoAmount;
  LFODestination lfoDestination;

  // Global parameters
  float masterVolume;
  OscillatorType osc1Waveform;
  OscillatorType osc2Waveform;
  float osc1Level;
  float osc2Level;
  float subLevel;
  float osc2Detune;
  float pulseWidth;

  float filterCutoff;
  float filterResonance;
  FilterType filterType;
  float filterEnvAmount;

  float ampAttack;
  float ampDecay;
  float ampSustain;
  float ampRelease;

  float filterAttack;
  float filterDecay;
  float filterSustain;
  float filterRelease;

  // Voice tracking for arpeggiator
  int lastArpNote;
  bool lastArpGate;

public:
  SynthEngine() :
    sampleRate(SAMPLE_RATE),
    initialized(false),
    lfoPhase(0.0f),
    lfoRate(2.0f),
    lfoAmount(0.0f),
    lfoDestination(LFO_DEST_FILTER_CUTOFF),
    masterVolume(0.7f),
    osc1Waveform(OSC_SAW),
    osc2Waveform(OSC_SAW),
    osc1Level(0.7f),
    osc2Level(0.7f),
    subLevel(0.5f),
    osc2Detune(7.0f),
    pulseWidth(0.5f),
    filterCutoff(2000.0f),
    filterResonance(0.707f),
    filterType(FILTER_LOWPASS),
    filterEnvAmount(0.5f),
    ampAttack(0.01f),
    ampDecay(0.1f),
    ampSustain(0.7f),
    ampRelease(0.3f),
    filterAttack(0.01f),
    filterDecay(0.2f),
    filterSustain(0.5f),
    filterRelease(0.5f),
    lastArpNote(-1),
    lastArpGate(false) {}

  void init() {
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].init(sampleRate);
      updateVoiceParameters(i);
    }
    initialized = true;
  }

  // MIDI note handling
  void noteOn(uint8_t note, uint8_t velocity) {
    // Pass to arpeggiator if enabled
    if (arp.isEnabled()) {
      arp.noteOn(note, velocity);
      return;
    }

    // Find free voice or steal oldest
    int voiceIndex = findFreeVoice();
    if (voiceIndex == -1) {
      voiceIndex = 0; // Steal first voice if none free
    }

    voices[voiceIndex].noteOn(note, velocity);
  }

  void noteOff(uint8_t note) {
    // Pass to arpeggiator if enabled
    if (arp.isEnabled()) {
      arp.noteOff(note);
      return;
    }

    // Find and release matching voice(s)
    for (int i = 0; i < MAX_VOICES; i++) {
      if (voices[i].isActive() && voices[i].getNote() == note) {
        voices[i].noteOff();
      }
    }
  }

  void allNotesOff() {
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].noteOff();
    }
    arp.allNotesOff();
  }

  // Process arpeggiator and generate audio sample
  float process() {
    if (!initialized) return 0.0f;

    // Update arpeggiator
    if (arp.isEnabled()) {
      auto arpEvent = arp.process();
      if (arpEvent.hasEvent) {
        if (arpEvent.gateOn) {
          // Note on from arpeggiator
          if (lastArpNote >= 0) {
            // Release previous arp note
            for (int i = 0; i < MAX_VOICES; i++) {
              if (voices[i].isActive() && voices[i].getNote() == lastArpNote) {
                voices[i].noteOff();
              }
            }
          }
          int voiceIndex = findFreeVoice();
          if (voiceIndex == -1) voiceIndex = 0;
          voices[voiceIndex].noteOn(arpEvent.note, arpEvent.velocity);
          lastArpNote = arpEvent.note;
          lastArpGate = true;
        } else {
          // Gate off from arpeggiator
          if (lastArpNote >= 0) {
            for (int i = 0; i < MAX_VOICES; i++) {
              if (voices[i].isActive() && voices[i].getNote() == lastArpNote) {
                voices[i].noteOff();
              }
            }
          }
          lastArpGate = false;
        }
      }
    }

    // Process LFO
    float lfoValue = sin(lfoPhase * 2.0f * PI) * lfoAmount;
    lfoPhase += lfoRate / sampleRate;
    if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;

    // Sum all voices
    float output = 0.0f;
    for (int i = 0; i < MAX_VOICES; i++) {
      output += voices[i].process();
    }

    // Apply master volume and prevent clipping
    output *= masterVolume;
    output = constrain(output, -1.0f, 1.0f);

    return output;
  }

  // Parameter setters that update all voices
  void setOsc1Waveform(OscillatorType type) {
    osc1Waveform = type;
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].setOsc1Waveform(type);
    }
  }

  void setOsc2Waveform(OscillatorType type) {
    osc2Waveform = type;
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].setOsc2Waveform(type);
    }
  }

  void setOsc1Level(float level) {
    osc1Level = level;
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].setOsc1Level(level);
    }
  }

  void setOsc2Level(float level) {
    osc2Level = level;
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].setOsc2Level(level);
    }
  }

  void setSubLevel(float level) {
    subLevel = level;
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].setSubLevel(level);
    }
  }

  void setOsc2Detune(float cents) {
    osc2Detune = cents;
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].setOsc2Detune(cents);
    }
  }

  void setPulseWidth(float pw) {
    pulseWidth = pw;
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].setPulseWidth(pw);
    }
  }

  void setFilterCutoff(float freq) {
    filterCutoff = freq;
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].setFilterCutoff(freq);
    }
  }

  void setFilterResonance(float res) {
    filterResonance = res;
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].setFilterResonance(res);
    }
  }

  void setFilterType(FilterType type) {
    filterType = type;
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].setFilterType(type);
    }
  }

  void setFilterEnvAmount(float amount) {
    filterEnvAmount = amount;
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].setFilterEnvAmount(amount);
    }
  }

  void setAmpAttack(float seconds) {
    ampAttack = seconds;
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].setAmpAttack(seconds);
    }
  }

  void setAmpDecay(float seconds) {
    ampDecay = seconds;
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].setAmpDecay(seconds);
    }
  }

  void setAmpSustain(float level) {
    ampSustain = level;
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].setAmpSustain(level);
    }
  }

  void setAmpRelease(float seconds) {
    ampRelease = seconds;
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].setAmpRelease(seconds);
    }
  }

  void setFilterAttack(float seconds) {
    filterAttack = seconds;
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].setFilterAttack(seconds);
    }
  }

  void setFilterDecay(float seconds) {
    filterDecay = seconds;
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].setFilterDecay(seconds);
    }
  }

  void setFilterSustain(float level) {
    filterSustain = level;
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].setFilterSustain(level);
    }
  }

  void setFilterRelease(float seconds) {
    filterRelease = seconds;
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].setFilterRelease(seconds);
    }
  }

  void setMasterVolume(float vol) {
    masterVolume = constrain(vol, 0.0f, 1.0f);
  }

  void setLFORate(float rate) { lfoRate = constrain(rate, 0.1f, 20.0f); }
  void setLFOAmount(float amount) { lfoAmount = constrain(amount, 0.0f, 1.0f); }
  void setLFODestination(LFODestination dest) { lfoDestination = dest; }

  // Arpeggiator control
  Arpeggiator& getArpeggiator() { return arp; }

  // Getters
  float getFilterCutoff() const { return filterCutoff; }
  float getFilterResonance() const { return filterResonance; }
  float getOsc1Level() const { return osc1Level; }
  float getOsc2Level() const { return osc2Level; }
  float getSubLevel() const { return subLevel; }
  float getOsc2Detune() const { return osc2Detune; }

private:
  int findFreeVoice() {
    for (int i = 0; i < MAX_VOICES; i++) {
      if (!voices[i].isActive()) {
        return i;
      }
    }
    return -1; // No free voices
  }

  void updateVoiceParameters(int index) {
    voices[index].setOsc1Waveform(osc1Waveform);
    voices[index].setOsc2Waveform(osc2Waveform);
    voices[index].setOsc1Level(osc1Level);
    voices[index].setOsc2Level(osc2Level);
    voices[index].setSubLevel(subLevel);
    voices[index].setOsc2Detune(osc2Detune);
    voices[index].setPulseWidth(pulseWidth);

    voices[index].setFilterCutoff(filterCutoff);
    voices[index].setFilterResonance(filterResonance);
    voices[index].setFilterType(filterType);
    voices[index].setFilterEnvAmount(filterEnvAmount);

    voices[index].setAmpAttack(ampAttack);
    voices[index].setAmpDecay(ampDecay);
    voices[index].setAmpSustain(ampSustain);
    voices[index].setAmpRelease(ampRelease);

    voices[index].setFilterAttack(filterAttack);
    voices[index].setFilterDecay(filterDecay);
    voices[index].setFilterSustain(filterSustain);
    voices[index].setFilterRelease(filterRelease);
  }
};

#endif // SYNTH_ENGINE_H
