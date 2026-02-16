#ifndef SYNTH_ENGINE_H
#define SYNTH_ENGINE_H

// ═══════════════════════════════════════════════════════════════════════════════
// ZOMBIE SS – Prophet-8 Style PolyBLEP Synth Engine (RP2040 Port)
// ═══════════════════════════════════════════════════════════════════════════════
//
// 6-voice polyphony with dual oscillators per voice, state variable filter,
// dual ADSR envelopes (amp + filter), per-voice modulation routing.
// All DSP is platform-independent float math — I2S output handled externally.
//
// Changes from ESP32 original:
//   - MAX_VOICES reduced to 6 (no FPU on Cortex-M0+)
//   - I2S init/write removed (handled in main sketch via arduino-pico I2S)
//   - heap_caps_malloc → standard malloc
//   - No ESP32-specific includes
// ═══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>
#include "config.h"

// ── PolyBLEP anti-aliasing ──────────────────────────────────────────────────
inline float polyBlep(float t, float dt) {
  if (t < dt) {
    t /= dt;
    return t + t - t * t - 1.0f;
  } else if (t > 1.0f - dt) {
    t = (t - 1.0f) / dt;
    return t * t + t + t + 1.0f;
  }
  return 0.0f;
}

// ── ADSR Envelope ───────────────────────────────────────────────────────────
struct Envelope {
  float attack;
  float decay;
  float sustain;
  float release;

  EnvelopeState state;
  float level;
  float attackRate;
  float decayRate;
  float releaseRate;

  void init(float a, float d, float s, float r) {
    attack = a; decay = d; sustain = s; release = r;
    state = ENV_IDLE;
    level = 0.0f;
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
};

// ── State Variable Filter (Chamberlin) ──────────────────────────────────────
struct Filter {
  FilterType type;
  float cutoff;
  float resonance;
  float lp, bp, hp;
  float f, q;

  void init(FilterType t, float freq, float res) {
    type = t;
    cutoff = constrain(freq, 0.0f, 1.0f);
    resonance = constrain(res, 0.0f, 1.0f);
    lp = bp = hp = 0.0f;
    updateCoefficients();
  }

  void updateCoefficients() {
    q = 1.0f - resonance;
    q = constrain(q, 0.1f, 1.0f);
    f = constrain(cutoff * 2.0f, 0.0f, 1.9f * q);
  }

  void setCutoff(float freq) {
    cutoff = constrain(freq, 0.0f, 1.0f);
    updateCoefficients();
  }

  void setResonance(float res) {
    resonance = constrain(res, 0.0f, 1.0f);
    updateCoefficients();
  }

  float process(float input) {
    lp += f * bp;
    hp = input - lp - q * bp;
    bp += f * hp;
    lp = constrain(lp, -2.0f, 2.0f);
    bp = constrain(bp, -2.0f, 2.0f);
    hp = constrain(hp, -2.0f, 2.0f);
    switch (type) {
      case FILTER_LOWPASS:  return lp;
      case FILTER_HIGHPASS: return hp;
      case FILTER_BANDPASS: return bp;
      case FILTER_NOTCH:    return lp + hp;
      default: return input;
    }
  }
};

// ── PolyBLEP Oscillator ────────────────────────────────────────────────────
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
  void setPulseWidth(float pw) { pulseWidth = constrain(pw, 0.01f, 0.99f); }

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

// ── Voice (dual-oscillator Prophet style) ───────────────────────────────────
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
  Filter filter;
  Envelope ampEnv;
  Envelope filterEnv;

  void init() {
    note = -1; velocity = 0; active = false; noteOnTime = 0;
    pitchBendRatio = 1.0f; aftertouchMod = 0.0f;
    lfoFilterMod = 0.0f; lfoPitchMod = 1.0f; lfoAmpMod = 0.0f;
    osc2DetuneRatio = 1.005f; osc2SemiOffset = 0.0f;
    filterEnvAmt = 0.3f; baseCutoff = 0.8f;
    baseFreq1 = 440.0f; baseFreq2 = 440.0f * 1.005f;
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
    filter.f = constrain(filter.f + fMod, 0.0f, 1.9f * filter.q);
    float filtered = filter.process(oscMix);
    filter.f = savedF;

    float ampEnvValue = ampEnv.process();
    float ampScale = 1.0f - lfoAmpMod * 0.5f;
    float output = filtered * ampEnvValue * ampScale * (velocity / 127.0f);

    if (!ampEnv.isActive()) active = false;

    return output;
  }
};

// ── SynthEngine ─────────────────────────────────────────────────────────────
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
    for (int i = 1; i < MAX_VOICES; i++)
      if (voices[i].noteOnTime < oldestTime) { oldest = i; oldestTime = voices[i].noteOnTime; }
    return oldest;
  }

  void applyOsc2ToVoices() {
    float detuneRatio = 1.0f + osc2Detune;
    float semiMult = powf(2.0f, osc2Semitones / 12.0f);
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].osc2DetuneRatio = detuneRatio;
      voices[i].osc2SemiOffset = osc2Semitones;
      if (voices[i].active)
        voices[i].baseFreq2 = voices[i].baseFreq1 * detuneRatio * semiMult;
    }
  }

public:
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
  }

  // ── LFO routing ──────────────────────────────────────────────────────────
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
    res = constrain(res, 0.0f, 0.95f);
    for (int i = 0; i < MAX_VOICES; i++) voices[i].filter.setResonance(res);
  }
  void setLFOPWMod(float v) {
    globalLfoPWMod = v;
    float pw = 0.5f + v * 0.3f;
    pw = constrain(pw, 0.05f, 0.95f);
    for (int i = 0; i < MAX_VOICES; i++) voices[i].osc1.setPulseWidth(pw);
  }
  void setLFODetuneMod(float v) {
    float detune = osc2Detune + v * 0.01f;
    for (int i = 0; i < MAX_VOICES; i++) voices[i].osc2DetuneRatio = 1.0f + detune;
  }

  // ── Pitch bend ───────────────────────────────────────────────────────────
  void setPitchBend(int16_t bendVal) {
    float semitones = (bendVal / 8192.0f) * 2.0f;
    globalPitchBendRatio = powf(2.0f, semitones / 12.0f);
    for (int i = 0; i < MAX_VOICES; i++) voices[i].pitchBendRatio = globalPitchBendRatio;
  }

  // ── Aftertouch ───────────────────────────────────────────────────────────
  void setChannelAftertouch(uint8_t val) {
    globalAftertouch = val / 127.0f;
    for (int i = 0; i < MAX_VOICES; i++) voices[i].aftertouchMod = globalAftertouch;
  }

  // ── Getters ──────────────────────────────────────────────────────────────
  float getOsc2Detune()      { return osc2Detune; }
  float getOsc2Semitones()   { return osc2Semitones; }
  float getMasterVolume()    { return masterVolume; }
  float getOsc1Level()       { return osc1Level; }
  float getOsc2Level()       { return osc2Level; }
  float getFilterCutoff()    { return voices[0].filter.cutoff; }
  float getFilterResonance() { return voices[0].filter.resonance; }
  float getBaseCutoff()      { return voices[0].baseCutoff; }
  float getFilterEnvAmount() { return voices[0].filterEnvAmt; }
  FilterType getFilterType() { return voices[0].filter.type; }
  WaveformType getOsc1Wave() { return voices[0].osc1.waveform; }
  WaveformType getOsc2Wave() { return voices[0].osc2.waveform; }

  // Amp envelope getters
  float getAmpAttack()   { return voices[0].ampEnv.attack; }
  float getAmpDecay()    { return voices[0].ampEnv.decay; }
  float getAmpSustain()  { return voices[0].ampEnv.sustain; }
  float getAmpRelease()  { return voices[0].ampEnv.release; }

  // Filter envelope getters
  float getFilterAttack()   { return voices[0].filterEnv.attack; }
  float getFilterDecay()    { return voices[0].filterEnv.decay; }
  float getFilterSustain()  { return voices[0].filterEnv.sustain; }
  float getFilterRelease()  { return voices[0].filterEnv.release; }

  // ── Setters ──────────────────────────────────────────────────────────────
  void setOsc2Detune(float d) { osc2Detune = d; applyOsc2ToVoices(); }
  void setOsc2Semitones(float s) { osc2Semitones = s; applyOsc2ToVoices(); }
  void setMasterVolume(float vol) { masterVolume = constrain(vol, 0.0f, 1.0f); }
  void setOsc1Level(float l) { osc1Level = constrain(l, 0.0f, 1.0f); }
  void setOsc2Level(float l) { osc2Level = constrain(l, 0.0f, 1.0f); }

  void setOsc1Waveform(WaveformType wave) {
    for (int i = 0; i < MAX_VOICES; i++) voices[i].osc1.waveform = wave;
  }
  void setOsc2Waveform(WaveformType wave) {
    for (int i = 0; i < MAX_VOICES; i++) voices[i].osc2.waveform = wave;
  }

  void setFilterCutoff(float cutoff) {
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].baseCutoff = cutoff;
      voices[i].filter.setCutoff(cutoff);
    }
  }
  void setFilterResonance(float resonance) {
    for (int i = 0; i < MAX_VOICES; i++) voices[i].filter.setResonance(resonance);
  }
  void setFilterType(FilterType type) {
    for (int i = 0; i < MAX_VOICES; i++) voices[i].filter.type = type;
  }
  void setFilterEnvAmount(float amt) {
    for (int i = 0; i < MAX_VOICES; i++) voices[i].filterEnvAmt = amt;
  }

  void setAmpEnvelope(float a, float d, float s, float r) {
    for (int i = 0; i < MAX_VOICES; i++) voices[i].ampEnv.init(a, d, s, r);
  }
  void setFilterEnvelope(float a, float d, float s, float r) {
    for (int i = 0; i < MAX_VOICES; i++) voices[i].filterEnv.init(a, d, s, r);
  }

  // ── Note control ─────────────────────────────────────────────────────────
  void noteOn(int note, int velocity) {
    int idx = findVoice(note);
    voices[idx].noteOn(note, velocity);
  }

  void noteOff(int note) {
    for (int i = 0; i < MAX_VOICES; i++)
      if (voices[i].active && voices[i].note == note) voices[i].noteOff();
  }

  void allNotesOff() {
    for (int i = 0; i < MAX_VOICES; i++)
      if (voices[i].active) voices[i].noteOff();
  }

  // ── Audio render (called from Core 1) ─────────────────────────────────
  // Fills a stereo interleaved int16 buffer. Returns number of samples written.
  void renderBlock(int16_t* buffer, int blockSize) {
    float scale = 0.3f;  // PCM5102A headroom

    for (int i = 0; i < blockSize; i++) {
      float mix = 0.0f;

      for (int v = 0; v < MAX_VOICES; v++) {
        if (voices[v].active || voices[v].ampEnv.isActive()) {
          mix += voices[v].process();
        }
      }

      mix *= masterVolume * scale;

      // Clamp and convert to int16 stereo
      float clamped = constrain(mix, -1.0f, 1.0f);
      int16_t sample = (int16_t)(clamped * 32767.0f);
      buffer[i * 2]     = sample;  // Left
      buffer[i * 2 + 1] = sample;  // Right
    }
  }

  int getActiveVoiceCount() {
    int count = 0;
    for (int i = 0; i < MAX_VOICES; i++)
      if (voices[i].active) count++;
    return count;
  }
};

#endif
