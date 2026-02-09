#ifndef COMMON_DEFINITIONS_H
#define COMMON_DEFINITIONS_H

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <BLEDevice.h>

// Color scheme - ZOMBIE SS Theme (Black/Red/White)
// Fixed with TFT_INVERSION_OFF in User_Setup.h
#define THEME_BG         0x0000  // Pure Black
#define THEME_SURFACE    0x1082  // Dark gray
#define THEME_PRIMARY    0xF800  // Pure Red
#define THEME_SECONDARY  0xC000  // Dark Red
#define THEME_ACCENT     0xFFFF  // Pure White
#define THEME_SUCCESS    0x07E0  // Green
#define THEME_WARNING    0xFFE0  // Yellow
#define THEME_ERROR      0xF800  // Red
#define THEME_TEXT       0xF800  // Red text
#define THEME_TEXT_DIM   0x8800  // Dim Red
#define THEME_OUTLINE    0xFFFF  // White outlines

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
  ZOMBIE_SYNTH,
  ZOMBIE_ARP,
  ZOMBIE_SEQ
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