#ifndef SYNTH_ENGINE_H
#define SYNTH_ENGINE_H

#include "config.h"
#include "zombie_effects.h"
#include <math.h>

// ── RP2040 Synth Engine ───────────────────────────────────────────────────────
// Prophet-8 style polyBLEP synthesizer — pure DSP, no hardware dependencies.
// Audio output (I2S) is handled by the main sketch on Core 1.

// ── PolyBLEP anti-aliasing ───────────────────────────────────────────────────
static inline float polyBlep(float t, float dt) {
  if (t < dt) {
    t /= dt;
    return t + t - t * t - 1.0f;
  } else if (t > 1.0f - dt) {
    t = (t - 1.0f) / dt;
    return t * t + t + t + 1.0f;
  }
  return 0.0f;
}

// ── ADSR Envelope ─────────────────────────────────────────────────────────────
struct Envelope {
  float attack, decay, sustain, release;
  EnvelopeState state;
  float level;
  float attackRate, decayRate, releaseRate;

  void init(float a, float d, float s, float r) {
    attack = a; decay = d; sustain = s; release = r;
    state = ENV_IDLE; level = 0.0f;
    attackRate  = (attack  > 0.0f) ? (1.0f / (attack  * SAMPLE_RATE)) : 1.0f;
    decayRate   = (decay   > 0.0f) ? ((1.0f - sustain) / (decay * SAMPLE_RATE)) : 1.0f;
    releaseRate = (release > 0.0f) ? (sustain / (release * SAMPLE_RATE)) : 1.0f;
  }

  void noteOn()  { state = ENV_ATTACK; }
  void noteOff() { state = ENV_RELEASE; }

  float process() {
    switch (state) {
      case ENV_ATTACK:
        level += attackRate;
        if (level >= 1.0f) { level = 1.0f; state = ENV_DECAY; }
        break;
      case ENV_DECAY:
        level -= decayRate;
        if (level <= sustain) { level = sustain; state = ENV_SUSTAIN; }
        break;
      case ENV_SUSTAIN:
        level = sustain;
        break;
      case ENV_RELEASE:
        level -= releaseRate;
        if (level <= 0.0f) { level = 0.0f; state = ENV_IDLE; }
        break;
      case ENV_IDLE:
        level = 0.0f;
        break;
    }
    return level;
  }

  bool isActive() { return state != ENV_IDLE; }

  // Update rates without resetting envelope state (safe for live parameter changes)
  void setParams(float a, float d, float s, float r) {
    attack = a; decay = d; sustain = s; release = r;
    attackRate  = (attack  > 0.0f) ? (1.0f / (attack  * SAMPLE_RATE)) : 1.0f;
    decayRate   = (decay   > 0.0f) ? ((1.0f - sustain) / (decay * SAMPLE_RATE)) : 1.0f;
    releaseRate = (release > 0.0f) ? (sustain / (release * SAMPLE_RATE)) : 1.0f;
  }
};

// ── State Variable Filter (Chamberlin) ────────────────────────────────────────
struct SVFilter {
  FilterType type;
  float cutoff, resonance;
  float lp, bp, hp;
  float f, q;

  void init(FilterType t, float freq, float res) {
    type = t;
    cutoff = fclamp(freq, 0.0f, 1.0f);
    resonance = fclamp(res, 0.0f, 1.0f);
    lp = bp = hp = 0.0f;
    updateCoefficients();
  }

  void updateCoefficients() {
    q = 1.0f - resonance;
    q = fclamp(q, 0.1f, 1.0f);
    f = fclamp(cutoff * 2.0f, 0.0f, 1.9f * q);
  }

  void setCutoff(float freq) {
    cutoff = fclamp(freq, 0.0f, 1.0f);
    updateCoefficients();
  }

  void setResonance(float res) {
    resonance = fclamp(res, 0.0f, 1.0f);
    updateCoefficients();
  }

  float process(float input) {
    lp += f * bp;
    hp = input - lp - q * bp;
    bp += f * hp;
    lp = fclamp(lp, -2.0f, 2.0f);
    bp = fclamp(bp, -2.0f, 2.0f);
    hp = fclamp(hp, -2.0f, 2.0f);

    switch (type) {
      case FILTER_LOWPASS:  return lp;
      case FILTER_HIGHPASS: return hp;
      case FILTER_BANDPASS: return bp;
      case FILTER_NOTCH:    return lp + hp;
      default: return input;
    }
  }
};

// ── PolyBLEP Oscillator ──────────────────────────────────────────────────────
struct Oscillator {
  WaveformType waveform;
  float phase, phase2, phase3;
  float frequency;
  float pulseWidth;
  uint32_t noiseSeed;

  void init(WaveformType wave) {
    waveform = wave;
    phase = 0.0f; phase2 = 0.33f; phase3 = 0.66f;
    frequency = 440.0f;
    pulseWidth = 0.5f;
    noiseSeed = 12345;
  }

  void setFrequency(float freq) { frequency = freq; }
  void setPulseWidth(float pw)  { pulseWidth = fclamp(pw, 0.01f, 0.99f); }

  float process() {
    float dt = frequency / SAMPLE_RATE;
    float sample = 0.0f;

    switch (waveform) {
      case WAVE_SAW:
        sample = 2.0f * phase - 1.0f;
        sample -= polyBlep(phase, dt);
        break;

      case WAVE_SQUARE:
        sample = (phase < 0.5f) ? 1.0f : -1.0f;
        sample += polyBlep(phase, dt);
        sample -= polyBlep(fmod(phase + 0.5f, 1.0f), dt);
        break;

      case WAVE_PULSE:
        sample = (phase < pulseWidth) ? 1.0f : -1.0f;
        sample += polyBlep(phase, dt);
        sample -= polyBlep(fmod(phase + (1.0f - pulseWidth), 1.0f), dt);
        break;

      case WAVE_TRIANGLE: {
        float t = phase;
        sample = -1.0f + (2.0f * t);
        sample = 2.0f * (fabsf(sample) - 0.5f);
        break;
      }

      case WAVE_SINE:
        sample = sinf(TWO_PI_F * phase);
        break;

      case WAVE_NOISE:
        noiseSeed = noiseSeed * 1664525u + 1013904223u;
        sample = ((int32_t)noiseSeed) / 2147483648.0f;
        break;

      case WAVE_SUPERSAW: {
        float dt2 = (frequency * 1.007f) / SAMPLE_RATE;
        float dt3 = (frequency * 0.993f) / SAMPLE_RATE;
        float s1 = 2.0f * phase  - 1.0f; s1 -= polyBlep(phase,  dt);
        float s2 = 2.0f * phase2 - 1.0f; s2 -= polyBlep(phase2, dt2);
        float s3 = 2.0f * phase3 - 1.0f; s3 -= polyBlep(phase3, dt3);
        sample = (s1 * 0.5f + s2 * 0.25f + s3 * 0.25f);
        phase2 += dt2; if (phase2 >= 1.0f) phase2 -= 1.0f;
        phase3 += dt3; if (phase3 >= 1.0f) phase3 -= 1.0f;
        break;
      }
    }

    if (waveform != WAVE_NOISE) {
      phase += dt;
      if (phase >= 1.0f) phase -= 1.0f;
    }
    return sample;
  }
};

// ── Synth Voice (2 osc, filter, 2 envelopes) ────────────────────────────────
struct Voice {
  int note;
  int velocity;
  bool active;
  unsigned long noteOnTime;

  float pitchBendRatio;
  float aftertouchMod;
  float lfoFilterMod;
  float lfoPitchMod;
  float lfoAmpMod;

  float osc2DetuneRatio;
  float osc2SemiOffset;
  float filterEnvAmt;
  float baseCutoff;

  float baseFreq1;
  float baseFreq2;

  Oscillator osc1;
  Oscillator osc2;
  SVFilter   filter;
  Envelope   ampEnv;
  Envelope   filterEnv;

  void init() {
    note = -1; velocity = 0; active = false; noteOnTime = 0;
    pitchBendRatio = 1.0f;
    aftertouchMod  = 0.0f;
    lfoFilterMod   = 0.0f;
    lfoPitchMod    = 1.0f;
    lfoAmpMod      = 0.0f;
    osc2DetuneRatio = 1.005f;
    osc2SemiOffset  = 0.0f;
    filterEnvAmt    = 0.3f;
    baseCutoff      = 0.8f;
    baseFreq1       = 440.0f;
    baseFreq2       = 440.0f * 1.005f;
    osc1.init(WAVE_SAW);
    osc2.init(WAVE_SAW);
    filter.init(FILTER_LOWPASS, 0.8f, 0.3f);
    ampEnv.init(0.01f, 0.3f, 0.7f, 0.5f);
    filterEnv.init(0.01f, 0.3f, 0.5f, 0.3f);
  }

  void noteOn(int n, int vel) {
    note = n; velocity = vel; active = true;
    noteOnTime = millis();
    baseFreq1 = 440.0f * powf(2.0f, (n - 69) / 12.0f);
    baseFreq2 = baseFreq1 * osc2DetuneRatio * powf(2.0f, osc2SemiOffset / 12.0f);
    osc1.setFrequency(baseFreq1);
    osc2.setFrequency(baseFreq2);
    osc1.phase = 0.0f;
    osc2.phase = 0.0f;
    ampEnv.noteOn();
    filterEnv.noteOn();
  }

  void noteOff() {
    ampEnv.noteOff();
    filterEnv.noteOff();
  }

  float process() {
    if (!active) return 0.0f;

    float mod = pitchBendRatio * lfoPitchMod;
    osc1.setFrequency(baseFreq1 * mod);
    osc2.setFrequency(baseFreq2 * mod);

    float osc1Out = osc1.process();
    float osc2Out = osc2.process();
    float oscMix = (osc1Out + osc2Out) * 0.5f;

    float filterEnvValue = filterEnv.process();
    float savedF = filter.f;
    float fMod = (filterEnvValue * filterEnvAmt + lfoFilterMod + aftertouchMod * 0.2f) * 2.0f;
    filter.f = fclamp(filter.f + fMod, 0.0f, 1.9f * filter.q);
    float filtered = filter.process(oscMix);
    filter.f = savedF;

    float ampEnvValue = ampEnv.process();
    float ampScale = 1.0f - lfoAmpMod * 0.5f;
    float output = filtered * ampEnvValue * ampScale * (velocity / 127.0f);

    if (!ampEnv.isActive()) active = false;
    return output;
  }
};

// ── Main Synth Engine ─────────────────────────────────────────────────────────
class SynthEngine {
private:
  Voice voices[MAX_VOICES];
  float masterVolume;
  float osc2Detune;
  float osc2Semitones;
  float osc1Level;
  float osc2Level;

  float globalLfoFilterMod;
  float globalLfoPitchMod;
  float globalLfoAmpMod;
  float globalLfoPWMod;
  float globalPitchBendRatio;
  float globalAftertouch;

  int findVoice(int note) {
    for (int i = 0; i < MAX_VOICES; i++)
      if (voices[i].active && voices[i].note == note) return i;
    for (int i = 0; i < MAX_VOICES; i++)
      if (!voices[i].active) return i;
    int oldest = 0;
    unsigned long oldestTime = voices[0].noteOnTime;
    for (int i = 1; i < MAX_VOICES; i++) {
      if (voices[i].noteOnTime < oldestTime) { oldest = i; oldestTime = voices[i].noteOnTime; }
    }
    return oldest;
  }

  void applyOsc2ToVoices() {
    float detuneRatio = 1.0f + osc2Detune;
    float semiMult = powf(2.0f, osc2Semitones / 12.0f);
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].osc2DetuneRatio = detuneRatio;
      voices[i].osc2SemiOffset  = osc2Semitones;
      if (voices[i].active)
        voices[i].baseFreq2 = voices[i].baseFreq1 * detuneRatio * semiMult;
    }
  }

public:
  ZombieEffects fx;

  SynthEngine() {
    masterVolume = 0.5f;
    osc1Level = 0.5f; osc2Level = 0.5f;
    osc2Detune = 0.005f; osc2Semitones = 0.0f;
    globalLfoFilterMod = 0.0f; globalLfoPitchMod = 1.0f;
    globalLfoAmpMod = 0.0f; globalLfoPWMod = 0.0f;
    globalPitchBendRatio = 1.0f; globalAftertouch = 0.0f;
  }

  void init() {
    for (int i = 0; i < MAX_VOICES; i++) voices[i].init();
    fx.init();
  }

  // ── Note control ──────────────────────────────────────────────────────────
  void noteOn(int note, int velocity) {
    int idx = findVoice(note);
    voices[idx].noteOn(note, velocity);
  }

  void noteOff(int note) {
    for (int i = 0; i < MAX_VOICES; i++)
      if (voices[i].active && voices[i].note == note)
        voices[i].noteOff();
  }

  void allNotesOff() {
    for (int i = 0; i < MAX_VOICES; i++)
      if (voices[i].active) voices[i].noteOff();
  }

  // ── Parameter setters ─────────────────────────────────────────────────────
  void setOsc1Waveform(WaveformType w) { for (int i = 0; i < MAX_VOICES; i++) voices[i].osc1.waveform = w; }
  void setOsc2Waveform(WaveformType w) { for (int i = 0; i < MAX_VOICES; i++) voices[i].osc2.waveform = w; }

  void setFilterCutoff(float c) {
    for (int i = 0; i < MAX_VOICES; i++) { voices[i].baseCutoff = c; voices[i].filter.setCutoff(c); }
  }
  void setFilterResonance(float r) {
    for (int i = 0; i < MAX_VOICES; i++) voices[i].filter.setResonance(r);
  }
  void setFilterType(FilterType t) {
    for (int i = 0; i < MAX_VOICES; i++) voices[i].filter.type = t;
  }

  void setAmpEnvelope(float a, float d, float s, float r) {
    for (int i = 0; i < MAX_VOICES; i++) voices[i].ampEnv.setParams(a, d, s, r);
  }
  void setFilterEnvelope(float a, float d, float s, float r) {
    for (int i = 0; i < MAX_VOICES; i++) voices[i].filterEnv.setParams(a, d, s, r);
  }
  void setFilterEnvAmount(float a) {
    for (int i = 0; i < MAX_VOICES; i++) voices[i].filterEnvAmt = a;
  }

  void setMasterVolume(float v) { masterVolume = fclamp(v, 0.0f, 1.0f); }
  void setOsc2Detune(float d)    { osc2Detune = d; applyOsc2ToVoices(); }
  void setOsc2Semitones(float s) { osc2Semitones = s; applyOsc2ToVoices(); }

  float getOsc2Detune()     { return osc2Detune; }
  float getOsc2Semitones()  { return osc2Semitones; }
  float getMasterVolume()   { return masterVolume; }
  float getFilterCutoff()   { return (MAX_VOICES > 0) ? voices[0].filter.cutoff : 0.5f; }
  float getFilterResonance(){ return (MAX_VOICES > 0) ? voices[0].filter.resonance : 0.3f; }

  // ── LFO routing ───────────────────────────────────────────────────────────
  void setLFOFilterMod(float v) {
    globalLfoFilterMod = v;
    for (int i = 0; i < MAX_VOICES; i++) voices[i].lfoFilterMod = v * 0.3f;
  }
  void setLFOPitchMod(float v) {
    float ratio = powf(2.0f, v * 2.0f / 12.0f);
    globalLfoPitchMod = ratio;
    for (int i = 0; i < MAX_VOICES; i++) voices[i].lfoPitchMod = ratio;
  }
  void setLFOAmpMod(float v) {
    globalLfoAmpMod = v;
    for (int i = 0; i < MAX_VOICES; i++) voices[i].lfoAmpMod = fabsf(v);
  }
  void setLFOResonanceMod(float v) {
    float res = voices[0].filter.resonance + v * 0.3f;
    res = fclamp(res, 0.0f, 0.95f);
    for (int i = 0; i < MAX_VOICES; i++) voices[i].filter.setResonance(res);
  }
  void setLFOPWMod(float v) {
    globalLfoPWMod = v;
    float pw = 0.5f + v * 0.3f;
    pw = fclamp(pw, 0.05f, 0.95f);
    for (int i = 0; i < MAX_VOICES; i++) voices[i].osc1.setPulseWidth(pw);
  }
  void setLFODetuneMod(float v) {
    float detune = osc2Detune + v * 0.01f;
    for (int i = 0; i < MAX_VOICES; i++) voices[i].osc2DetuneRatio = 1.0f + detune;
  }

  // ── Pitch bend / aftertouch ───────────────────────────────────────────────
  void setPitchBend(int16_t bendVal) {
    float semitones = (bendVal / 8192.0f) * 2.0f;
    globalPitchBendRatio = powf(2.0f, semitones / 12.0f);
    for (int i = 0; i < MAX_VOICES; i++) voices[i].pitchBendRatio = globalPitchBendRatio;
  }
  void setChannelAftertouch(uint8_t val) {
    globalAftertouch = val / 127.0f;
    for (int i = 0; i < MAX_VOICES; i++) voices[i].aftertouchMod = globalAftertouch;
  }

  // ── Audio rendering (called from Core 1) ──────────────────────────────────
  // Fills interleaved stereo int16_t buffer: [L0, R0, L1, R1, ...]
  void renderBlock(int16_t* buffer, int numFrames) {
    for (int i = 0; i < numFrames; i++) {
      float mixL = 0.0f, mixR = 0.0f;

      for (int v = 0; v < MAX_VOICES; v++) {
        if (voices[v].active || voices[v].ampEnv.isActive()) {
          float s = voices[v].process();
          mixL += s;
          mixR += s;
        }
      }

      mixL *= masterVolume * 0.3f;
      mixR *= masterVolume * 0.3f;

      float fxL, fxR;
      fx.process(mixL, mixR, fxL, fxR);

      buffer[i * 2]     = (int16_t)(fclamp(fxL, -1.0f, 1.0f) * 32767.0f);
      buffer[i * 2 + 1] = (int16_t)(fclamp(fxR, -1.0f, 1.0f) * 32767.0f);
    }
  }

  int getActiveVoiceCount() {
    int c = 0;
    for (int i = 0; i < MAX_VOICES; i++) if (voices[i].active) c++;
    return c;
  }
};

#endif
