#ifndef ADSR_ENVELOPE_H
#define ADSR_ENVELOPE_H

#include <Arduino.h>
#include "synth_config.h"

// ============================================================================
// ADSR Envelope Generator
// ============================================================================
// Classic Attack-Decay-Sustain-Release envelope
// Used for amplitude and filter modulation
// ============================================================================

enum EnvelopeState {
  ENV_IDLE = 0,
  ENV_ATTACK,
  ENV_DECAY,
  ENV_SUSTAIN,
  ENV_RELEASE
};

class ADSREnvelope {
private:
  float sampleRate;
  float attackRate;
  float decayRate;
  float sustainLevel;
  float releaseRate;

  EnvelopeState state;
  float currentLevel;
  float targetLevel;

public:
  ADSREnvelope() :
    sampleRate(SAMPLE_RATE),
    attackRate(0.01f),
    decayRate(0.01f),
    sustainLevel(0.7f),
    releaseRate(0.005f),
    state(ENV_IDLE),
    currentLevel(0.0f),
    targetLevel(0.0f) {}

  void setSampleRate(float sr) {
    sampleRate = sr;
  }

  // Set attack time in seconds
  void setAttack(float seconds) {
    seconds = max(0.001f, seconds);
    attackRate = 1.0f / (seconds * sampleRate);
  }

  // Set decay time in seconds
  void setDecay(float seconds) {
    seconds = max(0.001f, seconds);
    decayRate = 1.0f / (seconds * sampleRate);
  }

  // Set sustain level (0.0 - 1.0)
  void setSustain(float level) {
    sustainLevel = constrain(level, 0.0f, 1.0f);
  }

  // Set release time in seconds
  void setRelease(float seconds) {
    seconds = max(0.001f, seconds);
    releaseRate = 1.0f / (seconds * sampleRate);
  }

  void noteOn() {
    state = ENV_ATTACK;
    targetLevel = 1.0f;
  }

  void noteOff() {
    state = ENV_RELEASE;
    targetLevel = 0.0f;
  }

  void reset() {
    state = ENV_IDLE;
    currentLevel = 0.0f;
    targetLevel = 0.0f;
  }

  float process() {
    switch (state) {
      case ENV_IDLE:
        currentLevel = 0.0f;
        break;

      case ENV_ATTACK:
        currentLevel += attackRate;
        if (currentLevel >= 1.0f) {
          currentLevel = 1.0f;
          state = ENV_DECAY;
          targetLevel = sustainLevel;
        }
        break;

      case ENV_DECAY:
        currentLevel -= decayRate;
        if (currentLevel <= sustainLevel) {
          currentLevel = sustainLevel;
          state = ENV_SUSTAIN;
        }
        break;

      case ENV_SUSTAIN:
        currentLevel = sustainLevel;
        break;

      case ENV_RELEASE:
        currentLevel -= releaseRate;
        if (currentLevel <= 0.0f) {
          currentLevel = 0.0f;
          state = ENV_IDLE;
        }
        break;
    }

    return currentLevel;
  }

  bool isActive() const {
    return state != ENV_IDLE;
  }

  EnvelopeState getState() const {
    return state;
  }

  float getLevel() const {
    return currentLevel;
  }
};

#endif // ADSR_ENVELOPE_H
