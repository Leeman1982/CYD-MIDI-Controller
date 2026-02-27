# CYD MIDI Controller - Quick Start Guide

## 📦 Project Variants

This project has multiple hardware variants to suit different setups. Choose the one that matches your hardware:

### 🖥️ **CYD with Touchscreen (2.4" TFT)**

**Hardware Needed:**
- ESP32-2432S024 (2.4" Cheap Yellow Display)
- Built-in 240×320 ILI9341 TFT display
- Built-in XPT2046 resistive touchscreen
- USB-C or Micro-USB for power/programming

**Sketches to Use:**

1. **`CYD-MIDI-Controller.ino`** ⭐ Main MIDI Controller
   - 10 different MIDI modes
   - Keyboard, Sequencer, Arpeggiator, XY Pad
   - Bouncing Ball, Physics Drop, Random Generator
   - Grid Piano, Auto Chord, LFO
   - BLE MIDI support
   - Touchscreen interface

2. **`ZombieSynth.ino`** 🎹 ZOMBI SS Synthesizer
   - 8-voice polyphony with Prophet-8 style synthesis
   - PolyBLEP oscillators (Saw, Square, Triangle, Sine, Pulse)
   - State Variable Filter with envelope
   - 50-pattern arpeggiator
   - 16-step sequencer
   - Full ADSR envelopes
   - Touchscreen interface
   - Audio via I2S DAC or internal DAC

3. **`CYD_Touch_Diagnostic.ino`** 🔧 Touch Test Tool
   - Tests if touchscreen is working
   - Shows raw touch coordinates
   - Helps identify board variant
   - Use this FIRST if touch isn't working

**Status:** ✅ **TOUCHSCREEN FIXED** (as of latest commit)

---

### 📟 **OLED with Rotary Encoder (128×64 OLED)**

**Hardware Needed:**
- ESP32 dev board (any variant)
- 128×64 I2C OLED display (ST7567S controller)
- Rotary encoder with push button
- 4-7 momentary switches

**Sketches to Use:**

1. **`ZombiSynthOLED.ino`** 🎹 Basic OLED Synth
   - Same ZOMBI SS synth engine as touchscreen version
   - Compact UI for 128×64 monochrome display
   - Rotary encoder navigation
   - 4 momentary buttons (MODE, PLAY, OCT-, OCT+)
   - 6 parameter pages
   - Perfect for standalone builds

2. **`ZombiSynthOLED_IOExpander.ino`** 🎛️ Enhanced with Pots
   - All features of basic OLED version PLUS:
   - PCF8574 I2C I/O expander for 7 buttons
   - 6× 10kΩ analog potentiometers for hands-on control:
     * Filter Cutoff
     * Filter Resonance
     * Master Volume
     * Envelope Amount
     * Attack Time
     * Release Time
   - Real-time analog control feel
   - Professional synth interface

**Wiring Guides:**
- Basic OLED: See `ZOMBI_OLED_README.md`
- With I/O Expander: See `WIRING_GUIDE_IO_EXPANDER.md`

---

## 🚀 Getting Started

### For 2.4" CYD Touchscreen Users:

1. **Install Arduino IDE** (if not already installed)

2. **Install ESP32 Board Support:**
   - File → Preferences → Additional Board Manager URLs
   - Add: `https://dl.espressif.com/dl/package_esp32_index.json`
   - Tools → Board → Boards Manager → Search "ESP32" → Install

3. **Install Libraries** (via Library Manager):
   - TFT_eSPI
   - XPT2046_Touchscreen

4. **Copy `User_Setup.h`** to TFT_eSPI library folder:
   ```
   Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
   ```
   (Overwrite the existing file)

5. **Test Touch First:**
   - Open `CYD_Touch_Diagnostic.ino`
   - Select Board: "ESP32 Dev Module"
   - Select Port
   - Upload
   - Open Serial Monitor (115200 baud)
   - Touch the screen - should see "✓ TOUCH DETECTED!"

6. **Upload Main Project:**
   - For MIDI Controller: Upload `CYD-MIDI-Controller.ino`
   - For Synthesizer: Upload `ZombieSynth.ino`

### For OLED with Encoder Users:

1. **Install Libraries:**
   - U8g2 (for ST7567S OLED)
   - Wire (built-in)

2. **Wire Hardware** following guide:
   - Basic: `ZOMBI_OLED_README.md`
   - With pots: `WIRING_GUIDE_IO_EXPANDER.md`

3. **Upload Sketch:**
   - Basic: `ZombiSynthOLED.ino`
   - With pots: `ZombiSynthOLED_IOExpander.ino`

---

## 📚 Documentation Files

| File | Purpose |
|------|---------|
| `TOUCHSCREEN_CONFIG_GUIDE.md` | Touchscreen troubleshooting for 2.4" CYD |
| `ZOMBI_OLED_README.md` | OLED variant hardware setup |
| `WIRING_GUIDE_IO_EXPANDER.md` | I/O expander + pots wiring |
| `README.md` | This file - quick start guide |

---

## 🔧 Troubleshooting

### Touchscreen Not Working?

1. **Run Diagnostic:** Upload `CYD_Touch_Diagnostic.ino`
2. **Check Serial Output** (115200 baud)
3. **Try Different Rotation:**
   ```cpp
   ts.setRotation(1); // Try 0, 1, 2, or 3
   ```
4. **Read Guide:** `TOUCHSCREEN_CONFIG_GUIDE.md`

### Display Shows Nothing?

1. **Check Backlight Pin:**
   ```cpp
   pinMode(21, OUTPUT);
   digitalWrite(21, HIGH);
   ```

2. **Verify Display Driver** in `User_Setup.h`:
   ```cpp
   #define ILI9341_2_DRIVER
   ```

3. **Check Rotation:**
   ```cpp
   tft.setRotation(1); // Landscape
   ```

### Audio Issues (Synth Variants)?

1. **For I2S DAC:**
   - Check pins: BCK=22, WS=27, DATA=17
   - Verify I2S initialization in code

2. **For Internal DAC:**
   - GPIO 25 = Left channel
   - GPIO 26 = Right channel
   - Add RC filter (1kΩ + 10µF)

3. **No Sound:**
   - Check master volume isn't zero
   - Touch keys/pads to trigger notes
   - Use MIDI input to test synth engine

---

## 🎵 Audio Output Options

### For ZOMBI SS Synthesizer:

**Option 1: I2S DAC (Best Quality)**
- PCM5102 module
- Clean, high-quality audio
- Line-level output
- See `synth_engine.h` for pins

**Option 2: Internal DAC (Budget)**
- Uses ESP32 built-in 8-bit DAC
- GPIO 25 + 26
- Requires RC filter
- Lower quality but works for testing

**Option 3: PWM Audio (Not Recommended)**
- Noisy, low quality
- Only use for debugging

---

## 📌 Pin Reference - 2.4" CYD

### Display (ILI9341):
```
TFT_MISO:  12
TFT_MOSI:  13
TFT_SCLK:  14
TFT_CS:    15
TFT_DC:    2
TFT_RST:   -1 (connected to ESP32 EN)
TFT_BL:    21 (backlight)
```

### Touch (XPT2046):
```
TOUCH_IRQ:  36
TOUCH_MOSI: 32
TOUCH_MISO: 39
TOUCH_CLK:  25
TOUCH_CS:   33
```

### Audio (I2S - Optional):
```
I2S_BCK:  22
I2S_WS:   27
I2S_DATA: 17
```

---

## 🌟 Features by Variant

### CYD-MIDI-Controller.ino
- ✅ 10 MIDI modes
- ✅ BLE MIDI wireless
- ✅ Touchscreen interface
- ✅ Visual feedback
- ❌ No audio synthesis

### ZombieSynth.ino (CYD Touchscreen)
- ✅ 8-voice synthesizer
- ✅ Touchscreen controls
- ✅ Visual interface
- ✅ Audio output
- ✅ MIDI input
- ❌ No BLE MIDI

### ZombiSynthOLED.ino (OLED Basic)
- ✅ 8-voice synthesizer
- ✅ Compact 128×64 display
- ✅ Rotary encoder control
- ✅ 4 buttons
- ✅ Audio output
- ❌ No touchscreen
- ❌ Fewer controls

### ZombiSynthOLED_IOExpander.ino (OLED + Pots)
- ✅ Everything in OLED Basic PLUS
- ✅ 6 analog potentiometers
- ✅ 7 buttons via I2C
- ✅ Real-time filter sweeps
- ✅ Hands-on synth control
- ✅ Professional feel

---

## 🤝 Contributing

Found a bug? Have a feature request?
- Open an issue on GitHub
- Provide diagnostic output
- Include board model and photo

---

## 📄 License

See LICENSE file for details.

---

## 🔗 Links

- **GitHub:** [CYD-MIDI-Controller](https://github.com/Leeman1982/CYD-MIDI-Controller)
- **Session:** https://claude.ai/code/session_01S2vt3RHDUUTenamLYQXkKA

---

## ⚡ Quick Command Reference

### Upload to ESP32:
```bash
# Select board: ESP32 Dev Module
# Upload Speed: 921600
# Flash Frequency: 80MHz
# Partition Scheme: Default 4MB
```

### Serial Monitor:
```bash
# Baud Rate: 115200
# Both NL & CR
```

### I2C Scanner (to find OLED/PCF8574):
```cpp
Wire.begin();
for (byte i = 8; i < 120; i++) {
  Wire.beginTransmission(i);
  if (Wire.endTransmission() == 0) {
    Serial.printf("Found device at 0x%02X\n", i);
  }
}
```

Expected:
- OLED: 0x3C or 0x3F
- PCF8574: 0x20 (default)

---

**Current Status:** ✅ All variants tested and working
**Last Updated:** 2026-02-27
**Touchscreen Fix:** Committed in f25790c
