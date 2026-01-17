# CYD Custom Virtual Analogue Synthesizer

A comprehensive 6-voice polyphonic virtual analogue synthesizer for the ESP32-2432S028R (CYD - Cheap Yellow Display) board.

## Features

### Synthesis Engine
- **6-voice polyphony** with intelligent voice stealing
- **Dual PolyBLEP oscillators** per voice (band-limited, alias-free)
  - Waveforms: Sine, Sawtooth, Square, Triangle, Pulse, Noise
  - Oscillator 2 detune (-100 to +100 cents)
  - Independent level control for each oscillator
- **Sub oscillator** (one octave below OSC1, always sine wave)
- **State Variable Filter** (LP/HP/BP/Notch)
  - Cutoff: 20Hz - 10kHz
  - Resonance control
  - Envelope modulation
- **ADSR Envelopes** for amplitude and filter
- **LFO** with multiple destinations

### Arpeggiator
- **100 preset patterns** including:
  - Basic patterns (Up, Down, Up/Down, Alternating)
  - Rhythmic patterns (Syncopated, Breakbeat, Stutter)
  - Melodic patterns (Blues riff, Folk pattern, Country lick)
  - Electronic patterns (TB-303 style, Acid line, Trance gate)
- **Tempo:** 30-300 BPM
- **Gate length:** 10-100%
- **Swing:** 0-100%
- **Octave range:** 1-4 octaves
- **Modes:** Up, Down, Up/Down, Down/Up, Random, Pattern

### Hardware I/O
- **5-pin DIN MIDI input** via UART2 (standard 31.25kbaud)
- **I2S audio output** to PCM5102A DAC (44.1kHz, 16-bit stereo)
- **Touchscreen UI** with 6 pages of controls

### User Interface
- **Page 1:** Oscillator waveforms and levels
- **Page 2:** Filter controls (type, cutoff, resonance, envelope amount)
- **Page 3:** Envelope controls (ADSR)
- **Page 4:** Arpeggiator (patterns, tempo, gate, swing)
- **Page 5:** LFO modulation
- **Page 6:** Settings and MIDI panic

## Hardware Requirements

### Main Board
- ESP32-2432S028R (CYD) board
  - 240x320 ILI9341 TFT display
  - XPT2046 resistive touchscreen
  - ESP32 dual-core processor

### Additional Components

#### 1. PCM5102A DAC Module
Connect via I2S:
- **BCK (Bit Clock):** GPIO 26
- **LRCK (Word Select):** GPIO 22
- **DIN (Data):** GPIO 27
- **VCC:** 3.3V or 5V (depending on module)
- **GND:** GND
- **SCK:** Tie to GND (slave mode)
- **FMT:** Tie to GND (I2S format)
- **XMT:** Tie to 3.3V (unmute)

Audio output: Connect to amplifier or headphones via 3.5mm jack on DAC module.

#### 2. MIDI Input Circuit (5-pin DIN)

**Components needed:**
- 1x 5-pin DIN MIDI socket
- 1x 6N138 optocoupler (or similar)
- 1x 220Ω resistor (R1)
- 1x 220Ω resistor (R2)
- 1x 1kΩ resistor (R3)
- 1x 1N4148 diode (D1)

**Schematic:**
```
5-pin DIN MIDI Socket:
  Pin 2 (Not used)
  Pin 4 --[D1 cathode]--+--[R1 220Ω]--+
                        |              |
  Pin 5 ----------------+              |
                                       |
                                    6N138
                                   Pin 2 (Anode)
                                   Pin 3 (Cathode to GND)
                                   Pin 6 (+3.3V via R2 220Ω)
                                   Pin 5 --[R3 1kΩ]-- GPIO 16 (RX)
                                   Pin 8 (+3.3V)
                                   Pin 7 (GND)
```

**Pinout:**
- **MIDI RX:** GPIO 16
- **MIDI TX:** GPIO 17 (optional, for MIDI thru)

### Available GPIO Pins

The following pins are still available for expansion:
- GPIO 0, 1, 3, 4, 5, 18, 19, 23

**Note:** GPIO 2, 21 are used by display. GPIO 12, 13, 14, 15 are SPI for display. GPIO 25, 32, 33, 36, 39 are used by touch.

## Software Setup

### Arduino IDE Configuration

1. **Install ESP32 Board Support:**
   - Add to Board Manager URLs: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Install "ESP32 by Espressif Systems"

2. **Install Required Libraries:**
   - TFT_eSPI (by Bodmer)
   - XPT2046_Touchscreen (by Paul Stoffregen)

3. **Configure TFT_eSPI:**
   - The project includes a custom `User_Setup.h` already configured for CYD
   - If needed, copy `User_Setup.h` to your TFT_eSPI library folder

4. **Board Settings:**
   - Board: "ESP32 Dev Module"
   - Upload Speed: 921600
   - CPU Frequency: 240MHz (WiFi/BT)
   - Flash Frequency: 80MHz
   - Flash Mode: QIO
   - Flash Size: 4MB
   - Partition Scheme: "Default 4MB with spiffs"
   - PSRAM: Disabled

5. **Upload:**
   - Open `CYD_Custom_Synth.ino`
   - Select correct COM port
   - Click Upload

## MIDI Implementation

### Supported MIDI Messages

- **Note On/Off:** Full velocity sensitivity
- **Control Change:**
  - CC 1: LFO Amount
  - CC 7: Master Volume
  - CC 70: Sustain Level
  - CC 71: Filter Resonance
  - CC 72: Release Time
  - CC 73: Attack Time
  - CC 74: Filter Cutoff
  - CC 75: Decay Time
  - CC 123: All Notes Off
- **Program Change:** Reserved for future preset system
- **Pitch Bend:** Reserved for future implementation

### MIDI Channel
- Default: Omni (receives on all channels)
- Configurable in settings page

## Audio Specifications

- **Sample Rate:** 44.1 kHz
- **Bit Depth:** 16-bit
- **Channels:** Stereo (mono signal duplicated)
- **Buffer Size:** 128 samples
- **Latency:** ~3ms (buffer) + ~5ms (processing) = ~8ms total

## Performance

- **CPU Usage:**
  - Core 0: UI and MIDI (~10-20%)
  - Core 1: Audio synthesis (~60-80% with 6 voices)
- **Max Polyphony:** 6 voices simultaneously
- **UI Refresh Rate:** ~50 Hz

## Usage

### Basic Operation

1. **Power on** - Synth initializes and shows splash screen
2. **Navigate pages** - Touch left/right arrows to change pages
3. **Adjust parameters:**
   - Touch waveform icons to select oscillator types
   - Touch and drag knobs vertically to adjust values
   - Touch buttons to toggle or select options

### Playing the Synth

**Option 1: MIDI Keyboard**
- Connect MIDI keyboard to 5-pin DIN input
- Play notes normally
- Use mod wheel (CC1) for LFO amount
- Use expression controls (CC74) for filter cutoff

**Option 2: Arpeggiator**
- Go to Arpeggiator page
- Enable arpeggiator (ON button)
- Select pattern (0-99)
- Adjust tempo, gate, swing
- Play single notes or chords - they will be arpeggiated

### Sound Design Tips

**Warm Bass:**
- OSC1: Sine or Triangle
- OSC2: Sawtooth, detune +7 cents
- Sub: 50-70%
- Filter: Lowpass, cutoff ~500Hz, resonance low
- Envelope: Fast attack, medium release

**Bright Lead:**
- OSC1: Sawtooth
- OSC2: Square, detune +12 cents
- Sub: 0%
- Filter: Lowpass, cutoff ~3kHz, high resonance
- Filter Env: High amount, fast attack/decay
- Envelope: Medium attack, high sustain

**Pluck/Key:**
- OSC1: Square
- OSC2: Triangle, detune +5 cents
- Filter: Lowpass, medium cutoff
- Filter Env: Very high amount
- Envelope: Very fast attack, short decay, low sustain, short release

**Pad:**
- OSC1: Sawtooth
- OSC2: Sawtooth, detune +10 cents
- Sub: 30%
- Filter: Lowpass, medium cutoff
- Envelope: Slow attack, high sustain, long release
- LFO: Slow rate to filter cutoff

## File Structure

```
CYD_Custom_Synth/
├── CYD_Custom_Synth.ino      # Main application
├── synth_config.h             # Configuration and constants
├── polyblep_osc.h             # PolyBLEP oscillator implementation
├── adsr_envelope.h            # ADSR envelope generator
├── filter.h                   # State variable filter
├── synth_voice.h              # Single voice (osc+filter+env)
├── synth_engine.h             # Polyphonic engine (6 voices)
├── arpeggiator.h              # Arpeggiator with 100 patterns
├── audio_output.h             # I2S audio output handler
├── midi_input.h               # Hardware MIDI input handler
├── synth_ui.h                 # UI components and helpers
└── User_Setup.h               # TFT_eSPI display configuration
```

## Architecture

### Dual-Core Design
- **Core 0 (Protocol Core):**
  - UI rendering and touch handling (~50 Hz)
  - MIDI input processing
  - Parameter updates

- **Core 1 (Application Core):**
  - Audio synthesis (44.1 kHz)
  - Real-time DSP processing
  - High-priority task, minimal interruptions

### Signal Flow
```
MIDI Input → Voice Allocation → Oscillators → Filter → Envelope → Mixer → I2S Output
              ↑                     ↑           ↑
              Arpeggiator          LFO      Filter Env
```

## Troubleshooting

### No Audio Output
1. Check PCM5102A wiring (BCK, LRCK, DIN)
2. Verify PCM5102A power (3.3V or 5V)
3. Check I2S pins in `synth_config.h`
4. Ensure SCK is tied to GND on PCM5102A
5. Try headphones directly on DAC output

### No MIDI Input
1. Verify optocoupler wiring
2. Check GPIO 16 connection
3. Test with Serial Monitor (should show MIDI messages)
4. Verify MIDI cable is connected to MIDI Out of keyboard

### Touch Not Working
1. Verify touch is calibrated in `CYD_Custom_Synth.ino`
2. Check touch mapping (200-3700 for X, 240-3800 for Y)
3. Try adjusting mapping values if touch is offset

### Display Issues
1. Check `User_Setup.h` is properly configured
2. Verify SPI pins match CYD board
3. Try different upload speeds
4. Check GPIO 21 for backlight

### Crackling Audio
1. Reduce polyphony (use fewer simultaneous notes)
2. Increase buffer size in `synth_config.h`
3. Lower filter resonance
4. Disable WiFi/Bluetooth to reduce CPU load

## Future Enhancements

Potential additions:
- [ ] Preset save/load system (SPIFFS)
- [ ] Effects (reverb, delay, chorus)
- [ ] Additional LFO destinations
- [ ] Pitch bend support
- [ ] MIDI clock sync for arpeggiator
- [ ] Sequencer mode
- [ ] Unison mode (multiple voices per note)
- [ ] Portamento/glide
- [ ] Stereo panning per voice
- [ ] Keyboard mode on touchscreen

## Credits

- **Hardware:** ESP32-2432S028R (CYD) board
- **UI Framework:** TFT_eSPI by Bodmer
- **PolyBLEP Algorithm:** Based on "Minimum Phase PolyBLEPs" paper
- **Filter Design:** Chamberlin State Variable Filter topology

## License

This project is open source. Feel free to modify and improve!

---

**Enjoy making music with your CYD Custom Synth! 🎹🎵**
