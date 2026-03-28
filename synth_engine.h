#ifndef SYNTH_ENGINE_H
#define SYNTH_ENGINE_H

#include <Arduino.h>
#include <driver/i2s.h>
#include "zombie_effects.h"

// Prophet-8 style synthesizer engine with polyBLEP oscillators
// Supports 8-voice polyphony with full ADSR, filter, and modulation

#define MAX_VOICES 8
#define SAMPLE_RATE 44100
#define BUFFER_SIZE 256
#define TWO_PI 6.28318530718f
#define MIDI_NOTE_COUNT 128

// ── Audio output configuration ────────────────────────────────────────────────
//
//  CYD (ESP32-2432S028R) pin audit:
//  Display HSPI : GPIO 2(DC) 12(MISO) 13(MOSI) 14(SCK) 15(CS) 21(BL)
//  Touch VSPI   : GPIO 25(CLK) 32(MOSI) 33(CS) 36(IRQ) 39(MISO)
//  RGB LED      : GPIO 4(Blue) 16(Green) 17(Red) ← not driven in our code
//  SD Card slot : GPIO 5(CS) 18(SCK) 19(MISO) 23(MOSI)   ← all free
//  Internal DAC : GPIO 26 (DAC2 → SC8002B amp → P4 speaker header)
//                 GPIO 25 (DAC1, in use by touch SPI — avoid)
//  MIDI IN RX   : GPIO 35  (input-only pin — perfect for UART RX)
//  MIDI OUT TX  : GPIO 4   (shares RGB-LED blue trace; LED not driven — OK)
//
//  PCM5052 external I2S DAC pins (wired to CN1 header or breakout):
//    GPIO 22 → BCLK   (free, not used by display or touch)
//    GPIO 27 → LRCLK  (free)
//    GPIO 17 → DOUT   (shares RGB-LED red trace; LED not driven — OK)
//
//  Internal DAC mode (no external hardware):
//    GPIO 26 → SC8002B amp input → P4 speaker header (onboard)
//    Quality: 8-bit, mono.  Good for monitoring / built-in speaker.

#define I2S_NUM      I2S_NUM_0
#define I2S_BCK_PIN  22    // BCLK  → PCM5052 pin 3
#define I2S_WS_PIN   27    // LRCLK → PCM5052 pin 4
#define I2S_DATA_PIN 17    // DIN   → PCM5052 pin 5  (GPIO17 = RGB-LED red, not driven)
#define I2S_DAC_GPIO 26    // ESP32 internal DAC2 → SC8002B amp → speaker header

// Audio output mode (set via zombie_synth_mode.h UI, defined in ZombieSynth.ino)
enum AudioOutputMode {
  AUDIO_PCM5052      = 0,  // External 24-bit I2S DAC — best quality
  AUDIO_INTERNAL_DAC = 1,  // Onboard 8-bit DAC on GPIO26 — no extra hardware
  AUDIO_SPEAKER      = 2   // Same as INTERNAL_DAC, labelled for built-in speaker
};
extern AudioOutputMode audioOutputMode;

// Waveform types
enum WaveformType {
  WAVE_SAW,
  WAVE_SQUARE,
  WAVE_TRIANGLE,
  WAVE_SINE,
  WAVE_PULSE,
  WAVE_NOISE,     // White noise via LCG
  WAVE_SUPERSAW   // 3 detuned saws
};

const char* waveformNames[] = {"SAW","SQR","TRI","SIN","PUL","NOI","SUP"};

// Filter types
enum FilterType {
  FILTER_LOWPASS,
  FILTER_HIGHPASS,
  FILTER_BANDPASS,
  FILTER_NOTCH
};

// ADSR envelope state
enum EnvelopeState {
  ENV_IDLE,
  ENV_ATTACK,
  ENV_DECAY,
  ENV_SUSTAIN,
  ENV_RELEASE
};

// ADSR envelope
struct Envelope {
  float attack;      // seconds
  float decay;       // seconds
  float sustain;     // 0.0 - 1.0
  float release;     // seconds

  EnvelopeState state;
  float level;
  float attackRate;
  float decayRate;
  float releaseRate;

  void init(float a, float d, float s, float r) {
    attack = a;
    decay = d;
    sustain = s;
    release = r;
    state = ENV_IDLE;
    level = 0.0f;

    attackRate = (attack > 0.0f) ? (1.0f / (attack * SAMPLE_RATE)) : 1.0f;
    decayRate = (decay > 0.0f) ? ((1.0f - sustain) / (decay * SAMPLE_RATE)) : 1.0f;
    releaseRate = (release > 0.0f) ? (sustain / (release * SAMPLE_RATE)) : 1.0f;
  }

  void noteOn() {
    state = ENV_ATTACK;
  }

  void noteOff() {
    state = ENV_RELEASE;
  }

  float process() {
    switch (state) {
      case ENV_ATTACK:
        level += attackRate;
        if (level >= 1.0f) {
          level = 1.0f;
          state = ENV_DECAY;
        }
        break;

      case ENV_DECAY:
        level -= decayRate;
        if (level <= sustain) {
          level = sustain;
          state = ENV_SUSTAIN;
        }
        break;

      case ENV_SUSTAIN:
        level = sustain;
        break;

      case ENV_RELEASE:
        level -= releaseRate;
        if (level <= 0.0f) {
          level = 0.0f;
          state = ENV_IDLE;
        }
        break;

      case ENV_IDLE:
        level = 0.0f;
        break;
    }
    return level;
  }

  bool isActive() {
    return state != ENV_IDLE;
  }
};

// PolyBLEP function for bandlimited oscillators
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

// State variable filter (based on Chamberlin)
struct Filter {
  FilterType type;
  float cutoff;      // 0.0 - 1.0 (normalized frequency)
  float resonance;   // 0.0 - 1.0

  float lp, bp, hp;  // Filter states
  float f, q;        // Internal coefficients

  void init(FilterType t, float freq, float res) {
    type = t;
    cutoff = constrain(freq, 0.0f, 1.0f);
    resonance = constrain(res, 0.0f, 1.0f);
    lp = bp = hp = 0.0f;
    updateCoefficients();
  }

  void updateCoefficients() {
    // Linear f mapping avoids sin() on every UI/parameter change.
    // Chamberlin SVF: f = 2*sin(PI*fc/fs) ≈ 2*fc for small fc, here we use
    // the full range linearly.  Stability: enforce f < 1.9*q.
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

    // Clamp to prevent instability
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

// PolyBLEP oscillator
struct Oscillator {
  WaveformType waveform;
  float phase;
  float phase2;   // extra phase for supersaw voice 2
  float phase3;   // extra phase for supersaw voice 3
  float frequency;
  float pulseWidth;
  uint32_t noiseSeed;  // LCG state

  void init(WaveformType wave) {
    waveform = wave;
    phase = 0.0f;
    phase2 = 0.33f;
    phase3 = 0.66f;
    frequency = 440.0f;
    pulseWidth = 0.5f;
    noiseSeed = 12345;
  }

  void setFrequency(float freq) {
    frequency = freq;
  }

  void setPulseWidth(float pw) {
    pulseWidth = constrain(pw, 0.01f, 0.99f);
  }

  float process() {
    float dt = frequency / SAMPLE_RATE;
    float sample = 0.0f;

    switch (waveform) {
      case WAVE_SAW: {
        sample = 2.0f * phase - 1.0f;
        sample -= polyBlep(phase, dt);
        break;
      }

      case WAVE_SQUARE: {
        sample = (phase < 0.5f) ? 1.0f : -1.0f;
        sample += polyBlep(phase, dt);
        sample -= polyBlep(fmod(phase + 0.5f, 1.0f), dt);
        break;
      }

      case WAVE_PULSE: {
        sample = (phase < pulseWidth) ? 1.0f : -1.0f;
        sample += polyBlep(phase, dt);
        sample -= polyBlep(fmod(phase + (1.0f - pulseWidth), 1.0f), dt);
        break;
      }

      case WAVE_TRIANGLE: {
        float t = phase;
        sample = -1.0f + (2.0f * t);
        sample = 2.0f * (fabs(sample) - 0.5f);
        break;
      }

      case WAVE_SINE: {
        sample = sinf(TWO_PI * phase);
        break;
      }

      case WAVE_NOISE: {
        // LCG white noise — frequency param not used (full bandwidth)
        noiseSeed = noiseSeed * 1664525u + 1013904223u;
        sample = ((int32_t)noiseSeed) / 2147483648.0f;
        break;
      }

      case WAVE_SUPERSAW: {
        // 3 detuned saw waves mixed — slightly detuned around fundamental
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

// Synth voice with 2 oscillators (Prophet style)
struct Voice {
  int note;
  int velocity;
  bool active;
  unsigned long noteOnTime;

  // Per-voice modulation
  float pitchBendRatio;   // multiplier for frequency (1.0 = no bend)
  float aftertouchMod;    // 0-1, maps to filter cutoff boost
  float lfoFilterMod;     // from LFO → filter
  float lfoPitchMod;      // from LFO → pitch (semitones ratio)
  float lfoAmpMod;        // from LFO → amplitude

  // Patch parameters (set from preset/UI)
  float osc2DetuneRatio;  // e.g. 1.005 for slight detune
  float osc2SemiOffset;   // semitone offset (0 = unison, 12 = octave)
  float filterEnvAmt;     // 0-1 how much filter env modulates cutoff
  float baseCutoff;       // stored base cutoff so LFO can offset from it

  // Pre-baked oscillator base frequencies (recomputed in noteOn, not per-sample)
  float baseFreq1;        // 440 * 2^((note-69)/12)
  float baseFreq2;        // baseFreq1 * detuneRatio * 2^(semiOffset/12)

  Oscillator osc1;
  Oscillator osc2;
  Filter filter;
  Envelope ampEnv;
  Envelope filterEnv;

  void init() {
    note = -1;
    velocity = 0;
    active = false;
    noteOnTime = 0;
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
    note = n;
    velocity = vel;
    active = true;
    noteOnTime = millis();

    // Compute base frequencies once per note-on (not per sample)
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

  // Called per-sample from SynthEngine::processAudio()
  float process() {
    if (!active) return 0.0f;

    // Use pre-baked base frequencies — only fast multiplications per sample.
    // pitchBendRatio and lfoPitchMod are updated by their respective setters.
    float mod = pitchBendRatio * lfoPitchMod;
    osc1.setFrequency(baseFreq1 * mod);
    osc2.setFrequency(baseFreq2 * mod);

    float osc1Out = osc1.process();
    float osc2Out = osc2.process();
    float oscMix = (osc1Out + osc2Out) * 0.5f;

    // Filter modulation: modulate pre-computed f directly — no trig per sample.
    // filter.f was computed in setCutoff() (from UI/preset, not per-sample).
    // Envelope, LFO, and aftertouch add an offset to f in filter coefficient space.
    float filterEnvValue = filterEnv.process();
    float savedF = filter.f;
    float fMod = (filterEnvValue * filterEnvAmt + lfoFilterMod + aftertouchMod * 0.2f) * 2.0f;
    filter.f = constrain(filter.f + fMod, 0.0f, 1.9f * filter.q);
    float filtered = filter.process(oscMix);
    filter.f = savedF;  // restore so base cutoff isn't shifted by modulation

    float ampEnvValue = ampEnv.process();
    float ampScale = 1.0f - lfoAmpMod * 0.5f; // lfoAmpMod 0-1 → tremolo
    float output = filtered * ampEnvValue * ampScale * (velocity / 127.0f);

    if (!ampEnv.isActive()) active = false;

    return output;
  }
};

// Main synthesizer engine
class SynthEngine {
private:
  Voice voices[MAX_VOICES];
  int16_t audioBuffer[BUFFER_SIZE * 2]; // Stereo

  // Synth parameters (public-accessible via getters/setters)
  float masterVolume;
  float osc2Detune;      // ratio offset e.g. 0.005
  float osc2Semitones;   // semitone offset
  float osc1Level;
  float osc2Level;

  // Global modulation state (applied each audio tick by LFO/aftertouch routines)
  float globalLfoFilterMod;  // -1..+1 mapped to cutoff offset
  float globalLfoPitchMod;   // -1..+1 mapped to semitone ratio
  float globalLfoAmpMod;     // 0..1 tremolo depth
  float globalLfoPWMod;      // 0..1 pulse width offset
  float globalPitchBendRatio;// frequency multiplier (1.0=center)
  float globalAftertouch;    // 0-1

  // Find free voice or steal oldest
  int findVoice(int note) {
    for (int i = 0; i < MAX_VOICES; i++) {
      if (voices[i].active && voices[i].note == note) return i;
    }
    for (int i = 0; i < MAX_VOICES; i++) {
      if (!voices[i].active) return i;
    }
    int oldest = 0;
    unsigned long oldestTime = voices[0].noteOnTime;
    for (int i = 1; i < MAX_VOICES; i++) {
      if (voices[i].noteOnTime < oldestTime) { oldest = i; oldestTime = voices[i].noteOnTime; }
    }
    return oldest;
  }

  // Propagate patch-level params to all voices and rebake baseFreq2
  // so any in-flight voices immediately use the new detune/semitone values.
  void applyOsc2ToVoices() {
    float detuneRatio = 1.0f + osc2Detune;
    float semiMult    = powf(2.0f, osc2Semitones / 12.0f); // once, not per voice
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].osc2DetuneRatio = detuneRatio;
      voices[i].osc2SemiOffset  = osc2Semitones;
      // Rebake baseFreq2 so process() sees the change immediately
      if (voices[i].active) {
        voices[i].baseFreq2 = voices[i].baseFreq1 * detuneRatio * semiMult;
      }
    }
  }

public:
  ZombieEffects fx;  // chorus/delay/reverb chain
  volatile bool reinitInProgress = false;  // Set during output mode switch to prevent i2s_write race

  SynthEngine() {
    masterVolume       = 0.5f;
    osc1Level          = 0.5f;
    osc2Level          = 0.5f;
    osc2Detune         = 0.005f;
    osc2Semitones      = 0.0f;
    globalLfoFilterMod = 0.0f;
    globalLfoPitchMod  = 1.0f;
    globalLfoAmpMod    = 0.0f;
    globalLfoPWMod     = 0.0f;
    globalPitchBendRatio = 1.0f;
    globalAftertouch   = 0.0f;
  }

  void init() {
    for (int i = 0; i < MAX_VOICES; i++) voices[i].init();
    fx.init();
    reinitOutput();
  }

  // ── LFO routing (called from ZombieSynth.ino LFO tick) ───────────────────
  void setLFOFilterMod(float v) {
    globalLfoFilterMod = v;
    for (int i = 0; i < MAX_VOICES; i++) voices[i].lfoFilterMod = v * 0.3f;
  }
  void setLFOPitchMod(float v) {
    // v = -1..+1 depth in semitones (±2 semitones max)
    float ratio = powf(2.0f, v * 2.0f / 12.0f);
    globalLfoPitchMod = ratio;
    for (int i = 0; i < MAX_VOICES; i++) voices[i].lfoPitchMod = ratio;
  }
  void setLFOAmpMod(float v) {
    globalLfoAmpMod = v;
    for (int i = 0; i < MAX_VOICES; i++) voices[i].lfoAmpMod = fabsf(v);
  }
  void setLFOResonanceMod(float v) {
    // v = -1..+1 mapped to resonance offset
    float res = voices[0].filter.resonance + v * 0.3f;
    res = constrain(res, 0.0f, 0.95f);
    for (int i = 0; i < MAX_VOICES; i++) voices[i].filter.setResonance(res);
  }
  void setLFOPWMod(float v) {
    // v = -1..+1; modulates pulse width of osc1
    globalLfoPWMod = v;
    float pw = 0.5f + v * 0.3f;
    pw = constrain(pw, 0.05f, 0.95f);
    for (int i = 0; i < MAX_VOICES; i++) voices[i].osc1.setPulseWidth(pw);
  }
  void setLFODetuneMod(float v) {
    // v = -1..+1; modulates osc2 detune ratio
    float detune = osc2Detune + v * 0.01f;
    for (int i = 0; i < MAX_VOICES; i++) voices[i].osc2DetuneRatio = 1.0f + detune;
  }

  // ── Pitch bend (MIDI pitch bend, ±2 semitones default) ───────────────────
  void setPitchBend(int16_t bendVal) {
    // bendVal: -8192 to +8192
    float semitones = (bendVal / 8192.0f) * 2.0f;  // ±2 semitones
    globalPitchBendRatio = powf(2.0f, semitones / 12.0f);
    for (int i = 0; i < MAX_VOICES; i++) voices[i].pitchBendRatio = globalPitchBendRatio;
  }

  // ── Channel aftertouch → filter cutoff boost ──────────────────────────────
  void setChannelAftertouch(uint8_t val) {
    globalAftertouch = val / 127.0f;
    for (int i = 0; i < MAX_VOICES; i++) voices[i].aftertouchMod = globalAftertouch;
  }

  float getOsc2Detune() { return osc2Detune; }
  float getOsc2Semitones() { return osc2Semitones; }
  float getMasterVolume() { return masterVolume; }
  float getFilterCutoff() { return (MAX_VOICES > 0) ? voices[0].filter.cutoff : 0.5f; }
  float getFilterResonance() { return (MAX_VOICES > 0) ? voices[0].filter.resonance : 0.3f; }
  float getBaseCutoff() { return (MAX_VOICES > 0) ? voices[0].baseCutoff : 0.8f; }

  void setOsc2Detune(float d) {
    osc2Detune = d;
    applyOsc2ToVoices();
  }
  void setOsc2Semitones(float s) {
    osc2Semitones = s;
    applyOsc2ToVoices();
  }

  // Call after changing audioOutputMode to switch DAC route.
  // Uses reinitInProgress flag (not vTaskSuspend) to safely stop i2s_write:
  //   vTaskSuspend while the task is blocked on a DMA semaphore causes the audio
  //   task to hang permanently after i2s_driver_uninstall() deletes that semaphore.
  //   Instead we set the flag → processAudio() returns early → brief pause →
  //   then we uninstall, reconfigure, and clear the flag.
  void reinitOutput() {
    reinitInProgress = true;
    vTaskDelay(pdMS_TO_TICKS(25));  // Allow current processAudio() / i2s_write() to finish

    i2s_driver_uninstall(I2S_NUM);

    if (audioOutputMode == AUDIO_PCM5052) {
      // ── External PCM5052 I2S DAC (GPIO 22/27/17) ──────────────────────────
      i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = BUFFER_SIZE,
        .use_apll = true,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
      };
      i2s_driver_install(I2S_NUM, &cfg, 0, NULL);
      i2s_pin_config_t pins = {
        .bck_io_num   = I2S_BCK_PIN,
        .ws_io_num    = I2S_WS_PIN,
        .data_out_num = I2S_DATA_PIN,
        .data_in_num  = I2S_PIN_NO_CHANGE
      };
      i2s_set_pin(I2S_NUM, &pins);
      i2s_set_clk(I2S_NUM, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
      Serial.println("Audio: PCM5052 DAC (GPIO 22/27/17)");

    } else {
      // ── Internal DAC on GPIO26 (SC8002B amp → onboard speaker) ───────────
      // Same path for AUDIO_INTERNAL_DAC and AUDIO_SPEAKER
      i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = BUFFER_SIZE,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
      };
      i2s_driver_install(I2S_NUM, &cfg, 0, NULL);
      // Route DAC2 (right channel → GPIO26 → SC8002B amp)
      i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN);
      i2s_set_clk(I2S_NUM, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
      Serial.printf("Audio: Internal DAC on GPIO%d (SC8002B amp)\n", I2S_DAC_GPIO);
    }

    reinitInProgress = false;  // Audio task resumes automatically
  }

  void noteOn(int note, int velocity) {
    int voiceIdx = findVoice(note);
    voices[voiceIdx].noteOn(note, velocity);
  }

  void noteOff(int note) {
    for (int i = 0; i < MAX_VOICES; i++) {
      if (voices[i].active && voices[i].note == note) {
        voices[i].noteOff();
      }
    }
  }

  void allNotesOff() {
    for (int i = 0; i < MAX_VOICES; i++) {
      if (voices[i].active) {
        voices[i].noteOff();
      }
    }
  }

  // Update oscillator waveforms for all voices
  void setOsc1Waveform(WaveformType wave) {
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].osc1.waveform = wave;
    }
  }

  void setOsc2Waveform(WaveformType wave) {
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].osc2.waveform = wave;
    }
  }

  // Update filter for all voices (also stores baseCutoff for LFO/AT modulation)
  void setFilterCutoff(float cutoff) {
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].baseCutoff = cutoff;
      voices[i].filter.setCutoff(cutoff);
    }
  }

  void setFilterResonance(float resonance) {
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].filter.setResonance(resonance);
    }
  }

  void setFilterType(FilterType type) {
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].filter.type = type;
    }
  }

  // Update envelopes for all voices
  void setAmpEnvelope(float attack, float decay, float sustain, float release) {
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].ampEnv.init(attack, decay, sustain, release);
    }
  }

  void setFilterEnvelope(float attack, float decay, float sustain, float release) {
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].filterEnv.init(attack, decay, sustain, release);
    }
  }

  void setMasterVolume(float vol) {
    masterVolume = constrain(vol, 0.0f, 1.0f);
  }

  // Audio generation - called from Core 0 audio task
  void processAudio() {
    // Yield during output reinit so i2s_write() isn't called on a dying driver.
    // reinitInProgress is set by reinitOutput() on Core 1 before uninstalling.
    if (reinitInProgress) { vTaskDelay(1); return; }

    // Boost volume for internal DAC/speaker: SC8002B needs more headroom than PCM5052.
    float scale = (audioOutputMode == AUDIO_PCM5052) ? 0.3f : 0.65f;

    for (int i = 0; i < BUFFER_SIZE; i++) {
      float mixL = 0.0f;
      float mixR = 0.0f;

      // Mix all active voices
      for (int v = 0; v < MAX_VOICES; v++) {
        if (voices[v].active || voices[v].ampEnv.isActive()) {
          float sample = voices[v].process();
          mixL += sample;
          mixR += sample;
        }
      }

      // Master volume
      mixL *= masterVolume * scale;
      mixR *= masterVolume * scale;

      // FX chain (chorus → delay → reverb)
      float fxL, fxR;
      fx.process(mixL, mixR, fxL, fxR);

      if (audioOutputMode == AUDIO_PCM5052) {
        audioBuffer[i * 2]     = (int16_t)(constrain(fxL, -1.0f, 1.0f) * 32767.0f);
        audioBuffer[i * 2 + 1] = (int16_t)(constrain(fxR, -1.0f, 1.0f) * 32767.0f);
      } else {
        // Internal DAC: unsigned 8-bit in high byte; GPIO26 = right channel
        uint16_t dacVal = (uint16_t)((constrain(fxR, -1.0f, 1.0f) + 1.0f) * 127.5f);
        dacVal &= 0xFF;
        audioBuffer[i * 2]     = (int16_t)(dacVal << 8);
        audioBuffer[i * 2 + 1] = (int16_t)(dacVal << 8);
      }
    }

    size_t bytes_written;
    i2s_write(I2S_NUM, audioBuffer, sizeof(audioBuffer), &bytes_written, portMAX_DELAY);
  }

  int getActiveVoiceCount() {
    int count = 0;
    for (int i = 0; i < MAX_VOICES; i++) {
      if (voices[i].active) count++;
    }
    return count;
  }
};

#endif
