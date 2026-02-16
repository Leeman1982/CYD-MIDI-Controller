#ifndef CONFIG_H
#define CONFIG_H

// ═══════════════════════════════════════════════════════════════════════════════
// ZOMBIE SS PROPHET SYNTHESIZER – RP2040 Port Configuration
// ═══════════════════════════════════════════════════════════════════════════════
//
// Target: Raspberry Pi Pico / RP2040 (dual-core Cortex-M0+ @ 250MHz)
// Display: 1.3" SH1106 128×64 OLED (I2C)
// Input: 8-button matrix/direct GPIO
// Audio: PCM5102A I2S DAC
// MIDI: 5-pin DIN via UART
//
// Core 0: UI rendering, touch/button input, MIDI processing
// Core 1: Audio synthesis, I2S output
// ═══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>

// ── System Clock ─────────────────────────────────────────────────────────────
// Overclock to 250 MHz for adequate software-float DSP performance.
// The RP2040 M0+ has no FPU — all float ops are emulated.
#define RP2040_CLOCK_MHZ 250

// ── Audio Settings ───────────────────────────────────────────────────────────
#define SAMPLE_RATE     44100
#define BUFFER_SIZE     128       // Samples per I2S write (smaller = lower latency)
#define MAX_VOICES      6         // Reduced from 8 (no FPU on M0+)
#define TWO_PI_F        6.28318530718f
#define MIDI_NOTE_COUNT 128

// ── I2S Audio Pins (PCM5102A DAC) ────────────────────────────────────────────
#define I2S_DATA_PIN    20        // DIN  → PCM5102A
#define I2S_BCLK_PIN    21        // BCLK → PCM5102A
// LRCLK is auto-assigned to BCLK+1 = GPIO 22 by arduino-pico I2S library

// ── MIDI Pins ────────────────────────────────────────────────────────────────
#define MIDI_RX_PIN     1         // MIDI IN  (UART0 RX / Serial1)
#define MIDI_TX_PIN     0         // MIDI OUT (UART0 TX / Serial1)
#define MIDI_BAUD       31250

// ── OLED Display (I2C) ──────────────────────────────────────────────────────
#define OLED_SDA_PIN    4
#define OLED_SCL_PIN    5
#define OLED_WIDTH      128
#define OLED_HEIGHT     64
#define OLED_ADDR       0x3C      // Common I2C address for SH1106/SSD1306

// ── Button Pins (Active LOW with internal pull-up) ──────────────────────────
// Directly wired buttons or button matrix output to GPIO.
// Active LOW: pressed = LOW, released = HIGH (INPUT_PULLUP)
//
// Layout assumes a module with joystick + 3 function keys:
//   Joystick: UP / DOWN / LEFT / RIGHT / CENTER(press)
//   Keys:     A (BACK) / B (FUNCTION) / C (PLAY/STOP)
//
// Change these to match your specific OLED button matrix module.
#define BTN_UP_PIN      6
#define BTN_DOWN_PIN    7
#define BTN_LEFT_PIN    8
#define BTN_RIGHT_PIN   9
#define BTN_CENTER_PIN  10        // SELECT / ENTER
#define BTN_A_PIN       11        // BACK / EXIT
#define BTN_B_PIN       12        // FUNCTION / SHIFT
#define BTN_C_PIN       13        // PLAY / STOP

#define NUM_BUTTONS     8
#define BTN_DEBOUNCE_MS 30        // Debounce time (ms)
#define BTN_REPEAT_MS   400       // Initial key repeat delay
#define BTN_REPEAT_FAST 80        // Fast repeat interval

// ── Button indices ──────────────────────────────────────────────────────────
#define BTN_UP      0
#define BTN_DOWN    1
#define BTN_LEFT    2
#define BTN_RIGHT   3
#define BTN_CENTER  4
#define BTN_A       5
#define BTN_B       6
#define BTN_C       7

// ── Inter-core MIDI event queue ─────────────────────────────────────────────
// Single-producer (Core 0) / single-consumer (Core 1) ring buffer.
// No locking needed with power-of-2 size and separate head/tail.
#define MIDI_QUEUE_SIZE 64        // Must be power of 2
#define MIDI_QUEUE_MASK (MIDI_QUEUE_SIZE - 1)

struct MidiEvent {
  uint8_t type;   // EVENT_NOTE_ON, EVENT_NOTE_OFF, EVENT_CC, etc.
  uint8_t data1;  // note / CC number
  uint8_t data2;  // velocity / CC value
  uint8_t data3;  // extra (channel, etc.)
};

// Event types for inter-core queue
#define EVENT_NOTE_ON      1
#define EVENT_NOTE_OFF     2
#define EVENT_CC           3
#define EVENT_PITCH_BEND   4
#define EVENT_AFTERTOUCH   5
#define EVENT_ALL_OFF      6
#define EVENT_LOAD_PRESET  7

// ── Waveform types ──────────────────────────────────────────────────────────
enum WaveformType {
  WAVE_SAW,
  WAVE_SQUARE,
  WAVE_TRIANGLE,
  WAVE_SINE,
  WAVE_PULSE,
  WAVE_NOISE,
  WAVE_SUPERSAW
};

static const char* waveformNames[] = {"SAW","SQR","TRI","SIN","PUL","NOI","SUP"};

// ── Filter types ────────────────────────────────────────────────────────────
enum FilterType {
  FILTER_LOWPASS,
  FILTER_HIGHPASS,
  FILTER_BANDPASS,
  FILTER_NOTCH
};

static const char* filterTypeNames[] = {"LP","HP","BP","NOTCH"};

// ── LFO enums ───────────────────────────────────────────────────────────────
enum LFOWave   { LFO_SINE, LFO_TRIANGLE, LFO_SAW, LFO_SQUARE, LFO_RANDOM };
enum LFOTarget {
  LFO_TARGET_FILTER    = 0,
  LFO_TARGET_PITCH     = 1,
  LFO_TARGET_AMP       = 2,
  LFO_TARGET_RESONANCE = 3,
  LFO_TARGET_PW        = 4,
  LFO_TARGET_DETUNE    = 5
};

static const char* lfoWaveNames[]   = {"SINE","TRI","SAW","SQR","S&H"};
static const char* lfoTargetNames[] = {"FLTR","PTCH","AMP","RESO","PW","DET"};

// ── App modes ───────────────────────────────────────────────────────────────
enum AppMode {
  MODE_MENU,
  MODE_SYNTH,
  MODE_ARP,
  MODE_SEQ,
  MODE_PRESETS,
  MODE_CHORD
};

// ── ADSR envelope state ─────────────────────────────────────────────────────
enum EnvelopeState {
  ENV_IDLE,
  ENV_ATTACK,
  ENV_DECAY,
  ENV_SUSTAIN,
  ENV_RELEASE
};

// ── Note names ──────────────────────────────────────────────────────────────
static const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

inline const char* midiNoteName(int note) {
  if (note < 0 || note > 127) return "---";
  return noteNames[note % 12];
}

inline int midiNoteOctave(int note) {
  return (note / 12) - 1;
}

#endif
