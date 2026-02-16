#ifndef CONFIG_H
#define CONFIG_H

// ═══════════════════════════════════════════════════════════════════════════════
// ZOMBIE SS PROPHET SYNTHESIZER – RP2040 Port Configuration
// ═══════════════════════════════════════════════════════════════════════════════
//
// Target: Raspberry Pi Pico / RP2040 (dual-core Cortex-M0+ @ 250MHz)
// Display: 1.3" SH1106 128×64 OLED (I2C)
// Input: 4×4 key matrix (16 buttons)
// Audio: PCM5102A I2S DAC
// MIDI: 5-pin DIN via UART
//
// Core 0: UI rendering, button input, MIDI processing
// Core 1: Audio synthesis, I2S output
// ═══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>

// ── System Clock ─────────────────────────────────────────────────────────────
#define RP2040_CLOCK_MHZ 250

// ── Audio Settings ───────────────────────────────────────────────────────────
#define SAMPLE_RATE     44100
#define BUFFER_SIZE     128
#define MAX_VOICES      6
#define TWO_PI_F        6.28318530718f
#define MIDI_NOTE_COUNT 128

// ── I2S Audio Pins (PCM5102A DAC) ────────────────────────────────────────────
#define I2S_DATA_PIN    20        // DIN  → PCM5102A
#define I2S_BCLK_PIN    21        // BCLK → PCM5102A
// LRCLK auto-assigned to BCLK+1 = GPIO 22

// ── MIDI Pins ────────────────────────────────────────────────────────────────
#define MIDI_RX_PIN     1         // MIDI IN  (UART0 RX / Serial1)
#define MIDI_TX_PIN     0         // MIDI OUT (UART0 TX / Serial1)
#define MIDI_BAUD       31250

// ── OLED Display (I2C) ──────────────────────────────────────────────────────
#define OLED_SDA_PIN    4
#define OLED_SCL_PIN    5
#define OLED_WIDTH      128
#define OLED_HEIGHT     64
#define OLED_ADDR       0x3C

// ── 4×4 Key Matrix ─────────────────────────────────────────────────────────
// Module pinout: R4 R3 R2 R1 | C4 C3 C2 C1 | SDA SCL VCC GND
//
// Scanning: Rows are driven LOW one at a time (outputs).
//           Columns are read (inputs with pull-up). LOW = pressed.
//
// Physical layout:
//   [R1C1] [R1C2] [R1C3] [R1C4]
//   [R2C1] [R2C2] [R2C3] [R2C4]
//   [R3C1] [R3C2] [R3C3] [R3C4]
//   [R4C1] [R4C2] [R4C3] [R4C4]
//
// Functional mapping:
//   [BACK ] [ UP  ] [PGUP ] [PLAY ]     Row 1
//   [LEFT ] [ OK  ] [RIGHT] [ FN  ]     Row 2
//   [DOWN ] [PGDN ] [ −   ] [ +   ]     Row 3
//   [ Q1  ] [ Q2  ] [ Q3  ] [ Q4  ]     Row 4 (context)
//
// Q1-Q4 change function based on mode:
//   MENU:    SYNTH / ARP / SEQ / PRE
//   SYNTH:   OSC / FLT / ENV / FX
//   SEQ:     T1 / T2 / T3 / T4
//   ARP:     (pattern group shortcuts)
//   CHORD:   ROOT / TYPE / OCT / HOLD
//   PRESETS: LOAD / SAVE / FACT / USER

#define MATRIX_ROWS     4
#define MATRIX_COLS     4
#define NUM_BUTTONS     16

// Row pins (active-LOW outputs during scan)
#define ROW1_PIN        6
#define ROW2_PIN        7
#define ROW3_PIN        8
#define ROW4_PIN        9

// Column pins (inputs with pull-up, read during scan)
#define COL1_PIN        10
#define COL2_PIN        11
#define COL3_PIN        12
#define COL4_PIN        13

#define BTN_DEBOUNCE_MS 30
#define BTN_REPEAT_MS   400
#define BTN_REPEAT_FAST 80

// ── Button indices (row * 4 + col) ──────────────────────────────────────────
// Row 1: Navigation + Transport
#define BTN_BACK    0   // R1C1 — Go back / exit mode
#define BTN_UP      1   // R1C2 — Navigate up / cursor up
#define BTN_PGUP    2   // R1C3 — Page up / previous page
#define BTN_PLAY    3   // R1C4 — Play/Stop (arp/seq)

// Row 2: Selection
#define BTN_LEFT    4   // R2C1 — Cycle option left
#define BTN_OK      5   // R2C2 — Select / Enter / Toggle
#define BTN_RIGHT   6   // R2C3 — Cycle option right
#define BTN_FN      7   // R2C4 — Function/Shift (hold for fine adjust)

// Row 3: Value adjustment
#define BTN_DOWN    8   // R3C1 — Navigate down / cursor down
#define BTN_PGDN    9   // R3C2 — Page down / next page
#define BTN_MINUS   10  // R3C3 — Decrease value
#define BTN_PLUS    11  // R3C4 — Increase value

// Row 4: Context-sensitive quick buttons
#define BTN_Q1      12  // R4C1 — Context button 1
#define BTN_Q2      13  // R4C2 — Context button 2
#define BTN_Q3      14  // R4C3 — Context button 3
#define BTN_Q4      15  // R4C4 — Context button 4

// ── Inter-core MIDI event queue ─────────────────────────────────────────────
#define MIDI_QUEUE_SIZE 64
#define MIDI_QUEUE_MASK (MIDI_QUEUE_SIZE - 1)

struct MidiEvent {
  uint8_t type;
  uint8_t data1;
  uint8_t data2;
  uint8_t data3;

  // Explicit volatile-qualified operators required because compiler-generated
  // copy/move operators don't handle volatile qualifiers (GCC -fpermissive error).
  MidiEvent() : type(0), data1(0), data2(0), data3(0) {}
  MidiEvent(uint8_t t, uint8_t d1, uint8_t d2, uint8_t d3)
    : type(t), data1(d1), data2(d2), data3(d3) {}

  // Assign from volatile source (used in mqPop: queue → local copy)
  MidiEvent& operator=(const volatile MidiEvent& o) {
    type  = o.type;
    data1 = o.data1;
    data2 = o.data2;
    data3 = o.data3;
    return *this;
  }
};

#define EVENT_NOTE_ON      1
#define EVENT_NOTE_OFF     2
#define EVENT_CC           3
#define EVENT_PITCH_BEND   4
#define EVENT_AFTERTOUCH   5
#define EVENT_ALL_OFF      6
#define EVENT_LOAD_PRESET  7

// ── Waveform types ──────────────────────────────────────────────────────────
enum WaveformType {
  WAVE_SAW, WAVE_SQUARE, WAVE_TRIANGLE, WAVE_SINE,
  WAVE_PULSE, WAVE_NOISE, WAVE_SUPERSAW
};
static const char* waveformNames[] = {"SAW","SQR","TRI","SIN","PUL","NOI","SUP"};

// ── Filter types ────────────────────────────────────────────────────────────
enum FilterType {
  FILTER_LOWPASS, FILTER_HIGHPASS, FILTER_BANDPASS, FILTER_NOTCH
};
static const char* filterTypeNames[] = {"LP","HP","BP","NOTCH"};

// ── LFO enums ───────────────────────────────────────────────────────────────
enum LFOWave   { LFO_SINE, LFO_TRIANGLE, LFO_SAW, LFO_SQUARE, LFO_RANDOM };
enum LFOTarget {
  LFO_TARGET_FILTER = 0, LFO_TARGET_PITCH = 1, LFO_TARGET_AMP = 2,
  LFO_TARGET_RESONANCE = 3, LFO_TARGET_PW = 4, LFO_TARGET_DETUNE = 5
};
static const char* lfoWaveNames[]   = {"SINE","TRI","SAW","SQR","S&H"};
static const char* lfoTargetNames[] = {"FLTR","PTCH","AMP","RESO","PW","DET"};

// ── App modes ───────────────────────────────────────────────────────────────
enum AppMode {
  MODE_MENU, MODE_SYNTH, MODE_ARP, MODE_SEQ, MODE_PRESETS, MODE_CHORD
};

// ── ADSR envelope state ─────────────────────────────────────────────────────
enum EnvelopeState {
  ENV_IDLE, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE
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
