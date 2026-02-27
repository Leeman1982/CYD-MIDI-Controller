# 2.4" CYD Touchscreen Configuration Guide

This guide helps you identify and configure your specific 2.4" ESP32 display board variant.

## Common 2.4" CYD Variants

### 1. ESP32-2432S024C (Most Common - Current Default)

**Board Identification:**
- Size: 2.4" (240x320 pixels)
- Display Driver: ILI9341
- Touch Controller: XPT2046 (Resistive)
- USB-C connector
- P3 header on back

**Pin Configuration:**
```cpp
// Display (HSPI)
TFT_MISO:  12
TFT_MOSI:  13
TFT_SCLK:  14
TFT_CS:    15
TFT_DC:    2
TFT_RST:   -1 (connected to ESP32 EN)
TFT_BL:    21 (backlight)

// Touch (VSPI custom pins)
TOUCH_IRQ:  36
TOUCH_MOSI: 32
TOUCH_MISO: 39
TOUCH_CLK:  25
TOUCH_CS:   33
```

**Status:** ✅ Configured (Current default in this project)

---

### 2. ESP32-2432S024R (Older Variant)

**Board Identification:**
- Size: 2.4" (240x320 pixels)
- Display Driver: ILI9341
- Touch Controller: XPT2046
- Micro-USB connector
- Different PCB layout

**Pin Configuration:**
Same as ESP32-2432S024C above.

**Status:** ✅ Compatible with current configuration

---

### 3. ESP32-2432S024N (Newer/Alternative)

**Board Identification:**
- Newer revision with improved components
- May have slightly different touch response
- USB-C connector

**Pin Configuration:**
Same as ESP32-2432S024C, but may require different touch calibration values.

**Touch Calibration (if needed):**
```cpp
// In your sketch, after ts.begin():
ts.setRotation(1);
// Add these if touch is inverted or offset:
// calibrationData.xMin = 200;
// calibrationData.xMax = 3700;
// calibrationData.yMin = 200;
// calibrationData.yMax = 3900;
```

---

### 4. ESP32-2432S024 with Shared SPI (Rare)

**Board Identification:**
- Touch uses same SPI bus as display
- May have only 4 wires for touch (no separate MISO/MOSI/CLK)

**Pin Configuration:**
```cpp
// Display and Touch share HSPI
TFT_MISO:  12
TFT_MOSI:  13
TFT_SCLK:  14
TFT_CS:    15
TFT_DC:    2
TFT_RST:   -1
TFT_BL:    21

TOUCH_IRQ:  36
TOUCH_MOSI: 13  // Shared with display
TOUCH_MISO: 12  // Shared with display
TOUCH_CLK:  14  // Shared with display
TOUCH_CS:   33
```

**To Use This Configuration:**
In `CYD-MIDI-Controller.ino`, change:
```cpp
// Use HSPI instead of VSPI
SPIClass touchSPI = SPIClass(HSPI);

// Update pin definitions
#define XPT2046_CLK 14   // Shared with TFT
#define XPT2046_MISO 12  // Shared with TFT
#define XPT2046_MOSI 13  // Shared with TFT
```

---

## Troubleshooting Touch Issues

### Step 1: Run Diagnostic

Upload and run `CYD_Touch_Diagnostic.ino` to test if touch is detected.

**What to check:**
1. Open Serial Monitor (115200 baud)
2. Touch the screen
3. Look for "✓ TOUCH DETECTED!" message
4. Check if raw coordinates are printed

### Step 2: If No Touch Detected

**Check physical connections:**
1. Ensure display is firmly seated in ESP32 socket
2. Check for bent pins
3. Look for cold solder joints on touch connector

**Try different SPI configuration:**

#### Option A: VSPI with custom pins (Default)
```cpp
SPIClass touchSPI = SPIClass(VSPI);
touchSPI.begin(25, 39, 32, 33); // CLK, MISO, MOSI, CS
ts.begin(touchSPI);
```

#### Option B: HSPI shared with display
```cpp
SPIClass touchSPI = SPIClass(HSPI);
touchSPI.begin(14, 12, 13, 33); // CLK, MISO, MOSI, CS
ts.begin(touchSPI);
```

#### Option C: Software SPI (slowest but most compatible)
```cpp
// In User_Setup.h, add:
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK 25
#define TOUCH_CS 33
#define TOUCH_IRQ 36

// In setup():
ts.begin(); // Uses software bit-banging
```

### Step 3: Check Rotation

Different board variants may require different rotation values:

```cpp
ts.setRotation(0); // Portrait
ts.setRotation(1); // Landscape (default)
ts.setRotation(2); // Portrait inverted
ts.setRotation(3); // Landscape inverted
```

Try each rotation and see which one gives correct touch coordinates.

### Step 4: Adjust Touch Threshold

Some boards need different pressure thresholds:

```cpp
// Lower threshold = more sensitive (may get false touches)
// Higher threshold = less sensitive (may miss light touches)

// In loop(), check pressure:
if (ts.touched()) {
  TS_Point p = ts.getPoint();

  // Add pressure threshold
  if (p.z > 200 && p.z < 3000) {  // Adjust these values
    // Valid touch
  }
}
```

### Step 5: Verify SPI Frequency

XPT2046 requires slower SPI than the display. Verify in `User_Setup.h`:

```cpp
#define SPI_TOUCH_FREQUENCY  2500000  // 2.5 MHz for touch
```

If touch is unreliable, try lowering:
```cpp
#define SPI_TOUCH_FREQUENCY  1000000  // 1 MHz (slower, more reliable)
```

---

## Current Configuration Summary

**Your project is currently configured for:**
- Board: ESP32-2432S024C (most common 2.4" variant)
- Display SPI: HSPI (pins 12, 13, 14, 15)
- Touch SPI: VSPI with custom pins (25, 32, 33, 39)
- Display Driver: ILI9341_2_DRIVER
- Resolution: 240x320
- Rotation: 1 (Landscape)

**Files configured:**
- ✅ `User_Setup.h` - Display pins
- ✅ `CYD-MIDI-Controller.ino` - Touch initialization
- ✅ `ZombieSynth.ino` - Touch initialization

---

## Quick Test Code

If still having issues, paste this minimal test in Arduino IDE:

```cpp
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#define XPT2046_IRQ 36
#define XPT2046_CS 33

SPIClass touchSPI = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

void setup() {
  Serial.begin(115200);
  touchSPI.begin(25, 39, 32, 33); // CLK, MISO, MOSI, CS
  ts.begin(touchSPI);
  ts.setRotation(1);
  Serial.println("Touch initialized. Touch the screen...");
}

void loop() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    Serial.printf("Touch: X=%d, Y=%d, Z=%d\n", p.x, p.y, p.z);
    delay(100);
  }
  delay(50);
}
```

Expected output when touching:
```
Touch: X=2000, Y=2500, Z=800
Touch: X=2010, Y=2490, Z=820
```

---

## Alternative Board Variants

### If you have a 2.4" board with FT6236 Capacitive Touch:

Some rare 2.4" boards use capacitive touch instead of resistive. These require a different library:

```cpp
#include <FT6236.h>

FT6236 ts = FT6236();

void setup() {
  ts.begin(40); // Threshold
}

void loop() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    // Use p.x, p.y
  }
}
```

**How to identify:**
- Touch feels smooth, no pressure needed
- No XPT2046 chip visible on back of display
- Touch connector has 4 pins (not 5)

---

## Getting More Help

If touch still doesn't work after trying all configurations:

1. **Post on GitHub Issues** with:
   - Board model/vendor
   - Photo of board front and back
   - Output from diagnostic sketch
   - Serial monitor log during boot

2. **Check vendor documentation**:
   - AliExpress/Amazon listing may have pinout
   - Some sellers provide test sketches

3. **ESP32 Discord/Reddit**:
   - r/esp32
   - ESP32 Discord server

4. **Use I2C Scanner** to check if touch is on I2C:
   ```
   Some capacitive touch controllers use I2C instead of SPI
   ```

---

## Summary

**Most likely solution:** Your board is ESP32-2432S024C and the current configuration should work. If not:

1. Run `CYD_Touch_Diagnostic.ino`
2. Try different rotations (0, 1, 2, 3)
3. Check pressure threshold (z value)
4. Verify SPI frequency is 2.5MHz or lower

The updated code now includes better initialization logging. Watch the Serial Monitor during boot to see if touch initialization succeeds.
