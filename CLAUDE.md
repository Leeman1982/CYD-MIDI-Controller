# CLAUDE.md

## Mode

Claude memory synth dev mode is **enabled**.

## Project Overview

CYD MIDI Controller - A touchscreen Bluetooth MIDI controller for the ESP32-2432S028R "Cheap Yellow Display" (CYD). Written as an Arduino sketch in C++.

## Architecture

- **CYD-MIDI-Controller.ino** - Main sketch entry point
- **common_definitions.h** - Shared types, constants, and global state
- **midi_utils.h** - MIDI message helpers and Bluetooth MIDI transport
- **ui_elements.h** - Reusable UI drawing primitives for the TFT display
- Mode modules (each self-contained in a header):
  - `keyboard_mode.h` - Virtual piano keyboard
  - `sequencer_mode.h` - 16-step sequencer (BEATS)
  - `bouncing_ball_mode.h` - Generative ambient mode (ZEN)
  - `physics_drop_mode.h` - Physics ball-drop mode (DROP)
  - `random_generator_mode.h` - Random music generator (RNG)
  - `xy_pad_mode.h` - X/Y touch pad
  - `arpeggiator_mode.h` - Arpeggiator (ARP)
  - `grid_piano_mode.h` - Grid piano layout
  - `auto_chord_mode.h` - Auto-chord progressions
  - `lfo_mode.h` - LFO modulation

## Hardware Target

- **Board**: ESP32-2432S028R (CYD)
- **Display**: ILI9341 320x240 TFT with resistive touch (XPT2046)
- **Connectivity**: Bluetooth MIDI (BLE)
- **Framework**: Arduino / ESP32

## Development Notes

- All mode modules are header-only (.h files) included from the main .ino sketch
- Uses TFT_eSPI for display, XPT2046_Touchscreen for touch input, BLE MIDI for output
- `User_Setup.h` contains TFT_eSPI pin/display configuration for the CYD board
- Touch coordinates are calibrated specifically for the CYD hardware
