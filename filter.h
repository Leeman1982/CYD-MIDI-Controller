#ifndef FILTER_H
#define FILTER_H

#include <Arduino.h>
#include "synth_config.h"

// ============================================================================
// State Variable Filter
// ============================================================================
// Multi-mode filter providing lowpass, highpass, bandpass, and notch
// Based on the Chamberlin state variable filter topology
// ============================================================================

class StateVariableFilter {
private:
  float sampleRate;
  float cutoff;
  float resonance;
  FilterType filterType;

  // State variables
  float lowpass;
  float bandpass;
  float highpass;
  float notch;

  // Filter coefficients
  float f;
  float q;

  void updateCoefficients() {
    // Clamp cutoff to valid range
    float fc = constrain(cutoff, 20.0f, sampleRate * 0.45f);

    // Calculate filter coefficients
    f = 2.0f * sin(PI * fc / sampleRate);
    q = 1.0f / constrain(resonance, 0.5f, 10.0f);
  }

public:
  StateVariableFilter() :
    sampleRate(SAMPLE_RATE),
    cutoff(1000.0f),
    resonance(0.707f),
    filterType(FILTER_LOWPASS),
    lowpass(0.0f),
    bandpass(0.0f),
    highpass(0.0f),
    notch(0.0f),
    f(0.0f),
    q(1.0f) {
    updateCoefficients();
  }

  void setSampleRate(float sr) {
    sampleRate = sr;
    updateCoefficients();
  }

  void setCutoff(float freq) {
    cutoff = freq;
    updateCoefficients();
  }

  void setResonance(float res) {
    resonance = res;
    updateCoefficients();
  }

  void setType(FilterType type) {
    filterType = type;
  }

  float getCutoff() const { return cutoff; }
  float getResonance() const { return resonance; }
  FilterType getType() const { return filterType; }

  void reset() {
    lowpass = 0.0f;
    bandpass = 0.0f;
    highpass = 0.0f;
    notch = 0.0f;
  }

  float process(float input) {
    // State variable filter algorithm
    lowpass += f * bandpass;
    highpass = input - lowpass - q * bandpass;
    bandpass += f * highpass;
    notch = highpass + lowpass;

    // Return selected output
    switch (filterType) {
      case FILTER_LOWPASS:
        return lowpass;
      case FILTER_HIGHPASS:
        return highpass;
      case FILTER_BANDPASS:
        return bandpass;
      case FILTER_NOTCH:
        return notch;
      default:
        return lowpass;
    }
  }
};

#endif // FILTER_H
