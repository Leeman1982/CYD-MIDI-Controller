#ifndef ZOMBIE_LFO_H
#define ZOMBIE_LFO_H

// ZOMBIE SS – LFO Engine (RP2040 Port)
// Runs at 200Hz from Core 0 UI loop, routes to synth engine on Core 1.

#include <Arduino.h>
#include <math.h>
#include "config.h"

#define LFO_SAMPLE_RATE 200.0f

class LFOEngine {
public:
  LFOWave   wave     = LFO_SINE;
  LFOTarget target   = LFO_TARGET_FILTER;
  float     rate     = 2.0f;
  float     depth    = 0.0f;
  bool      enabled  = false;

  float phase     = 0.0f;
  float output    = 0.0f;
  float holdValue = 0.0f;

  void tick() {
    if (!enabled || depth < 0.001f) { output = 0.0f; return; }
    float dt = rate / LFO_SAMPLE_RATE;
    float raw = 0.0f;
    switch (wave) {
      case LFO_SINE:
        raw = sinf(phase * TWO_PI_F);
        break;
      case LFO_TRIANGLE:
        raw = (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
        break;
      case LFO_SAW:
        raw = 2.0f * phase - 1.0f;
        break;
      case LFO_SQUARE:
        raw = (phase < 0.5f) ? 1.0f : -1.0f;
        break;
      case LFO_RANDOM:
        if (phase + dt >= 1.0f) holdValue = ((float)random(0, 200) / 100.0f) - 1.0f;
        raw = holdValue;
        break;
    }
    output = raw * depth;
    phase += dt;
    if (phase >= 1.0f) phase -= 1.0f;
  }
};

#endif
