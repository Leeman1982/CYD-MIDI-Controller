#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include "config.h"

// ── Rotary Encoder + Button Input Handler ─────────────────────────────────────
// Debounced encoder with acceleration, plus back/confirm buttons.
// Call update() from loop() at ≥200 Hz.

#define DEBOUNCE_MS     30
#define ENC_ACCEL_MS    80   // If two clicks within this window, count as 4× step

class InputHandler {
private:
  // Encoder state
  volatile int  encDelta;
  uint8_t       encLastState;
  unsigned long encLastTime;

  // Button states
  struct Button {
    uint8_t       pin;
    bool          state;
    bool          lastReading;
    bool          justPressed;
    bool          justReleased;
    unsigned long lastDebounce;
  };

  Button encBtn;
  Button backBtn;
  Button confirmBtn;

  void initButton(Button& b, uint8_t pin) {
    b.pin = pin;
    b.state = false;
    b.lastReading = false;
    b.justPressed = false;
    b.justReleased = false;
    b.lastDebounce = 0;
    pinMode(pin, INPUT_PULLUP);
  }

  void updateButton(Button& b) {
    bool reading = !digitalRead(b.pin);  // Active low
    b.justPressed = false;
    b.justReleased = false;

    if (reading != b.lastReading) {
      b.lastDebounce = millis();
    }

    if ((millis() - b.lastDebounce) > DEBOUNCE_MS) {
      if (reading != b.state) {
        b.state = reading;
        if (b.state) b.justPressed = true;
        else         b.justReleased = true;
      }
    }

    b.lastReading = reading;
  }

public:
  void init() {
    // Encoder pins
    pinMode(ENC_A_PIN, INPUT_PULLUP);
    pinMode(ENC_B_PIN, INPUT_PULLUP);
    encDelta = 0;
    encLastState = (digitalRead(ENC_A_PIN) << 1) | digitalRead(ENC_B_PIN);
    encLastTime = 0;

    // Buttons
    initButton(encBtn,     ENC_SW_PIN);
    initButton(backBtn,    BTN_BACK_PIN);
    initButton(confirmBtn, BTN_CONFIRM_PIN);
  }

  void update() {
    // ── Encoder rotation ──────────────────────────────────────────────────
    uint8_t a = digitalRead(ENC_A_PIN);
    uint8_t b = digitalRead(ENC_B_PIN);
    uint8_t newState = (a << 1) | b;

    if (newState != encLastState) {
      // Gray code state table for direction detection
      // Transitions: 00→01=CW, 00→10=CCW, etc.
      static const int8_t encTable[16] = {
         0, -1,  1,  0,
         1,  0,  0, -1,
        -1,  0,  0,  1,
         0,  1, -1,  0
      };
      int8_t dir = encTable[(encLastState << 2) | newState];
      if (dir != 0) {
        // Acceleration: if turning fast, multiply step
        unsigned long now = millis();
        int mult = 1;
        if (now - encLastTime < ENC_ACCEL_MS) mult = 4;
        encDelta += dir * mult;
        encLastTime = now;
      }
      encLastState = newState;
    }

    // ── Buttons ─────────────────────────────────────────────────────────
    updateButton(encBtn);
    updateButton(backBtn);
    updateButton(confirmBtn);
  }

  // ── Polling API ─────────────────────────────────────────────────────────
  // Returns accumulated encoder delta since last call (can be >1 or <-1)
  int getEncoderDelta() {
    int d = encDelta;
    encDelta = 0;
    return d;
  }

  // Get next event (call in a loop until EVT_NONE)
  InputEvent getEvent() {
    if (encDelta > 0) { encDelta--; return EVT_ENC_CW; }
    if (encDelta < 0) { encDelta++; return EVT_ENC_CCW; }
    if (encBtn.justPressed)     { encBtn.justPressed = false;     return EVT_ENC_PRESS; }
    if (encBtn.justReleased)    { encBtn.justReleased = false;    return EVT_ENC_RELEASE; }
    if (backBtn.justPressed)    { backBtn.justPressed = false;    return EVT_BACK_PRESS; }
    if (backBtn.justReleased)   { backBtn.justReleased = false;   return EVT_BACK_RELEASE; }
    if (confirmBtn.justPressed) { confirmBtn.justPressed = false; return EVT_CONFIRM_PRESS; }
    if (confirmBtn.justReleased){ confirmBtn.justReleased = false;return EVT_CONFIRM_RELEASE; }
    return EVT_NONE;
  }

  // Direct state queries
  bool isEncPressed()     { return encBtn.state; }
  bool isBackPressed()    { return backBtn.state; }
  bool isConfirmPressed() { return confirmBtn.state; }
};

#endif
