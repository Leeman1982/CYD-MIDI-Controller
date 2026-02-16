#ifndef BUTTON_INPUT_H
#define BUTTON_INPUT_H

// ═══════════════════════════════════════════════════════════════════════════════
// ZOMBIE SS – Button Input Handler (RP2040)
// ═══════════════════════════════════════════════════════════════════════════════
//
// Supports 8 direct GPIO buttons with debounce and key repeat.
// Active LOW with internal pull-ups.
//
// Button events:
//   justPressed  – true for one cycle when button first pressed
//   justReleased – true for one cycle when button released
//   isHeld       – true while button is held down
//   isRepeating  – true at repeat interval while held
// ═══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>
#include "config.h"

struct ButtonState {
  bool     raw;           // Current raw reading (LOW = pressed)
  bool     pressed;       // Debounced state
  bool     justPressed;   // Edge: just went down
  bool     justReleased;  // Edge: just came up
  bool     isHeld;        // Held longer than repeat delay
  bool     isRepeating;   // Currently in repeat mode
  uint32_t pressTime;     // When the button was pressed (ms)
  uint32_t lastRepeat;    // Last repeat event time
  uint32_t lastChange;    // Last raw state change (for debounce)
};

class ButtonInput {
private:
  int pins[NUM_BUTTONS];
  ButtonState state[NUM_BUTTONS];

public:
  void init() {
    pins[BTN_UP]     = BTN_UP_PIN;
    pins[BTN_DOWN]   = BTN_DOWN_PIN;
    pins[BTN_LEFT]   = BTN_LEFT_PIN;
    pins[BTN_RIGHT]  = BTN_RIGHT_PIN;
    pins[BTN_CENTER] = BTN_CENTER_PIN;
    pins[BTN_A]      = BTN_A_PIN;
    pins[BTN_B]      = BTN_B_PIN;
    pins[BTN_C]      = BTN_C_PIN;

    for (int i = 0; i < NUM_BUTTONS; i++) {
      pinMode(pins[i], INPUT_PULLUP);
      memset(&state[i], 0, sizeof(ButtonState));
    }
  }

  void update() {
    uint32_t now = millis();

    for (int i = 0; i < NUM_BUTTONS; i++) {
      bool rawNow = (digitalRead(pins[i]) == LOW);

      // Clear single-frame events
      state[i].justPressed  = false;
      state[i].justReleased = false;
      state[i].isRepeating  = false;

      // Debounce
      if (rawNow != state[i].raw) {
        state[i].raw = rawNow;
        state[i].lastChange = now;
      }

      if (now - state[i].lastChange >= BTN_DEBOUNCE_MS) {
        bool wasPressed = state[i].pressed;
        state[i].pressed = state[i].raw;

        if (state[i].pressed && !wasPressed) {
          // Just pressed
          state[i].justPressed = true;
          state[i].pressTime   = now;
          state[i].lastRepeat  = now;
          state[i].isHeld      = false;
        }

        if (!state[i].pressed && wasPressed) {
          // Just released
          state[i].justReleased = true;
          state[i].isHeld       = false;
        }

        // Key repeat logic
        if (state[i].pressed) {
          uint32_t heldTime = now - state[i].pressTime;

          if (heldTime >= BTN_REPEAT_MS) {
            state[i].isHeld = true;
            uint32_t repeatInterval = BTN_REPEAT_FAST;

            if (now - state[i].lastRepeat >= repeatInterval) {
              state[i].isRepeating = true;
              state[i].lastRepeat  = now;
            }
          }
        }
      }
    }
  }

  // ── Accessors ─────────────────────────────────────────────────────────────
  bool pressed(int btn)      { return (btn >= 0 && btn < NUM_BUTTONS) ? state[btn].justPressed : false; }
  bool released(int btn)     { return (btn >= 0 && btn < NUM_BUTTONS) ? state[btn].justReleased : false; }
  bool held(int btn)         { return (btn >= 0 && btn < NUM_BUTTONS) ? state[btn].isHeld : false; }
  bool repeating(int btn)    { return (btn >= 0 && btn < NUM_BUTTONS) ? state[btn].isRepeating : false; }
  bool isDown(int btn)       { return (btn >= 0 && btn < NUM_BUTTONS) ? state[btn].pressed : false; }

  // Convenience: returns true on press OR repeat (for value adjustment)
  bool pressOrRepeat(int btn) {
    if (btn < 0 || btn >= NUM_BUTTONS) return false;
    return state[btn].justPressed || state[btn].isRepeating;
  }
};

#endif
