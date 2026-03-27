#ifndef CONFIG_H
#define CONFIG_H

/*******************************************************************
 *  ZOMBI SS PROPHET SYNTHESIZER v3 — RP2040 Dual-Core Port
 *  Pin definitions, constants, and shared types
 *
 *  Hardware:
 *    MCU:     RP2040 (Raspberry Pi Pico / Pico W)
 *    Display: 1.3" OLED SH1106 128x64 I2C (EstarDyn module)
 *    Audio:   PCM5102 I2S DAC
 *    Input:   EC11 rotary encoder + back/confirm buttons
 *    MIDI:    5-pin DIN MIDI IN on UART
 *
 *  Dual-core allocation:
 *    Core 0 — UI, MIDI, encoder, menu system
 *    Core 1 — Audio synthesis, I2S output
 *******************************************************************/

#include <Arduino.h>

// ── I2S DAC (PCM5102) ─────────────────────────────────────────────────────────
#define I2S_BCK_PIN   20   // BCLK → PCM5102 BCK
#define I2S_WS_PIN    21   // LRCK → PCM5102 LCK  (must be BCK+1 for PIO I2S)
#define I2S_DATA_PIN  22   // DOUT → PCM5102 DIN

// ══════════════════════════════════════════════════════════════════════════════
// ■■■  OLED DISPLAY CONFIG — CHANGE THESE IF YOUR SCREEN IS BLANK!  ■■■
// ══════════════════════════════════════════════════════════════════════════════
//
// The built-in LED blinks 3x at startup to confirm code is running.
// Open Serial Monitor at 115200 baud to see I2C scan results.
//
// OLED_DRIVER — set for your display chip:
//   0 = SH1106  (most 1.3" OLEDs, e.g. EstarDyn blue/white)
//   1 = SSD1306 (most 0.96" OLEDs, and some 1.3" OLEDs)
//
// OLED_I2C_ADDR — try 0x3D if 0x3C doesn't work
//
#define OLED_DRIVER     0         // << Change to 1 for SSD1306 / 0.96" OLED
#define OLED_I2C_ADDR   0x3C      // << Change to 0x3D if still blank
// ══════════════════════════════════════════════════════════════════════════════

// ── Status LED ───────────────────────────────────────────────────────────────
#define STATUS_LED_PIN 25   // GPIO 25 = onboard LED on standard Pico

// ── OLED Pins (I2C0) ────────────────────────────────────────────────────────
#define OLED_SDA_PIN  4    // I2C0 SDA
#define OLED_SCL_PIN  5    // I2C0 SCL
#define OLED_WIDTH    128
#define OLED_HEIGHT   64

// ── Rotary Encoder (EC11) ─────────────────────────────────────────────────────
#define ENC_A_PIN     2    // CLK (phase A)
#define ENC_B_PIN     3    // DT  (phase B)
#define ENC_SW_PIN    6    // Push switch

// ── Navigation Buttons ────────────────────────────────────────────────────────
#define BTN_BACK_PIN    7
#define BTN_CONFIRM_PIN 8

// ── MIDI (UART0) ──────────────────────────────────────────────────────────────
#define MIDI_RX_PIN   1    // UART0 RX — 5-pin DIN MIDI IN
#define MIDI_TX_PIN   0    // UART0 TX — MIDI OUT (optional)
#define MIDI_BAUD     31250

// ── Audio Constants ───────────────────────────────────────────────────────────
#define SAMPLE_RATE   44100
#define BUFFER_SIZE   256
#define MAX_VOICES    8
#define TWO_PI_F      6.28318530718f
#define MIDI_NOTE_COUNT 128

// ── Application Modes ─────────────────────────────────────────────────────────
enum AppMode {
  MODE_MENU,
  MODE_SYNTH,
  MODE_ARP,
  MODE_SEQ,
  MODE_PRESETS,
  MODE_CHORD
};

// ── Waveform Types ────────────────────────────────────────────────────────────
enum WaveformType {
  WAVE_SAW,
  WAVE_SQUARE,
  WAVE_TRIANGLE,
  WAVE_SINE,
  WAVE_PULSE,
  WAVE_NOISE,
  WAVE_SUPERSAW
};
#define NUM_WAVEFORMS 7

static const char* waveformNames[] = {"SAW","SQR","TRI","SIN","PUL","NOI","SUP"};

// ── Filter Types ──────────────────────────────────────────────────────────────
enum FilterType {
  FILTER_LOWPASS,
  FILTER_HIGHPASS,
  FILTER_BANDPASS,
  FILTER_NOTCH
};
#define NUM_FILTER_TYPES 4

static const char* filterTypeNames[] = {"LP","HP","BP","NOTCH"};

// ── Envelope State ────────────────────────────────────────────────────────────
enum EnvelopeState {
  ENV_IDLE,
  ENV_ATTACK,
  ENV_DECAY,
  ENV_SUSTAIN,
  ENV_RELEASE
};

// ── Synth Parameter Snapshot (for UI ↔ engine sync) ──────────────────────────
struct SynthParams {
  int   osc1Wave;
  float osc1Level;
  int   osc2Wave;
  float osc2Level;
  float osc2Detune;      // UI 0-1 → 0..0.02 ratio
  float osc2Semitones;   // -12..+12
  int   filterType;
  float filterCutoff;
  float filterResonance;
  float filterEnvAmount;
  float ampAttack, ampDecay, ampSustain, ampRelease;
  float filterAttack, filterDecay, filterSustain, filterRelease;
  float masterVolume;
};

// ── Input Event Types ─────────────────────────────────────────────────────────
enum InputEvent {
  EVT_NONE,
  EVT_ENC_CW,         // Encoder clockwise
  EVT_ENC_CCW,        // Encoder counter-clockwise
  EVT_ENC_PRESS,      // Encoder push
  EVT_ENC_RELEASE,
  EVT_BACK_PRESS,
  EVT_BACK_RELEASE,
  EVT_CONFIRM_PRESS,
  EVT_CONFIRM_RELEASE
};

// ── MIDI Output Routing (for sequencer) ──────────────────────────────────────
#define SEQ_OUT_INTERNAL 0
#define SEQ_OUT_EXTERNAL 1
#define SEQ_OUT_BOTH     2
static const char* seqOutNames[] = {"INT","EXT","BOTH"};

#endif // CONFIG_H
