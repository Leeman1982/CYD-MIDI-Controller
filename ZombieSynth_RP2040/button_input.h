#ifndef BUTTON_INPUT_H
#define BUTTON_INPUT_H

// ═══════════════════════════════════════════════════════════════════════════════
// ZOMBIE SS – 4×4 Matrix Keypad Scanner (RP2040)
// ═══════════════════════════════════════════════════════════════════════════════
//
// Scans a 4×4 key matrix connected to the 1.3" OLED module.
// Rows (R1-R4) are driven LOW one at a time as outputs.
// Columns (C1-C4) are read as inputs with internal pull-ups.
// A key press grounds the column through the row, reading LOW.
//
// 16 buttons with debounce, edge detection, and auto-repeat.
// ═══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>
#include "config.h"

struct ButtonState {
  bool     pressed;
  bool     justPressed;
  bool     justReleased;
  bool     isHeld;
  bool     isRepeating;
  uint32_t pressTime;
  uint32_t lastRepeat;
  // Debounce
  bool     rawState;
  uint32_t lastChange;
};

class ButtonInput {
private:
  int rowPins[MATRIX_ROWS];
  int colPins[MATRIX_COLS];
  ButtonState state[NUM_BUTTONS];

  void scanMatrix(bool rawOut[NUM_BUTTONS]) {
    for (int r = 0; r < MATRIX_ROWS; r++) {
      // Drive this row LOW
      pinMode(rowPins[r], OUTPUT);
      digitalWrite(rowPins[r], LOW);

      // Small settle time (important for reliable reads)
      delayMicroseconds(5);

      // Read columns
      for (int c = 0; c < MATRIX_COLS; c++) {
        int idx = r * MATRIX_COLS + c;
        rawOut[idx] = (digitalRead(colPins[c]) == LOW);
      }

      // Release row (high-Z / input)
      pinMode(rowPins[r], INPUT);
    }
  }

public:
  void init() {
    rowPins[0] = ROW1_PIN;
    rowPins[1] = ROW2_PIN;
    rowPins[2] = ROW3_PIN;
    rowPins[3] = ROW4_PIN;

    colPins[0] = COL1_PIN;
    colPins[1] = COL2_PIN;
    colPins[2] = COL3_PIN;
    colPins[3] = COL4_PIN;

    // Rows start as inputs (high-Z, not driving)
    for (int r = 0; r < MATRIX_ROWS; r++) {
      pinMode(rowPins[r], INPUT);
    }

    // Columns are inputs with pull-ups (idle HIGH)
    for (int c = 0; c < MATRIX_COLS; c++) {
      pinMode(colPins[c], INPUT_PULLUP);
    }

    memset(state, 0, sizeof(state));
  }

  void update() {
    uint32_t now = millis();
    bool raw[NUM_BUTTONS];
    scanMatrix(raw);

    for (int i = 0; i < NUM_BUTTONS; i++) {
      // Clear single-frame events
      state[i].justPressed  = false;
      state[i].justReleased = false;
      state[i].isRepeating  = false;

      // Debounce
      if (raw[i] != state[i].rawState) {
        state[i].rawState   = raw[i];
        state[i].lastChange = now;
      }

      if (now - state[i].lastChange >= BTN_DEBOUNCE_MS) {
        bool wasPressed = state[i].pressed;
        state[i].pressed = state[i].rawState;

        if (state[i].pressed && !wasPressed) {
          state[i].justPressed = true;
          state[i].pressTime   = now;
          state[i].lastRepeat  = now;
          state[i].isHeld      = false;
        }

        if (!state[i].pressed && wasPressed) {
          state[i].justReleased = true;
          state[i].isHeld       = false;
        }

        // Auto-repeat
        if (state[i].pressed) {
          uint32_t heldTime = now - state[i].pressTime;
          if (heldTime >= BTN_REPEAT_MS) {
            state[i].isHeld = true;
            if (now - state[i].lastRepeat >= BTN_REPEAT_FAST) {
              state[i].isRepeating = true;
              state[i].lastRepeat  = now;
            }
          }
        }
      }
    }
  }

  // ── Accessors ─────────────────────────────────────────────────────────────
  bool pressed(int btn)       { return (btn >= 0 && btn < NUM_BUTTONS) ? state[btn].justPressed : false; }
  bool released(int btn)      { return (btn >= 0 && btn < NUM_BUTTONS) ? state[btn].justReleased : false; }
  bool held(int btn)          { return (btn >= 0 && btn < NUM_BUTTONS) ? state[btn].isHeld : false; }
  bool repeating(int btn)     { return (btn >= 0 && btn < NUM_BUTTONS) ? state[btn].isRepeating : false; }
  bool isDown(int btn)        { return (btn >= 0 && btn < NUM_BUTTONS) ? state[btn].pressed : false; }

  // True on initial press OR auto-repeat (for value adjustment buttons)
  bool pressOrRepeat(int btn) {
    if (btn < 0 || btn >= NUM_BUTTONS) return false;
    return state[btn].justPressed || state[btn].isRepeating;
  }
};

#endif
