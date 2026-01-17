#ifndef SYNTH_CONFIG_H
#define SYNTH_CONFIG_H

// ============================================================================
// CUSTOM VIRTUAL ANALOGUE SYNTHESIZER - Hardware Configuration
// ============================================================================
// 6-voice polyphonic synthesizer with dual oscillators + sub
// PolyBLEP oscillators, hardware MIDI input, PCM5102A DAC output
// Comprehensive UI for the CYD (ESP32-2432S028R) platform
// ============================================================================

// Audio Configuration
#define SAMPLE_RATE 44100
#define BUFFER_SIZE 128
#define MAX_VOICES 6

// I2S Configuration for PCM5102A DAC
#define I2S_BCK_PIN 26        // Bit clock
#define I2S_LRCK_PIN 22       // Left/Right clock (Word select)
#define I2S_DATA_PIN 27       // Data out
#define I2S_PORT I2S_NUM_0

// Hardware MIDI Input (5-pin DIN via UART2)
#define MIDI_RX_PIN 16
#define MIDI_TX_PIN 17
#define MIDI_SERIAL Serial2
#define MIDI_BAUD_RATE 31250

// Display Configuration (from existing setup)
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Synthesizer Parameters
#define MIDI_CHANNEL 0        // 0 = omni, 1-16 = specific channel

// Oscillator types
enum OscillatorType {
  OSC_SINE = 0,
  OSC_SAW,
  OSC_SQUARE,
  OSC_TRIANGLE,
  OSC_PULSE,
  OSC_NOISE
};

// Filter types
enum FilterType {
  FILTER_LOWPASS = 0,
  FILTER_HIGHPASS,
  FILTER_BANDPASS,
  FILTER_NOTCH
};

// Arpeggiator modes
enum ArpMode {
  ARP_UP = 0,
  ARP_DOWN,
  ARP_UP_DOWN,
  ARP_DOWN_UP,
  ARP_RANDOM,
  ARP_PATTERN
};

// LFO destinations
enum LFODestination {
  LFO_DEST_OSC1_PITCH = 0,
  LFO_DEST_OSC2_PITCH,
  LFO_DEST_FILTER_CUTOFF,
  LFO_DEST_AMPLITUDE,
  LFO_DEST_PWM,
  LFO_DEST_PAN
};

// Color theme (matching existing project aesthetic)
#define COLOR_BG         0x0841
#define COLOR_SURFACE    0x2945
#define COLOR_PRIMARY    0x06FF
#define COLOR_SECONDARY  0xFD20
#define COLOR_ACCENT     0x07FF
#define COLOR_SUCCESS    0x07E0
#define COLOR_WARNING    0xFFE0
#define COLOR_ERROR      0xF800
#define COLOR_TEXT       0xFFFF
#define COLOR_TEXT_DIM   0x8410
#define COLOR_KNOB       0x4A69
#define COLOR_INDICATOR  0xF81F

// UI Pages
enum UIPage {
  PAGE_OSC = 0,
  PAGE_FILTER,
  PAGE_ENV,
  PAGE_ARP,
  PAGE_LFO,
  PAGE_SETTINGS
};

#endif // SYNTH_CONFIG_H
