#ifndef COMMON_DEFINITIONS_H
#define COMMON_DEFINITIONS_H

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <BLEDevice.h>

// Color scheme - Black background with blood red theme
#define THEME_BG         0x0000  // Pure black
#define THEME_SURFACE    0x2104  // Very dark red
#define THEME_PRIMARY    0xC800  // Blood red
#define THEME_SECONDARY  0xF800  // Bright red
#define THEME_ACCENT     0xF000  // Dark red accent
#define THEME_SUCCESS    0xC800  // Blood red for success
#define THEME_WARNING    0xFD20  // Orange warning
#define THEME_ERROR      0xF800  // Bright red error
#define THEME_TEXT       0xC800  // Blood red text
#define THEME_TEXT_DIM   0x8000  // Dim red

// BLE MIDI UUIDs
#define SERVICE_UUID        "03b80e5a-ede8-4b33-a751-6ce34ec4c700"
#define CHARACTERISTIC_UUID "7772e5db-3868-4112-a1a9-f2669d106bf3"

// Touch handling
struct TouchState {
  bool wasPressed = false;
  bool isPressed = false;
  bool justPressed = false;
  bool justReleased = false;
  int x = 0, y = 0;
};

// App modes
enum AppMode {
  MENU,
  KEYBOARD,
  SEQUENCER,
  BOUNCING_BALL,
  PHYSICS_DROP,
  RANDOM_GENERATOR,
  XY_PAD,
  ARPEGGIATOR,
  GRID_PIANO,
  AUTO_CHORD,
  LFO,
  SYNTH_SEQUENCER
};

// Music theory
struct Scale {
  String name;
  int intervals[12];
  int numNotes;
};

// Global scale definitions
extern Scale scales[];
extern const int NUM_SCALES;

// Global objects - declared in main file
extern TFT_eSPI tft;
extern XPT2046_Touchscreen ts;
extern BLECharacteristic *pCharacteristic;
extern bool deviceConnected;
extern uint8_t midiPacket[];
extern TouchState touch;
extern AppMode currentMode;

#endif