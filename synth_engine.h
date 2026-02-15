#ifndef SYNTH_ENGINE_H
#define SYNTH_ENGINE_H

#include <Arduino.h>
#include <driver/i2s.h>

// Prophet-8 style synthesizer engine with polyBLEP oscillators
// Supports 8-voice polyphony with full ADSR, filter, and modulation

#define MAX_VOICES 8
#define SAMPLE_RATE 44100
#define BUFFER_SIZE 256
#define TWO_PI 6.28318530718f
#define MIDI_NOTE_COUNT 128

// Audio output configuration for PCM5052 DAC
// Note: If using onboard speaker (P4 connector), GPIO26 controls SC8002B amp
// For external I2S DAC, use these free GPIOs:
#define I2S_NUM I2S_NUM_0
#define I2S_BCK_PIN 22      // Bit clock (CN1 connector)
#define I2S_WS_PIN 27       // Word select/LRCLK (CN1 connector)
#define I2S_DATA_PIN 17     // Data out (requires RGB LED removal or can cause conflicts)

// Waveform types
enum WaveformType {
  WAVE_SAW,
  WAVE_SQUARE,
  WAVE_TRIANGLE,
  WAVE_SINE,
  WAVE_PULSE
};

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
    // Linear f mapping: monotonic, no transcendental function needed.
    // Chamberlin SVF stability requires f < 2*q.  We enforce that here with a
    // 5% margin so high resonance + high cutoff doesn't blow up the filter.
    q = 1.0f - resonance;
    q = constrain(q, 0.1f, 1.0f);
    float maxF = 1.9f * q;   // 0.95 × (2*q) safety margin
    f = constrain(cutoff * 2.0f, 0.0f, maxF);
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
  float frequency;
  float pulseWidth;

  void init(WaveformType wave) {
    waveform = wave;
    phase = 0.0f;
    frequency = 440.0f;
    pulseWidth = 0.5f;
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
        sample = sin(TWO_PI * phase);
        break;
      }
    }

    phase += dt;
    if (phase >= 1.0f) phase -= 1.0f;

    return sample;
  }
};

// Synth voice with 2 oscillators (Prophet style)
struct Voice {
  int note;
  int velocity;
  bool active;
  unsigned long noteOnTime;

  Oscillator osc1;
  Oscillator osc2;
  Filter filter;
  Envelope ampEnv;
  Envelope filterEnv;
  float filterEnvAmt;  // How much filterEnv opens the filter (0-1 range, mapped to f units)

  void init() {
    note = -1;
    velocity = 0;
    active = false;
    noteOnTime = 0;
    filterEnvAmt = 0.5f;

    osc1.init(WAVE_SAW);
    osc2.init(WAVE_SAW);
    filter.init(FILTER_LOWPASS, 0.5f, 0.3f);  // 0.5 = mid-range start (f=1.0)
    ampEnv.init(0.01f, 0.3f, 0.7f, 0.5f);
    filterEnv.init(0.01f, 0.3f, 0.5f, 0.3f);
  }

  void noteOn(int n, int vel) {
    note = n;
    velocity = vel;
    active = true;
    noteOnTime = millis();

    float freq = 440.0f * pow(2.0f, (n - 69) / 12.0f);
    osc1.setFrequency(freq);
    osc2.setFrequency(freq * 1.005f); // Slight detune for thickness
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

    // Generate oscillator signals
    float osc1Out = osc1.process();
    float osc2Out = osc2.process();
    float oscMix = (osc1Out + osc2Out) * 0.5f;

    // Apply filter envelope: modulate f directly (no per-sample sin/trig call).
    // filter.f is pre-computed from setCutoff(); envelope adds on top.
    float filterEnvValue = filterEnv.process();
    float savedF = filter.f;
    filter.f = constrain(filter.f + filterEnvValue * filterEnvAmt, 0.0f, 1.9f * filter.q);

    // Process through filter
    float filtered = filter.process(oscMix);
    filter.f = savedF;  // restore so UI-set cutoff isn't drifted by envelope

    // Apply amplitude envelope
    float ampEnvValue = ampEnv.process();
    float output = filtered * ampEnvValue * (velocity / 127.0f);

    // Deactivate voice if envelope finished
    if (!ampEnv.isActive()) {
      active = false;
    }

    return output;
  }
};

// Main synthesizer engine
class SynthEngine {
private:
  Voice voices[MAX_VOICES];
  int16_t audioBuffer[BUFFER_SIZE * 2]; // Stereo

  // Synth parameters
  float masterVolume;
  float osc1Level;
  float osc2Level;
  float osc2Detune;
  float osc2Semitones;

  // LFO
  float lfoRate;
  float lfoDepth;
  float lfoPhase;

  // Find free voice or steal oldest
  int findVoice(int note) {
    // First, try to find matching note
    for (int i = 0; i < MAX_VOICES; i++) {
      if (voices[i].active && voices[i].note == note) {
        return i;
      }
    }

    // Then try to find free voice
    for (int i = 0; i < MAX_VOICES; i++) {
      if (!voices[i].active) {
        return i;
      }
    }

    // Steal oldest voice
    int oldest = 0;
    unsigned long oldestTime = voices[0].noteOnTime;
    for (int i = 1; i < MAX_VOICES; i++) {
      if (voices[i].noteOnTime < oldestTime) {
        oldest = i;
        oldestTime = voices[i].noteOnTime;
      }
    }
    return oldest;
  }

public:
  SynthEngine() {
    masterVolume = 0.5f;
    osc1Level = 0.5f;
    osc2Level = 0.5f;
    osc2Detune = 0.005f;
    osc2Semitones = 0.0f;
    lfoRate = 5.0f;
    lfoDepth = 0.0f;
    lfoPhase = 0.0f;
  }

  void init() {
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].init();
    }

    // Configure I2S for PCM5052 DAC
    i2s_config_t i2s_config = {
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

    i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_BCK_PIN,
      .ws_io_num = I2S_WS_PIN,
      .data_out_num = I2S_DATA_PIN,
      .data_in_num = I2S_PIN_NO_CHANGE
    };

    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM, &pin_config);
    i2s_set_clk(I2S_NUM, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
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

  // Update filter for all voices
  void setFilterCutoff(float cutoff) {
    for (int i = 0; i < MAX_VOICES; i++) {
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

  void setFilterEnvAmount(float amt) {
    for (int i = 0; i < MAX_VOICES; i++) {
      voices[i].filterEnvAmt = constrain(amt, 0.0f, 1.9f);
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

  // Audio generation - call this frequently from core task
  void processAudio() {
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

      // Apply master volume and convert to 16-bit
      mixL *= masterVolume * 0.3f; // Scale down to prevent clipping
      mixR *= masterVolume * 0.3f;

      audioBuffer[i * 2] = (int16_t)(constrain(mixL, -1.0f, 1.0f) * 32767.0f);
      audioBuffer[i * 2 + 1] = (int16_t)(constrain(mixR, -1.0f, 1.0f) * 32767.0f);
    }

    // Send to I2S
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
