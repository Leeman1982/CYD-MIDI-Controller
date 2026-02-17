#ifndef ZOMBIE_LFO_H
#define ZOMBIE_LFO_H

#include <Arduino.h>
#include <math.h>

// ── Zombie LFO Engine ─────────────────────────────────────────────────────────
// Runs at 200 Hz from UI core (Core 0), outputs modulation to synth on Core 1

#define LFO_SAMPLE_RATE 200.0f

enum LFOWave   { LFO_SINE, LFO_TRIANGLE, LFO_SAW, LFO_SQUARE, LFO_RANDOM };
enum LFOTarget {
  LFO_TARGET_FILTER    = 0,
  LFO_TARGET_PITCH     = 1,
  LFO_TARGET_AMP       = 2,
  LFO_TARGET_RESONANCE = 3,
  LFO_TARGET_PW        = 4,
  LFO_TARGET_DETUNE    = 5
};

#define NUM_LFO_WAVES   5
#define NUM_LFO_TARGETS 6

static const char* lfoWaveNames[]   = {"SINE","TRI","SAW","SQR","S&H"};
static const char* lfoTargetNames[] = {"FILTR","PITCH","AMP","RESON","PW","DETUN"};

class LFOEngine {
public:
  LFOWave   wave     = LFO_SINE;
  LFOTarget target   = LFO_TARGET_FILTER;
  float     rate     = 2.0f;    // Hz
  float     depth    = 0.0f;    // 0-1
  bool      enabled  = false;

  float phase     = 0.0f;
  float output    = 0.0f;       // -1..+1 scaled by depth
  float holdValue = 0.0f;       // S&H latch

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
        if (phase + dt >= 1.0f)
          holdValue = ((float)random(0, 200) / 100.0f) - 1.0f;
        raw = holdValue;
        break;
    }

    output = raw * depth;
    phase += dt;
    if (phase >= 1.0f) phase -= 1.0f;
  }
};

#endif
