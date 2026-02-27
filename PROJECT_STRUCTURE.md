# Project Structure & File Organization

## 📁 Repository Layout

```
CYD-MIDI-Controller/
│
├── 📱 CYD TOUCHSCREEN VARIANTS (2.4" TFT)
│   ├── CYD-MIDI-Controller.ino       ⭐ Main MIDI controller (10 modes)
│   ├── ZombieSynth.ino                🎹 ZOMBI SS synthesizer
│   └── CYD_Touch_Diagnostic.ino       🔧 Touch testing tool
│
├── 📟 OLED VARIANTS (128×64 Display)
│   ├── ZombiSynthOLED.ino             🎹 OLED synth (encoder only)
│   └── ZombiSynthOLED_IOExpander.ino  🎛️ OLED synth (pots + encoder)
│
├── 🎼 MIDI MODE MODULES (for CYD-MIDI-Controller)
│   ├── keyboard_mode.h                Piano keyboard layout
│   ├── sequencer_mode.h               Step sequencer
│   ├── arpeggiator_mode.h             Arpeggiator patterns
│   ├── xy_pad_mode.h                  2D XY pad controller
│   ├── grid_piano_mode.h              Grid-based piano
│   ├── auto_chord_mode.h              Auto chord generator
│   ├── bouncing_ball_mode.h           Bouncing ball physics MIDI
│   ├── physics_drop_mode.h            Gravity drop physics
│   ├── random_generator_mode.h        Random MIDI generator
│   └── lfo_mode.h                     LFO MIDI controller
│
├── 🎹 SYNTH ENGINE MODULES (for ZOMBI variants)
│   ├── synth_engine.h                 ⚙️ Core synthesis engine
│   │   ├── PolyBLEP oscillators
│   │   ├── State Variable Filter (Chamberlin)
│   │   ├── ADSR envelopes
│   │   └── 8-voice polyphony
│   │
│   ├── zombie_synth_mode.h            ZOMBI synth UI (touchscreen)
│   ├── zombie_arp_mode.h              Arpeggiator mode UI
│   ├── zombie_seq_mode.h              Sequencer mode UI
│   ├── zombie_step_sequencer.h        16-step sequencer logic
│   ├── zombie_oled_ui.h               OLED display rendering
│   ├── zombie_encoder.h               Rotary encoder input
│   └── zombie_io_expander.h           PCF8574 + analog pots
│
├── 📡 MIDI & UTILITY MODULES
│   ├── midi_input.h                   MIDI input handling (UART)
│   ├── midi_utils.h                   BLE MIDI utilities
│   ├── arpeggiator_patterns.h         50 arpeggiator patterns
│   └── common_definitions.h           Shared constants & enums
│
├── 🎨 UI MODULES
│   └── ui_elements.h                  Common UI elements (buttons, sliders)
│
├── ⚙️ CONFIGURATION FILES
│   └── User_Setup.h                   TFT_eSPI display configuration
│
└── 📚 DOCUMENTATION
    ├── QUICK_START_GUIDE.md           🚀 Start here!
    ├── PROJECT_STRUCTURE.md           📁 This file
    ├── TOUCHSCREEN_CONFIG_GUIDE.md    🔧 Touch troubleshooting
    ├── ZOMBI_OLED_README.md           📟 OLED variant guide
    └── WIRING_GUIDE_IO_EXPANDER.md    🎛️ I/O expander wiring
```

---

## 🎯 Which Files Do I Need?

### Scenario 1: "I have a 2.4" CYD and want MIDI controller"

**Upload:**
- ✅ `CYD-MIDI-Controller.ino`

**Used Automatically:**
- `keyboard_mode.h`
- `sequencer_mode.h`
- `arpeggiator_mode.h`
- `xy_pad_mode.h`
- `grid_piano_mode.h`
- `auto_chord_mode.h`
- `bouncing_ball_mode.h`
- `physics_drop_mode.h`
- `random_generator_mode.h`
- `lfo_mode.h`
- `midi_utils.h`
- `ui_elements.h`
- `common_definitions.h`

**Configure:**
- Copy `User_Setup.h` to TFT_eSPI library

**Libraries Needed:**
- TFT_eSPI
- XPT2046_Touchscreen

---

### Scenario 2: "I have a 2.4" CYD and want synthesizer"

**Upload:**
- ✅ `ZombieSynth.ino`

**Used Automatically:**
- `synth_engine.h`
- `zombie_synth_mode.h`
- `zombie_arp_mode.h`
- `zombie_seq_mode.h`
- `zombie_step_sequencer.h`
- `midi_input.h`
- `arpeggiator_patterns.h`
- `ui_elements.h`
- `common_definitions.h`

**Configure:**
- Copy `User_Setup.h` to TFT_eSPI library

**Libraries Needed:**
- TFT_eSPI
- XPT2046_Touchscreen

---

### Scenario 3: "I have OLED + encoder (no pots)"

**Upload:**
- ✅ `ZombiSynthOLED.ino`

**Used Automatically:**
- `synth_engine.h`
- `zombie_encoder.h`
- `zombie_oled_ui.h`
- `zombie_step_sequencer.h`
- `midi_input.h`
- `arpeggiator_patterns.h`

**Wiring Guide:**
- Read `ZOMBI_OLED_README.md`

**Libraries Needed:**
- U8g2
- Wire

---

### Scenario 4: "I have OLED + encoder + 6 pots + PCF8574"

**Upload:**
- ✅ `ZombiSynthOLED_IOExpander.ino`

**Used Automatically:**
- `synth_engine.h`
- `zombie_encoder.h`
- `zombie_oled_ui.h`
- `zombie_io_expander.h`
- `zombie_step_sequencer.h`
- `midi_input.h`
- `arpeggiator_patterns.h`

**Wiring Guide:**
- Read `WIRING_GUIDE_IO_EXPANDER.md`

**Libraries Needed:**
- U8g2
- Wire

---

### Scenario 5: "Touch not working on CYD"

**Upload:**
- ✅ `CYD_Touch_Diagnostic.ino`

**Then Read:**
- `TOUCHSCREEN_CONFIG_GUIDE.md`

**Libraries Needed:**
- TFT_eSPI
- XPT2046_Touchscreen

---

## 🔄 Dependency Graph

### CYD-MIDI-Controller.ino Dependencies:
```
CYD-MIDI-Controller.ino
├── TFT_eSPI library
├── XPT2046_Touchscreen library
├── BLE libraries (built-in)
├── keyboard_mode.h
├── sequencer_mode.h
├── arpeggiator_mode.h
├── xy_pad_mode.h
├── grid_piano_mode.h
├── auto_chord_mode.h
├── bouncing_ball_mode.h
├── physics_drop_mode.h
├── random_generator_mode.h
├── lfo_mode.h
├── midi_utils.h
├── ui_elements.h
└── common_definitions.h
```

### ZombieSynth.ino Dependencies:
```
ZombieSynth.ino
├── TFT_eSPI library
├── XPT2046_Touchscreen library
├── synth_engine.h
│   └── I2S driver (built-in)
├── zombie_synth_mode.h
├── zombie_arp_mode.h
├── zombie_seq_mode.h
├── zombie_step_sequencer.h
├── midi_input.h
├── arpeggiator_patterns.h
├── ui_elements.h
└── common_definitions.h
```

### ZombiSynthOLED.ino Dependencies:
```
ZombiSynthOLED.ino
├── U8g2 library
├── Wire library (built-in)
├── synth_engine.h
│   └── I2S driver (built-in)
├── zombie_encoder.h
├── zombie_oled_ui.h
├── zombie_step_sequencer.h
├── midi_input.h
└── arpeggiator_patterns.h
```

### ZombiSynthOLED_IOExpander.ino Dependencies:
```
ZombiSynthOLED_IOExpander.ino
├── U8g2 library
├── Wire library (built-in)
├── synth_engine.h
│   └── I2S driver (built-in)
├── zombie_io_expander.h
│   ├── PCF8574 I2C I/O
│   └── ESP32 ADC
├── zombie_oled_ui.h
├── zombie_step_sequencer.h
├── midi_input.h
└── arpeggiator_patterns.h
```

---

## 📝 File Type Legend

| Extension | Purpose | Example |
|-----------|---------|---------|
| `.ino` | Main Arduino sketch | `ZombieSynth.ino` |
| `.h` | Header (module/library) | `synth_engine.h` |
| `.md` | Markdown documentation | `QUICK_START_GUIDE.md` |

---

## 🔧 Core Modules Explained

### synth_engine.h
The heart of the ZOMBI SS synthesizer. Contains:
- **Voice Management**: 8-voice polyphony with voice stealing
- **Oscillators**: PolyBLEP anti-aliased waveforms (Saw, Square, Triangle, Sine, Pulse)
- **Filter**: State Variable Filter (Chamberlin SVF) with LP/HP/BP/Notch
- **Envelopes**: ADSR for amplitude and filter modulation
- **Audio Pipeline**: I2S output at 44.1kHz, 16-bit stereo

**Used by:**
- `ZombieSynth.ino`
- `ZombiSynthOLED.ino`
- `ZombiSynthOLED_IOExpander.ino`

### zombie_oled_ui.h
UI rendering for 128×64 monochrome OLED displays. Provides:
- Menu system
- 6 parameter pages (OSC, FILTER, AMP ENV, FILT ENV, LFO, MASTER)
- Arpeggiator UI
- Sequencer step display
- Preset browser

**Uses:** U8g2 library for ST7567S OLED controller

### zombie_io_expander.h
Hardware I/O management for OLED variants with physical controls:
- **PCF8574Expander**: 8-bit I2C I/O expander for buttons
- **AnalogPots**: 6-channel ADC manager with smoothing
- **RotaryEncoderDirect**: Direct GPIO encoder reading

**Hardware:** PCF8574 module + 6× 10kΩ pots + rotary encoder

### midi_input.h
MIDI input handling for hardware MIDI (5-pin DIN). Supports:
- UART2 (GPIO 16) with optocoupler circuit
- Note On/Off
- Control Change (CC)
- Pitch Bend
- All MIDI channels

**Used by all ZOMBI synth variants**

### ui_elements.h
Common touchscreen UI elements:
- Touch state management
- Button drawing and hit detection
- Slider controls
- Status indicators
- Color themes

**Used by CYD touchscreen variants**

---

## 🎨 Display Variants

### 240×320 TFT (ILI9341)
- **Files:** `CYD-MIDI-Controller.ino`, `ZombieSynth.ino`
- **Library:** TFT_eSPI
- **Input:** Touchscreen (XPT2046)
- **Colors:** 65K colors (RGB565)
- **Pros:** Visual interface, touch input, large display
- **Cons:** More expensive, requires CYD board

### 128×64 OLED (ST7567S)
- **Files:** `ZombiSynthOLED.ino`, `ZombiSynthOLED_IOExpander.ino`
- **Library:** U8g2
- **Input:** Rotary encoder + buttons (+ pots optional)
- **Colors:** Monochrome (white/blue on black)
- **Pros:** Cheap, low power, compact, standalone builds
- **Cons:** Smaller display, less visual feedback

---

## 🔌 Hardware Interface Comparison

| Feature | CYD Touchscreen | OLED Basic | OLED + I/O Expander |
|---------|----------------|------------|---------------------|
| Display | 240×320 TFT | 128×64 OLED | 128×64 OLED |
| Input | Touchscreen | 4 buttons + encoder | 7 buttons + 6 pots + encoder |
| ESP32 Board | ESP32-2432S024 | Any ESP32 | Any ESP32 |
| I2C Devices | None | OLED only | OLED + PCF8574 |
| Analog Inputs | None | None | 6× ADC channels |
| Visual Feedback | High | Low | Low |
| Tactile Control | None | Medium | High |
| Cost | $$$ | $ | $$ |
| Enclosure | Integrated | DIY friendly | DIY synth box |

---

## 💾 Memory Usage

### CYD-MIDI-Controller.ino
- Flash: ~380KB
- RAM: ~45KB
- Core 0: BLE MIDI processing
- Core 1: UI + MIDI generation

### ZombieSynth.ino (CYD)
- Flash: ~420KB
- RAM: ~85KB
- Core 0: Audio synthesis (24kHz priority task)
- Core 1: UI + MIDI input

### ZombiSynthOLED_IOExpander.ino
- Flash: ~350KB
- RAM: ~60KB
- Core 0: Audio synthesis
- Core 1: UI + encoder + ADC polling

---

## 🚀 Build Flags

Recommended Arduino IDE settings:

```
Board: ESP32 Dev Module
Upload Speed: 921600
CPU Frequency: 240MHz
Flash Frequency: 80MHz
Flash Mode: QIO
Flash Size: 4MB (32Mb)
Partition Scheme: Default 4MB with spiffs
Core Debug Level: None
PSRAM: Disabled
```

---

## 🔍 Finding Files

### "Where is the synthesizer code?"
→ `synth_engine.h` (lines 1-545)

### "Where is the MIDI keyboard layout?"
→ `keyboard_mode.h` (for CYD-MIDI-Controller)

### "Where is the arpeggiator?"
→ `arpeggiator_patterns.h` (50 patterns)
→ `zombie_arp_mode.h` (UI for ZOMBI)

### "Where is the sequencer?"
→ `sequencer_mode.h` (for CYD-MIDI-Controller)
→ `zombie_step_sequencer.h` (for ZOMBI)

### "Where is the touchscreen config?"
→ `User_Setup.h` (TFT_eSPI configuration)
→ Lines 206-212 (pin definitions)

### "Where is the OLED config?"
→ `zombie_oled_ui.h` (line 32: U8G2_ST7567S)

---

## 📦 Arduino Library Requirements

### For CYD Touchscreen Variants:
```bash
TFT_eSPI (by Bodmer) - version 2.5.x
XPT2046_Touchscreen (by Paul Stoffregen) - version 1.4.x
```

### For OLED Variants:
```bash
U8g2 (by oliver) - version 2.34.x
```

### Built-in (no installation):
- Wire (I2C)
- SPI
- BLEDevice (ESP32)
- BLEServer (ESP32)
- driver/i2s (ESP32)

---

## 🎓 Learning Path

**Beginner:**
1. Upload `CYD_Touch_Diagnostic.ino` → Test hardware
2. Upload `CYD-MIDI-Controller.ino` → Try MIDI modes
3. Read `QUICK_START_GUIDE.md` → Understand features

**Intermediate:**
4. Upload `ZombieSynth.ino` → Explore synthesis
5. Read `synth_engine.h` → Understand audio pipeline
6. Modify parameters in UI modes

**Advanced:**
7. Build OLED variant with encoder
8. Add I/O expander + pots for hands-on control
9. Modify synthesis algorithm or add effects
10. Create custom arpeggiator patterns

---

**Last Updated:** 2026-02-27
**Project Status:** ✅ Stable - All variants working
**Touchscreen Fix:** Commit f25790c
