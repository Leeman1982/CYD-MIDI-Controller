#ifndef ZOMBIE_LFO_H
#define ZOMBIE_LFO_H

#include <Arduino.h>
#include <math.h>

#define LFO_SAMPLE_RATE 200.0f  // Run at 200Hz from UI task

enum LFOWave  { LFO_SINE, LFO_TRIANGLE, LFO_SAW, LFO_SQUARE, LFO_RANDOM };
enum LFOTarget{
  LFO_TARGET_FILTER    = 0,  // → filter cutoff
  LFO_TARGET_PITCH     = 1,  // → oscillator pitch
  LFO_TARGET_AMP       = 2,  // → amplitude (tremolo)
  LFO_TARGET_RESONANCE = 3,  // → filter resonance
  LFO_TARGET_PW        = 4,  // → osc1 pulse width
  LFO_TARGET_DETUNE    = 5   // → osc2 detune amount
};

const char* lfoWaveNames[]   = {"SINE","TRI","SAW","SQR","S&H"};
const char* lfoTargetNames[] = {"FILTER","PITCH","AMP","RESON","PW","DETUN"};

class LFOEngine {
public:
  LFOWave   wave     = LFO_SINE;
  LFOTarget target   = LFO_TARGET_FILTER;
  float     rate     = 2.0f;   // Hz
  float     depth    = 0.0f;   // 0-1
  bool      enabled  = false;

  float phase     = 0.0f;
  float output    = 0.0f;      // current value -1..+1 (scaled by depth)
  float holdValue = 0.0f;      // S&H

  void tick() {
    if (!enabled || depth < 0.001f) { output = 0.0f; return; }
    float dt = rate / LFO_SAMPLE_RATE;
    float raw = 0.0f;
    switch (wave) {
      case LFO_SINE:
        raw = sinf(phase * 6.28318f);
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
