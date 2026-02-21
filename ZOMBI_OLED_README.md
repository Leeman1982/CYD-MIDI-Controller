# ZOMBI SS Synthesizer - OLED Edition

A compact hardware variant of the ZOMBI SS synthesizer optimized for a 128×64 monochrome OLED display with rotary encoder and button controls.

## Hardware Requirements

### Display
- **128×64 I2C OLED Display** with ST7567S COG controller
- 4-pin I2C interface (VCC, GND, SDA, SCL)

### Controls
- **Rotary Encoder** with integrated push button (3 pins: A, B, SW)
- **4 Momentary Switches**:
  - MODE (page/pattern selection)
  - PLAY (start/stop playback)
  - OCT- (octave down)
  - OCT+ (octave up)

### ESP32 Board
- Any ESP32 development board with:
  - I2S DAC capability (for audio output)
  - Available GPIO pins for encoder/buttons
  - I2C bus for OLED

### Audio Output
- **Option 1**: External I2S DAC (PCM5052 or similar)
- **Option 2**: ESP32 internal DAC (GPIO 25/26)
- **Option 3**: PWM audio output with simple RC filter

### MIDI Input (Optional)
- MIDI IN circuit on UART2 RX (GPIO 16)
- Standard MIDI optocoupler circuit

## Pin Connections

### I2C OLED Display (ST7567S)
```
OLED VCC  → ESP32 3.3V
OLED GND  → ESP32 GND
OLED SDA  → ESP32 GPIO 21 (I2C SDA)
OLED SCL  → ESP32 GPIO 22 (I2C SCL)
```

### Rotary Encoder
```
Encoder A   → ESP32 GPIO 34 (input only)
Encoder B   → ESP32 GPIO 35 (input only)
Encoder SW  → ESP32 GPIO 32
Encoder GND → ESP32 GND
```

### Control Buttons
```
Button MODE (SW1) → ESP32 GPIO 33 → GND
Button PLAY (SW2) → ESP32 GPIO 25 → GND
Button OCT- (SW3) → ESP32 GPIO 26 → GND
Button OCT+ (SW4) → ESP32 GPIO 27 → GND
```
*Note: Internal pull-ups are enabled in software*

### I2S Audio Output (External DAC)
```
I2S BCK  (Bit Clock)   → ESP32 GPIO 22
I2S LRCK (Left/Right)  → ESP32 GPIO 27
I2S DATA (Audio Data)  → ESP32 GPIO 17
DAC GND                → ESP32 GND
```

### Internal DAC Audio Output
```
Audio Out Left  → ESP32 GPIO 25 (DAC1)
Audio Out Right → ESP32 GPIO 26 (DAC2)
```
*Note: Requires RC filter (1kΩ + 10µF) for clean output*

### MIDI Input
```
MIDI IN 5 (pin 5) → Optocoupler → GPIO 16 (UART2 RX)
MIDI IN 2 (pin 2) → GND
```
*Standard MIDI optocoupler circuit recommended (6N138 or H11L1)*

## Software Requirements

### Arduino Libraries
Install these libraries via Arduino Library Manager:

1. **U8g2** by olikraus
   - For ST7567S OLED display support
   - Version 2.34.x or newer

2. **Wire** (built-in)
   - For I2C communication

### Arduino IDE Setup

1. Install ESP32 board support:
   - Add to Board Manager URLs: `https://dl.espressif.com/dl/package_esp32_index.json`
   - Install "ESP32 by Espressif Systems"

2. Select your ESP32 board (e.g., "ESP32 Dev Module")

3. Configure partition scheme:
   - Tools → Partition Scheme → "Default 4MB with spiffs"

4. Upload settings:
   - Upload Speed: 921600
   - Flash Frequency: 80MHz

## Features

### Synth Engine
- **8-voice polyphony** with voice stealing
- **PolyBLEP oscillators** for alias-free waveforms
  - Waveforms: Sawtooth, Square, Triangle, Sine, Pulse
  - Dual oscillators per voice with detune and semitone offset
- **State Variable Filter** (Chamberlin SVF)
  - Types: Lowpass, Highpass, Bandpass, Notch
  - Adjustable cutoff and resonance
  - Filter envelope modulation
- **ADSR Envelopes**
  - Amplitude envelope
  - Filter envelope with adjustable amount
- **Master Volume** control

### Modes

#### 1. SYNTH Mode
Navigate through 6 parameter pages:

- **PAGE 1: OSCILLATOR**
  - OSC1 Waveform (5 types)
  - OSC2 Waveform (5 types)
  - OSC2 Detune (0-2%)
  - OSC2 Semitones (-12 to +12)

- **PAGE 2: FILTER**
  - Cutoff (0-100%)
  - Resonance (0-100%)
  - Type (LP/HP/BP/Notch)
  - Envelope Amount (0-100%)

- **PAGE 3: AMP ENVELOPE**
  - Attack (1ms-2s)
  - Decay (1ms-2s)
  - Sustain (0-100%)
  - Release (1ms-3s)

- **PAGE 4: FILTER ENVELOPE**
  - Attack (1ms-2s)
  - Decay (1ms-2s)
  - Sustain (0-100%)
  - Release (1ms-3s)

- **PAGE 5: LFO**
  - Rate (0.1-20Hz)
  - Depth (0-100%)
  - Target (Filter/Pitch/Amp)
  - Waveform (Sin/Tri/Sqr/Saw)

- **PAGE 6: MASTER**
  - Volume (0-100%)
  - Voice Count (display only)
  - Octave (-2 to +2)
  - Fine Tune (±50 cents)

#### 2. ARPEGGIATOR Mode
- 50 built-in patterns
- Adjustable BPM (40-240)
- Play/stop control
- Pattern selection

#### 3. SEQUENCER Mode
- 16-step sequencer
- Visual step display
- Adjustable BPM (40-240)
- Play/stop control

#### 4. PRESETS Mode
- 8 factory presets
- Quick preset loading
- Preset browser

## Controls

### In MENU
- **Encoder Turn**: Navigate menu items
- **Encoder Press**: Select menu item

### In SYNTH Mode
- **Encoder Turn**: Adjust selected parameter value
- **MODE Button**: Switch to next parameter page
- **MODE + Encoder**: Change parameter selection within page
- **OCT- / OCT+**: Transpose octave down/up
- **Encoder Press**: Return to menu

### In ARPEGGIATOR Mode
- **Encoder Turn**: Adjust BPM
- **MODE Button**: Select next pattern
- **PLAY Button**: Start/stop arpeggiator
- **Encoder Press**: Return to menu

### In SEQUENCER Mode
- **Encoder Turn**: Adjust BPM
- **PLAY Button**: Start/stop sequencer
- **Encoder Press**: Return to menu

### In PRESET Mode
- **Encoder Turn**: Browse presets
- **Encoder Press**: Load selected preset
- **MODE Button**: Return to menu

## MIDI Implementation

### MIDI Input
- **Note On/Off**: Triggers synth voices with octave transpose
- **CC 74**: Filter Cutoff
- **CC 71**: Filter Resonance
- **CC 7**: Master Volume
- **Pitch Bend**: (reserved for future implementation)

### MIDI Channel
- Responds to all channels (omni mode)

## Audio Architecture

### Processing Pipeline
```
MIDI/UI Input → Synth Engine → I2S Output
     ↓              ↓
  Arpeggiator   8 Voices (polyBLEP)
  Sequencer         ↓
                  Filter
                    ↓
                 Envelope
                    ↓
                 Mix/Output
```

### Dual-Core Processing
- **Core 0**: Audio generation (high priority)
  - Runs `audioTask()` continuously
  - Generates 256 samples per buffer
  - Outputs via I2S at 44.1kHz

- **Core 1**: UI and control (normal priority)
  - Encoder/button reading
  - OLED display updates
  - MIDI input parsing
  - Arpeggiator/sequencer logic

## Memory Usage

- **Flash**: ~350KB (synth engine + UI + libraries)
- **SRAM**: ~60KB (audio buffers + voice states + UI)
- **PSRAM**: Not required

## Performance

- **Sample Rate**: 44,100 Hz
- **Bit Depth**: 16-bit
- **Latency**: <6ms (256-sample buffer)
- **CPU Usage**: ~40% (Core 0), ~15% (Core 1)
- **Polyphony**: 8 voices
- **UI Refresh**: 50 Hz

## Building the Project

1. Open `ZombiSynthOLED.ino` in Arduino IDE
2. Ensure all `.h` files are in the same directory:
   - `synth_engine.h`
   - `midi_input.h`
   - `arpeggiator_patterns.h`
   - `zombie_step_sequencer.h`
   - `zombie_encoder.h`
   - `zombie_oled_ui.h`

3. Install required libraries (U8g2)
4. Select ESP32 board
5. Connect ESP32 via USB
6. Click Upload

## Troubleshooting

### Display Issues
- **Blank screen**: Check I2C address (try 0x3F or 0x3C)
- **Garbled display**: Verify ST7567S controller type
- **No response**: Check SDA/SCL connections and pull-ups

### Encoder Issues
- **Jittery readings**: Increase `debounceDelay` in `zombie_encoder.h`
- **Wrong direction**: Swap A and B pin definitions
- **No response**: Check pull-up resistors (internal enabled by default)

### Audio Issues
- **No sound**: Verify I2S pin connections
- **Distorted audio**: Lower master volume, check for clipping
- **Crackling**: Increase audio task priority or buffer size

### MIDI Issues
- **No MIDI input**: Check optocoupler circuit and baud rate (31250)
- **Wrong notes**: Verify octave transpose setting

## Pin Customization

To use different GPIO pins, edit these definitions in the header files:

**In `zombie_encoder.h`**:
```cpp
#define ENC_A_PIN 34
#define ENC_B_PIN 35
#define ENC_SW_PIN 32
#define BTN_MODE_PIN 33
#define BTN_PLAY_PIN 25
#define BTN_OCT_DOWN_PIN 26
#define BTN_OCT_UP_PIN 27
```

**In `synth_engine.h`** (for I2S pins):
```cpp
#define I2S_BCK_PIN 22
#define I2S_WS_PIN 27
#define I2S_DATA_PIN 17
```

## Future Enhancements

Potential additions for this variant:
- [ ] CV/Gate output for modular synth integration
- [ ] MIDI OUT for sequencer
- [ ] User preset saving to SPIFFS/EEPROM
- [ ] Step sequencer note editing
- [ ] LFO sync to BPM
- [ ] Effects (delay, reverb, chorus)
- [ ] Multi-mode filter (LP24, etc.)

## License

This project is part of the ZOMBI SS synthesizer family. See main project for license details.

## Credits

- Synth Engine: Prophet-8 inspired polyBLEP implementation
- OLED Library: U8g2 by olikraus
- Hardware Platform: ESP32 by Espressif Systems
