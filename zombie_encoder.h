#ifndef ZOMBIE_ENCODER_H
#define ZOMBIE_ENCODER_H

#include <Arduino.h>

// Rotary encoder with integrated push button
// Hardware connections (adjust pins as needed):
// - Encoder A: GPIO 34
// - Encoder B: GPIO 35
// - Encoder SW (push): GPIO 32
// - Button 1 (MODE): GPIO 33
// - Button 2 (PLAY/STOP): GPIO 25
// - Button 3 (OCT-): GPIO 26
// - Button 4 (OCT+): GPIO 27

#define ENC_A_PIN 34
#define ENC_B_PIN 35
#define ENC_SW_PIN 32
#define BTN_MODE_PIN 33
#define BTN_PLAY_PIN 25
#define BTN_OCT_DOWN_PIN 26
#define BTN_OCT_UP_PIN 27

class RotaryEncoder {
private:
  int position;
  int lastPosition;
  volatile int encoderCount;

  bool lastA;
  bool lastB;

  unsigned long lastDebounceTime;
  static const unsigned long debounceDelay = 5;

public:
  RotaryEncoder() : position(0), lastPosition(0), encoderCount(0),
                    lastA(false), lastB(false), lastDebounceTime(0) {}

  void init() {
    pinMode(ENC_A_PIN, INPUT_PULLUP);
    pinMode(ENC_B_PIN, INPUT_PULLUP);

    lastA = digitalRead(ENC_A_PIN);
    lastB = digitalRead(ENC_B_PIN);
  }

  void update() {
    bool a = digitalRead(ENC_A_PIN);
    bool b = digitalRead(ENC_B_PIN);

    if (a != lastA || b != lastB) {
      if (millis() - lastDebounceTime > debounceDelay) {
        if (a != lastA) {
          if (a == LOW) {
            if (b == HIGH) {
              encoderCount++;
            } else {
              encoderCount--;
            }
          }
        }
        lastDebounceTime = millis();
      }
    }

    lastA = a;
    lastB = b;
  }

  int getPosition() {
    return encoderCount;
  }

  int getDelta() {
    int delta = encoderCount - lastPosition;
    lastPosition = encoderCount;
    return delta;
  }

  void resetDelta() {
    lastPosition = encoderCount;
  }
};

class ButtonManager {
private:
  struct Button {
    uint8_t pin;
    bool lastState;
    bool currentState;
    bool pressed;
    bool released;
    unsigned long lastDebounceTime;
  };

  Button encButton;      // Encoder push button
  Button modeButton;     // Mode switch
  Button playButton;     // Play/stop
  Button octDownButton;  // Octave down
  Button octUpButton;    // Octave up

  static const unsigned long debounceDelay = 50;

  void initButton(Button& btn, uint8_t pin) {
    btn.pin = pin;
    btn.lastState = HIGH;
    btn.currentState = HIGH;
    btn.pressed = false;
    btn.released = false;
    btn.lastDebounceTime = 0;
    pinMode(pin, INPUT_PULLUP);
  }

  void updateButton(Button& btn) {
    bool reading = digitalRead(btn.pin);
    btn.pressed = false;
    btn.released = false;

    if (reading != btn.lastState) {
      btn.lastDebounceTime = millis();
    }

    if ((millis() - btn.lastDebounceTime) > debounceDelay) {
      if (reading != btn.currentState) {
        btn.currentState = reading;

        if (btn.currentState == LOW) {
          btn.pressed = true;
        } else {
          btn.released = true;
        }
      }
    }

    btn.lastState = reading;
  }

public:
  ButtonManager() {}

  void init() {
    initButton(encButton, ENC_SW_PIN);
    initButton(modeButton, BTN_MODE_PIN);
    initButton(playButton, BTN_PLAY_PIN);
    initButton(octDownButton, BTN_OCT_DOWN_PIN);
    initButton(octUpButton, BTN_OCT_UP_PIN);
  }

  void update() {
    updateButton(encButton);
    updateButton(modeButton);
    updateButton(playButton);
    updateButton(octDownButton);
    updateButton(octUpButton);
  }

  bool isEncoderPressed() { return encButton.pressed; }
  bool isModePressed() { return modeButton.pressed; }
  bool isPlayPressed() { return playButton.pressed; }
  bool isOctDownPressed() { return octDownButton.pressed; }
  bool isOctUpPressed() { return octUpButton.pressed; }

  bool isEncoderHeld() { return encButton.currentState == LOW; }
  bool isModeHeld() { return modeButton.currentState == LOW; }
};

#endif
