# ZOMBI SS OLED Synthesizer - I/O Expander Wiring Guide

Complete wiring guide for the enhanced OLED variant with PCF8574 I2C I/O expander and analog potentiometer control.

## Bill of Materials (BOM)

### Core Components
- **1x ESP32 Development Board** (any variant with exposed GPIOs)
- **1x 128×64 I2C OLED Display** (ST7567S controller, 4-pin I2C)
- **1x PCF8574 I2C I/O Expander Module** (8-bit digital I/O)
- **1x Rotary Encoder** with integrated push button (KY-040 or similar)

### Controls
- **6x 10kΩ Linear Potentiometers** (B10K)
- **7x Momentary Push Buttons** (SPST NO, normally open)
- **6x Potentiometer Knobs** (to fit 6mm shaft)

### Audio Output (Choose One)
- **Option 1**: PCM5102 I2S DAC module (best quality)
- **Option 2**: ESP32 internal DAC + RC filter (budget option)

### Power & Connectivity
- **1x USB Cable** (for ESP32 programming and power)
- **1x MIDI DIN 5-pin Female Connector** (optional, for MIDI input)
- **1x 6N138 Optocoupler** (for MIDI input circuit)
- **1x 1N4148 Diode** (for MIDI input protection)
- **Resistors**: 220Ω (1x), 470Ω (1x) for MIDI circuit

### Miscellaneous
- **Breadboard or Prototype PCB**
- **Jumper wires** (M-M, M-F)
- **Enclosure** (optional, for standalone build)

## I2C Bus Connections

Both the OLED display and PCF8574 share the same I2C bus:

```
ESP32 GPIO 21 (SDA) ──┬── OLED SDA
                      └── PCF8574 SDA

ESP32 GPIO 22 (SCL) ──┬── OLED SCL
                      └── PCF8574 SCL

ESP32 3.3V ───────────┬── OLED VCC
                      └── PCF8574 VCC

ESP32 GND ────────────┬── OLED GND
                      └── PCF8574 GND
```

**Note**: I2C pull-up resistors (4.7kΩ) may already be present on modules. If experiencing communication issues, add external 4.7kΩ pull-ups from SDA/SCL to 3.3V.

## PCF8574 I2C Address Configuration

The PCF8574 address is set via A0, A1, A2 pins:

| A2 | A1 | A0 | I2C Address |
|----|----|----|-------------|
| 0  | 0  | 0  | 0x20 (default) |
| 0  | 0  | 1  | 0x21 |
| 0  | 1  | 0  | 0x22 |
| 0  | 1  | 1  | 0x23 |
| 1  | 0  | 0  | 0x24 |
| 1  | 1  | 1  | 0x27 |

**Default Configuration**: Leave A0, A1, A2 unconnected or tied to GND for address 0x20.

**If using different address**: Update `#define PCF8574_ADDR 0x20` in `zombie_io_expander.h`

## Button Connections via PCF8574

All 7 buttons connect to PCF8574 pins. Buttons are active-low (pressed = GND).

```
PCF8574 Pin Assignments:
┌─────────────┬───────────────────┐
│ PCF8574 Pin │ Function          │
├─────────────┼───────────────────┤
│ P0          │ MODE Button       │
│ P1          │ PLAY/STOP Button  │
│ P2          │ OCT- Button       │
│ P3          │ OCT+ Button       │
│ P4          │ SHIFT Button      │
│ P5          │ MENU/BACK Button  │
│ P6          │ REC Button        │
│ P7          │ SPARE (future)    │
└─────────────┴───────────────────┘
```

### Wiring Each Button:

```
                    ┌────── 10kΩ Pull-up ────── PCF8574 VCC
                    │
PCF8574 Px ─────────┼────────┐
                            │
                         Button
                            │
                           GND
```

**Internal Pull-ups**: PCF8574 requires external pull-up resistors. Most PCF8574 modules include built-in 10kΩ pull-ups. If your module doesn't have them, add 10kΩ resistors from each Px pin to VCC.

**Simplified Wiring** (if module has built-in pull-ups):
```
Button 1 (MODE):   PCF8574 P0 ──── Switch ──── GND
Button 2 (PLAY):   PCF8574 P1 ──── Switch ──── GND
Button 3 (OCT-):   PCF8574 P2 ──── Switch ──── GND
Button 4 (OCT+):   PCF8574 P3 ──── Switch ──── GND
Button 5 (SHIFT):  PCF8574 P4 ──── Switch ──── GND
Button 6 (MENU):   PCF8574 P5 ──── Switch ──── GND
Button 7 (REC):    PCF8574 P6 ──── Switch ──── GND
```

## Rotary Encoder Connections

The rotary encoder connects directly to ESP32 GPIOs (not via PCF8574) for better responsiveness:

```
Encoder Pin    ESP32 Pin    Description
─────────────────────────────────────────
CLK (A)    →   GPIO 25      Phase A
DT (B)     →   GPIO 26      Phase B
SW         →   GPIO 27      Push button
+          →   3.3V         Power
GND        →   GND          Ground
```

**Notes**:
- Internal pull-ups are enabled in software
- No external resistors needed for encoder
- Encoder should be mechanical type (not optical) for best compatibility

## Analog Potentiometer Connections

6 potentiometers connect to ESP32 ADC pins. All pots are 10kΩ linear (B10K).

```
Potentiometer Wiring (all 6 pots use same pattern):

        3.3V
         │
         ├─── Pot Pin 1 (CCW end)
         │
      ┌──┴──┐
      │ POT │
      └──┬──┘
         │
         ├─── Pot Pin 2 (Wiper) ──→ ESP32 ADC Pin
         │
        GND
         │
         └─── Pot Pin 3 (CW end)
```

### ESP32 ADC Pin Assignments:

| Pot Function      | ESP32 Pin | ADC Channel | Control          |
|-------------------|-----------|-------------|------------------|
| Filter Cutoff     | GPIO 36   | ADC1_CH0    | Low-pass cutoff  |
| Filter Resonance  | GPIO 39   | ADC1_CH3    | Filter Q         |
| Master Volume     | GPIO 34   | ADC1_CH6    | Output level     |
| Envelope Amount   | GPIO 35   | ADC1_CH7    | Filter env depth |
| Attack Time       | GPIO 32   | ADC1_CH4    | Envelope attack  |
| Release Time      | GPIO 33   | ADC1_CH5    | Envelope release |

**ADC Configuration**:
- Resolution: 12-bit (0-4095)
- Range: 0-3.3V
- Attenuation: 11dB (full 3.3V range)
- Smoothing: Exponential filter (15% factor) + 0.5% deadzone

**Important**: Use ADC1 channels only. ADC2 channels conflict with WiFi and should be avoided.

### Complete Single Pot Wiring Example:

```
Filter Cutoff Pot:

3.3V ──── Pot Left Pin (1)
              │
           ┌──┴──┐
           │10kΩ │
           │ POT │
           └──┬──┘
              │
              ├───────→ ESP32 GPIO 36 (VP)
              │
GND ───── Pot Right Pin (3)
```

Repeat this pattern for all 6 pots, changing only the ESP32 GPIO pin number.

## Audio Output Options

### Option 1: I2S DAC (PCM5102) - Recommended

```
PCM5102 Module    ESP32 Pin     Description
───────────────────────────────────────────
VIN          →    3.3V          Power
GND          →    GND           Ground
BCK          →    GPIO 22       Bit clock
LCK (LRCK)   →    GPIO 27       L/R clock
DIN          →    GPIO 17       Data
SCK          →    (leave open)  System clock
XSMT         →    3.3V          Un-mute
FLT          →    GND           Normal filter
DEMP         →    GND           De-emphasis off
FMT          →    GND           I2S format

Audio Output:
LOUT         →    Left channel out
ROUT         →    Right channel out
AGND         →    Audio ground
```

**Notes**:
- Best audio quality with low noise
- No external components needed
- Outputs line-level signal (can drive powered speakers or headphones)

### Option 2: ESP32 Internal DAC with RC Filter

```
ESP32 DAC Output:

GPIO 25 (DAC1) ───┬─── 1kΩ ───┬─── Left Audio Out
                  │            │
                  │           10µF
                  │            │
                  │           GND

GPIO 26 (DAC2) ───┬─── 1kΩ ───┬─── Right Audio Out
                  │            │
                  │           10µF
                  │            │
                  │           GND
```

**Component Values**:
- Resistor: 1kΩ (1/4W)
- Capacitor: 10µF electrolytic (16V or higher)

**Notes**:
- Budget option using ESP32's built-in 8-bit DAC
- Requires RC low-pass filter to remove switching noise
- Lower quality than I2S DAC but acceptable for testing
- Output level ~1.0V peak-to-peak

**WARNING**: If using internal DAC, GPIO 25 and 26 are NOT available for rotary encoder. You must reassign encoder pins in the code.

## MIDI Input Circuit

Standard MIDI input using 6N138 optocoupler:

```
MIDI DIN Socket (5-pin female, rear view):

    ┌─── 4 ───┐
  ┌─────┴─────┐
  │   5   2   │
  │  ╱       ╲│
  └─────3─────┘
    └─────────┘

Pin 2 ──── GND (shield)
Pin 4 ──┬─── 220Ω ───┬─── 6N138 Pin 2 (Anode)
        │             │
        └── 1N4148 ──┘ (Cathode to Pin 4, Anode to resistor)

Pin 5 ──── 6N138 Pin 3 (Cathode)


6N138 Optocoupler Connections:

Pin 2 (Anode)     ← MIDI Pin 4 via 220Ω + diode
Pin 3 (Cathode)   ← MIDI Pin 5
Pin 5 (GND)       ← ESP32 GND
Pin 6 (Output)    → GPIO 16 (UART2 RX)
Pin 7 (Enable)    → 470Ω → ESP32 3.3V
Pin 8 (VCC)       → ESP32 3.3V


Complete MIDI Input Circuit:

MIDI Pin 4 ──┬── 220Ω ──┬── 6N138 Pin 2
             │          │
             └ 1N4148 ──┘ (protection diode)

MIDI Pin 5 ───────────────── 6N138 Pin 3

6N138 Pin 8 ─────────────── ESP32 3.3V
6N138 Pin 5 ─────────────── ESP32 GND
6N138 Pin 7 ──── 470Ω ───── ESP32 3.3V
6N138 Pin 6 ─────────────── ESP32 GPIO 16 (UART2 RX)
```

**MIDI Settings**:
- Baud rate: 31,250 bps
- Format: 8N1 (8 data bits, no parity, 1 stop bit)
- UART: UART2 on GPIO 16

## Complete Pin Assignment Table

| Component        | ESP32 Pin | Alt Function | Notes                    |
|------------------|-----------|--------------|--------------------------|
| **I2C Bus**      |           |              |                          |
| OLED SDA         | GPIO 21   | I2C SDA      | Shared with PCF8574      |
| OLED SCL         | GPIO 22   | I2C SCL      | Shared with PCF8574      |
| **Rotary Encoder** |         |              |                          |
| Encoder A        | GPIO 25   | -            | Phase A (CLK)            |
| Encoder B        | GPIO 26   | -            | Phase B (DT)             |
| Encoder SW       | GPIO 27   | -            | Push button              |
| **Analog Pots**  |           |              |                          |
| Cutoff Pot       | GPIO 36   | ADC1_CH0     | VP pin                   |
| Resonance Pot    | GPIO 39   | ADC1_CH3     | VN pin                   |
| Volume Pot       | GPIO 34   | ADC1_CH6     | Input only               |
| Env Amount Pot   | GPIO 35   | ADC1_CH7     | Input only               |
| Attack Pot       | GPIO 32   | ADC1_CH4     | -                        |
| Release Pot      | GPIO 33   | ADC1_CH5     | -                        |
| **I2S DAC**      |           |              |                          |
| I2S BCK          | GPIO 22   | I2S0_BCK     | Shared with I2C SCL*     |
| I2S LRC          | GPIO 27   | I2S0_WS      | Shared with Enc SW*      |
| I2S DATA         | GPIO 17   | I2S0_DATA    | -                        |
| **MIDI**         |           |              |                          |
| MIDI RX          | GPIO 16   | UART2_RX     | Via optocoupler          |

**Important Notes**:
- GPIO 22 is shared between I2C SCL and I2S BCK - this works because I2C uses open-drain signaling
- GPIO 27 is shared between encoder switch and I2S LRCK - encoder should use a different pin if using I2S
- If using internal DAC instead of I2S, GPIO 25/26 become DAC outputs and encoder must move to different pins

## Recommended Encoder Pin Alternatives (if using I2S DAC)

If GPIO 25/26/27 are needed for I2S audio output:

```
Encoder A    → GPIO 14
Encoder B    → GPIO 12
Encoder SW   → GPIO 13
```

Update these pin definitions in `zombie_io_expander.h`:
```cpp
#define ENC_A_PIN   14
#define ENC_B_PIN   12
#define ENC_SW_PIN  13
```

## Power Supply Requirements

**Total Current Draw**:
- ESP32: ~200mA (peak during WiFi, ~80mA typical)
- OLED Display: ~30mA
- PCF8574: ~5mA
- Backlit buttons (if used): 7 × 20mA = 140mA
- **Total**: ~250-400mA @ 3.3V

**Power Options**:
1. **USB (5V)**: ESP32's onboard regulator → 3.3V (most common)
2. **External 3.3V**: Direct to ESP32 3.3V pin (regulated, max 600mA)
3. **Battery**: 3.7V LiPo → 3.3V LDO regulator → ESP32

**Recommended**: Power via USB (5V) and let ESP32's onboard regulator supply 3.3V.

## Physical Layout Suggestions

### Front Panel Layout (suggested arrangement):

```
┌─────────────────────────────────────────┐
│  ZOMBI SS SYNTHESIZER                   │
├─────────────────────────────────────────┤
│                                         │
│        ┌───────────────────┐            │
│        │                   │            │
│        │   OLED Display    │            │
│        │   128 × 64        │            │
│        │                   │            │
│        └───────────────────┘            │
│                                         │
│  ╭───╮  ╭───╮  ╭───╮  ╭───╮  ╭───╮  ╭─── │
│  │CUT│  │RES│  │VOL│  │ENV│  │ATK│  │REL│ │ ← Knobs
│  ╰───╯  ╰───╯  ╰───╯  ╰───╯  ╰───╯  ╰─── │
│                                         │
│           ┌─────────┐                   │
│           │ ENCODER │                   │ ← Rotary Encoder
│           └─────────┘                   │
│                                         │
│  [MODE] [PLAY] [OCT-] [OCT+]           │ ← Buttons
│  [SHIFT] [MENU] [REC]                  │
│                                         │
└─────────────────────────────────────────┘
```

**Dimensions (suggested)**:
- Enclosure: 200mm × 120mm × 50mm
- Front panel: 3mm acrylic or aluminum
- Knob spacing: 30mm centers
- Button spacing: 25mm centers

## Assembly Steps

1. **Test I2C Bus**:
   - Connect OLED and PCF8574 to ESP32
   - Upload I2C scanner sketch to verify addresses (0x3C for OLED, 0x20 for PCF8574)

2. **Wire Rotary Encoder**:
   - Connect encoder to GPIO 25, 26, 27
   - Test encoder rotation and button press

3. **Wire Potentiometers**:
   - Connect all 6 pots to ESP32 ADC pins
   - Test by reading raw ADC values

4. **Wire Buttons to PCF8574**:
   - Connect all 7 buttons to PCF8574 P0-P6
   - Verify pull-ups are present (measure ~3.3V when button not pressed)

5. **Wire Audio Output**:
   - Connect I2S DAC or internal DAC with RC filter
   - Test with simple tone generation

6. **Add MIDI Input** (optional):
   - Build optocoupler circuit on breadboard
   - Connect to GPIO 16
   - Test with MIDI keyboard

7. **Upload Firmware**:
   - Open `ZombiSynthOLED_IOExpander.ino`
   - Upload to ESP32
   - Check serial monitor for initialization messages

8. **Calibrate Pots**:
   - Turn each pot fully CCW (should read ~0%)
   - Turn each pot fully CW (should read ~100%)
   - Adjust if needed by measuring 3.3V on pot ends

## Troubleshooting

### I2C Issues
**Problem**: "PCF8574 not found" error
- **Solution**: Check I2C wiring (SDA/SCL not swapped)
- Run I2C scanner to verify addresses
- Check pull-up resistors (4.7kΩ to 3.3V)

### OLED Display Issues
**Problem**: Blank or garbage on display
- **Solution**: Verify ST7567S controller type
- Try different I2C address (0x3C or 0x3F)
- Check contrast setting in code

### Button Issues
**Problem**: Buttons not responding
- **Solution**: Verify PCF8574 pull-ups are present
- Measure voltage on Px pins (should be 3.3V when not pressed, 0V when pressed)
- Check button wiring (should connect Px to GND)

### Pot Issues
**Problem**: Jittery or unstable pot readings
- **Solution**: Increase smoothing factor in code
- Add 100nF capacitor across pot wiper to GND
- Check for loose connections

### Audio Issues
**Problem**: No audio output
- **Solution**: Check I2S pin connections
- Verify synth engine is initialized
- Check master volume pot is not at zero

**Problem**: Distorted audio
- **Solution**: Lower master volume in code
- Check power supply is stable (measure 3.3V under load)
- Verify I2S clock settings

## Advanced Modifications

### Add CV/Gate Outputs
Connect DAC (MCP4725) via I2C for CV out:
```
MCP4725 SDA → ESP32 GPIO 21 (shared I2C)
MCP4725 SCL → ESP32 GPIO 22 (shared I2C)
MCP4725 OUT → 1/8" jack (0-3.3V CV)
```

### Add RGB LED Indicators
Connect WS2812B LED strip:
```
WS2812B DIN → ESP32 GPIO 15
WS2812B VCC → 5V
WS2812B GND → GND
```

### Add External Trigger Inputs
Use PCF8574 P7 (spare) for external clock/trigger:
```
Trigger Input ──┬── 10kΩ ─── 3.3V
                │
                └── PCF8574 P7
```

## Schematic Files

Full schematic diagrams are available in:
- `schematics/zombi_oled_ioexpander_main.pdf` (main board)
- `schematics/zombi_oled_controls.pdf` (pots and buttons)
- `schematics/zombi_oled_audio.pdf` (audio output options)

## Support

For issues, questions, or contributions:
- GitHub Issues: [link to repo]
- Discussion Forum: [link]
- Email: [email]

## License

This hardware design and documentation are released under the MIT License.
See LICENSE file for details.
