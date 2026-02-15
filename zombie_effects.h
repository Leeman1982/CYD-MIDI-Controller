#ifndef ZOMBIE_EFFECTS_H
#define ZOMBIE_EFFECTS_H

#include <Arduino.h>
#include <math.h>

// ── Zombie Effects Engine ──────────────────────────────────────────────────────
// Chorus  : stereo, 2-voice LFO-modulated delay (heap)
// Delay   : ping-pong stereo delay (heap)
// Reverb  : Freeverb-style (4 comb + 2 allpass per channel, heap)
// All heap-allocated to avoid DRAM stack overflow on ESP32

#define SAMPLE_RATE_FX 44100

// ─────────────────────────────────────────────────────────────────────────────
// Chorus
// ─────────────────────────────────────────────────────────────────────────────
class ChorusEffect {
public:
  bool    enabled  = false;
  float   rate     = 0.5f;   // LFO Hz
  float   depth    = 0.003f; // seconds of modulation depth (0.001–0.010)
  float   mix      = 0.5f;   // wet/dry 0-1
  float   feedback = 0.0f;

private:
  static const int BUF_MS = 25;                          // max delay: 25 ms
  static const int BUF_LEN = (SAMPLE_RATE_FX * BUF_MS / 1000 + 16);

  int16_t* bufL = nullptr;
  int16_t* bufR = nullptr;
  int   writePos = 0;

  float lfoPhaseL = 0.0f;
  float lfoPhaseR = 0.5f;   // 180° apart for width

  float baseDelayMs = 8.0f; // centre of modulation in ms

public:
  void init() {
    bufL = (int16_t*)heap_caps_malloc(BUF_LEN * sizeof(int16_t), MALLOC_CAP_8BIT);
    bufR = (int16_t*)heap_caps_malloc(BUF_LEN * sizeof(int16_t), MALLOC_CAP_8BIT);
    if (bufL) memset(bufL, 0, BUF_LEN * sizeof(int16_t));
    if (bufR) memset(bufR, 0, BUF_LEN * sizeof(int16_t));
  }

  // Process one stereo sample pair. Returns wet+dry mixed.
  void process(float inL, float inR, float& outL, float& outR) {
    if (!enabled || !bufL || !bufR) { outL = inL; outR = inR; return; }

    float lfoL = sinf(lfoPhaseL * 6.28318f);
    float lfoR = sinf(lfoPhaseR * 6.28318f);
    lfoPhaseL += rate / (float)SAMPLE_RATE_FX;
    lfoPhaseR += rate / (float)SAMPLE_RATE_FX;
    if (lfoPhaseL >= 1.0f) lfoPhaseL -= 1.0f;
    if (lfoPhaseR >= 1.0f) lfoPhaseR -= 1.0f;

    // Delay times in samples
    float delSampL = (baseDelayMs + depth * 1000.0f * lfoL) * SAMPLE_RATE_FX / 1000.0f;
    float delSampR = (baseDelayMs + depth * 1000.0f * lfoR) * SAMPLE_RATE_FX / 1000.0f;
    delSampL = constrain(delSampL, 1.0f, (float)(BUF_LEN - 2));
    delSampR = constrain(delSampR, 1.0f, (float)(BUF_LEN - 2));

    // Linear interpolation from delay buffer
    int   idxL  = (int)delSampL;
    float fracL = delSampL - idxL;
    int   rL0   = (writePos - idxL + BUF_LEN) % BUF_LEN;
    int   rL1   = (rL0 - 1 + BUF_LEN) % BUF_LEN;
    float wetL  = (bufL[rL0] + fracL * (bufL[rL1] - bufL[rL0])) / 32768.0f;

    int   idxR  = (int)delSampR;
    float fracR = delSampR - idxR;
    int   rR0   = (writePos - idxR + BUF_LEN) % BUF_LEN;
    int   rR1   = (rR0 - 1 + BUF_LEN) % BUF_LEN;
    float wetR  = (bufR[rR0] + fracR * (bufR[rR1] - bufR[rR0])) / 32768.0f;

    // Write to delay buffer with optional feedback
    bufL[writePos] = (int16_t)constrain((inL + feedback * wetL) * 32767.0f, -32768.0f, 32767.0f);
    bufR[writePos] = (int16_t)constrain((inR + feedback * wetR) * 32767.0f, -32768.0f, 32767.0f);
    writePos = (writePos + 1) % BUF_LEN;

    outL = inL * (1.0f - mix) + wetL * mix;
    outR = inR * (1.0f - mix) + wetR * mix;
  }
};

// ─────────────────────────────────────────────────────────────────────────────
// Ping-Pong Delay
// ─────────────────────────────────────────────────────────────────────────────
class DelayEffect {
public:
  bool  enabled  = false;
  float delayMs  = 250.0f;  // ms default
  float feedback = 0.45f;   // 0-0.9
  float mix      = 0.35f;   // wet/dry

private:
  static const int MAX_MS = 375;   // 375ms max saves ~66KB vs 750ms (was 132KB, now 66KB)
  static const int BUF_LEN = (SAMPLE_RATE_FX * MAX_MS / 1000 + 8);

  int16_t* bufL = nullptr;
  int16_t* bufR = nullptr;
  int writePos  = 0;

public:
  void init() {
    bufL = (int16_t*)heap_caps_malloc(BUF_LEN * sizeof(int16_t), MALLOC_CAP_8BIT);
    bufR = (int16_t*)heap_caps_malloc(BUF_LEN * sizeof(int16_t), MALLOC_CAP_8BIT);
    if (bufL) memset(bufL, 0, BUF_LEN * sizeof(int16_t));
    if (bufR) memset(bufR, 0, BUF_LEN * sizeof(int16_t));
  }

  // Sync delay time to BPM (quarter note = 1 beat)
  void setBPM(float bpm) {
    if (bpm > 0.0f) delayMs = constrain(60000.0f / bpm, 50.0f, (float)MAX_MS);
  }

  void process(float inL, float inR, float& outL, float& outR) {
    if (!enabled || !bufL || !bufR) { outL = inL; outR = inR; return; }

    int delaySamps = (int)(delayMs * SAMPLE_RATE_FX / 1000.0f);
    delaySamps = constrain(delaySamps, 1, BUF_LEN - 1);

    int readPos = (writePos - delaySamps + BUF_LEN) % BUF_LEN;
    float wetL = bufL[readPos] / 32768.0f;
    float wetR = bufR[readPos] / 32768.0f;

    // Ping-pong: L delays go to R output and vice versa
    bufL[writePos] = (int16_t)constrain((inL + feedback * wetR) * 32767.0f, -32768.0f, 32767.0f);
    bufR[writePos] = (int16_t)constrain((inR + feedback * wetL) * 32767.0f, -32768.0f, 32767.0f);
    writePos = (writePos + 1) % BUF_LEN;

    outL = inL * (1.0f - mix) + wetL * mix;
    outR = inR * (1.0f - mix) + wetR * mix;
  }
};

// ─────────────────────────────────────────────────────────────────────────────
// Freeverb-style Reverb (4 comb + 2 allpass per channel)
// ─────────────────────────────────────────────────────────────────────────────
class ReverbEffect {
public:
  bool  enabled  = false;
  float roomSize = 0.5f;   // 0-1
  float damping  = 0.5f;   // 0-1
  float mix      = 0.3f;   // wet/dry

private:
  // Comb filter lengths (prime numbers, tuned for 44.1 kHz)
  static const int NC = 4;
  static const int NA = 2;
  const int combLensL[NC] = { 1116, 1188, 1277, 1356 };
  const int combLensR[NC] = { 1124, 1196, 1285, 1364 };
  const int apLensL[NA]   = { 556, 441 };
  const int apLensR[NA]   = { 564, 449 };

  float* combBufL[NC];
  float* combBufR[NC];
  int    combPosL[NC];
  int    combPosR[NC];
  float  combFilterL[NC];
  float  combFilterR[NC];

  float* apBufL[NA];
  float* apBufR[NA];
  int    apPosL[NA];
  int    apPosR[NA];

  float feedback = 0.84f;
  float dampVal  = 0.2f;

  inline float processComb(float* buf, int& pos, int len, float& flt, float input) {
    float out = buf[pos];
    flt = flt * dampVal + out * (1.0f - dampVal);
    buf[pos] = input + flt * feedback;
    if (++pos >= len) pos = 0;
    return out;
  }

  inline float processAllpass(float* buf, int& pos, int len, float input) {
    float out = buf[pos];
    buf[pos] = input + out * 0.5f;
    if (++pos >= len) pos = 0;
    return out - input;
  }

public:
  void init() {
    for (int i = 0; i < NC; i++) {
      combBufL[i] = (float*)heap_caps_malloc(combLensL[i] * sizeof(float), MALLOC_CAP_8BIT);
      combBufR[i] = (float*)heap_caps_malloc(combLensR[i] * sizeof(float), MALLOC_CAP_8BIT);
      if (combBufL[i]) memset(combBufL[i], 0, combLensL[i] * sizeof(float));
      if (combBufR[i]) memset(combBufR[i], 0, combLensR[i] * sizeof(float));
      combPosL[i] = combPosR[i] = 0;
      combFilterL[i] = combFilterR[i] = 0.0f;
    }
    for (int i = 0; i < NA; i++) {
      apBufL[i] = (float*)heap_caps_malloc(apLensL[i] * sizeof(float), MALLOC_CAP_8BIT);
      apBufR[i] = (float*)heap_caps_malloc(apLensR[i] * sizeof(float), MALLOC_CAP_8BIT);
      if (apBufL[i]) memset(apBufL[i], 0, apLensL[i] * sizeof(float));
      if (apBufR[i]) memset(apBufR[i], 0, apLensR[i] * sizeof(float));
      apPosL[i] = apPosR[i] = 0;
    }
    updateParams();
  }

  void updateParams() {
    feedback = 0.7f + roomSize * 0.28f;
    dampVal  = damping * 0.4f;
  }

  void process(float inL, float inR, float& outL, float& outR) {
    if (!enabled) { outL = inL; outR = inR; return; }

    // Mono sum input at lower level
    float mono = (inL + inR) * 0.015f;

    float wetL = 0.0f, wetR = 0.0f;
    for (int i = 0; i < NC; i++) {
      if (combBufL[i]) wetL += processComb(combBufL[i], combPosL[i], combLensL[i], combFilterL[i], mono);
      if (combBufR[i]) wetR += processComb(combBufR[i], combPosR[i], combLensR[i], combFilterR[i], mono);
    }
    for (int i = 0; i < NA; i++) {
      if (apBufL[i]) wetL = processAllpass(apBufL[i], apPosL[i], apLensL[i], wetL);
      if (apBufR[i]) wetR = processAllpass(apBufR[i], apPosR[i], apLensR[i], wetR);
    }

    outL = inL * (1.0f - mix) + wetL * mix;
    outR = inR * (1.0f - mix) + wetR * mix;
  }
};

// ─────────────────────────────────────────────────────────────────────────────
// Combined FX chain (order: Chorus → Delay → Reverb)
// ─────────────────────────────────────────────────────────────────────────────
class ZombieEffects {
public:
  ChorusEffect chorus;
  DelayEffect  delay;
  ReverbEffect reverb;

  void init() {
    chorus.init();
    delay.init();
    reverb.init();
  }

  void process(float inL, float inR, float& outL, float& outR) {
    float cL, cR, dL, dR;
    chorus.process(inL, inR, cL, cR);
    delay.process(cL, cR, dL, dR);
    reverb.process(dL, dR, outL, outR);
  }
};

#endif
