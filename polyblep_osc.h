#ifndef POLYBLEP_OSC_H
#define POLYBLEP_OSC_H

#include <Arduino.h>
#include "synth_config.h"

// ============================================================================
// PolyBLEP Oscillator
// ============================================================================
// Band-limited oscillator using Polyharmonic Bandlimited Step function
// Provides alias-free waveforms: sine, saw, square, triangle, pulse, noise
// ============================================================================

class PolyBLEPOscillator {
private:
  float phase;
  float phaseIncrement;
  float sampleRate;
  OscillatorType waveform;
  float pulseWidth;
  float lastOutput;

  // PolyBLEP residual function for anti-aliasing
  inline float polyBLEP(float t) {
    if (t < phaseIncrement) {
      t /= phaseIncrement;
      return t + t - t * t - 1.0f;
    } else if (t > 1.0f - phaseIncrement) {
      t = (t - 1.0f) / phaseIncrement;
      return t * t + t + t + 1.0f;
    }
    return 0.0f;
  }

public:
  PolyBLEPOscillator() :
    phase(0.0f),
    phaseIncrement(0.0f),
    sampleRate(SAMPLE_RATE),
    waveform(OSC_SAW),
    pulseWidth(0.5f),
    lastOutput(0.0f) {}

  void setSampleRate(float sr) {
    sampleRate = sr;
  }

  void setFrequency(float freq) {
    phaseIncrement = freq / sampleRate;
  }

  void setWaveform(OscillatorType type) {
    waveform = type;
  }

  void setPulseWidth(float pw) {
    pulseWidth = constrain(pw, 0.01f, 0.99f);
  }

  void reset() {
    phase = 0.0f;
    lastOutput = 0.0f;
  }

  float process() {
    float output = 0.0f;

    switch (waveform) {
      case OSC_SINE:
        output = sin(phase * 2.0f * PI);
        break;

      case OSC_SAW: {
        output = (2.0f * phase) - 1.0f;
        output -= polyBLEP(phase);
        break;
      }

      case OSC_SQUARE: {
        output = (phase < 0.5f) ? 1.0f : -1.0f;
        output += polyBLEP(phase);
        output -= polyBLEP(fmod(phase + 0.5f, 1.0f));
        break;
      }

      case OSC_TRIANGLE: {
        // Generate triangle from integrated square wave
        float t = (phase < 0.5f) ? 1.0f : -1.0f;
        output = phaseIncrement * (t + polyBLEP(phase) - polyBLEP(fmod(phase + 0.5f, 1.0f)));
        output = (phaseIncrement * 4.0f) * (lastOutput + output);
        lastOutput = output;
        break;
      }

      case OSC_PULSE: {
        output = (phase < pulseWidth) ? 1.0f : -1.0f;
        output += polyBLEP(phase);
        output -= polyBLEP(fmod(phase + (1.0f - pulseWidth), 1.0f));
        break;
      }

      case OSC_NOISE:
        output = ((float)random(-32768, 32767)) / 32768.0f;
        break;
    }

    // Advance phase
    phase += phaseIncrement;
    if (phase >= 1.0f) {
      phase -= 1.0f;
    }

    return output;
  }

  float getPhase() const { return phase; }
};

#endif // POLYBLEP_OSC_H
