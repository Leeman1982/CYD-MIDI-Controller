#ifndef ZOMBIE_IO_EXPANDER_H
#define ZOMBIE_IO_EXPANDER_H

#include <Arduino.h>
#include <Wire.h>

// PCF8574 I2C I/O Expander
// Default I2C address: 0x20 (A2=A1=A0=0)
// Alternative addresses: 0x21-0x27 (set via A0, A1, A2 jumpers)

#define PCF8574_ADDR 0x20

// PCF8574 pin assignments (P0-P7)
#define PCF_BTN_MODE     0  // P0 - MODE button
#define PCF_BTN_PLAY     1  // P1 - PLAY/STOP button
#define PCF_BTN_OCT_DN   2  // P2 - Octave down
#define PCF_BTN_OCT_UP   3  // P3 - Octave up
#define PCF_BTN_SHIFT    4  // P4 - SHIFT modifier (for advanced functions)
#define PCF_BTN_MENU     5  // P5 - Quick menu/back
#define PCF_BTN_REC      6  // P6 - Record (for sequencer)
#define PCF_SPARE        7  // P7 - Spare for future use

// ESP32 ADC pins for analog potentiometers
// Using ADC1 channels (ADC2 conflicts with WiFi)
#define POT_CUTOFF_PIN   36  // VP  - Filter cutoff
#define POT_RESONANCE_PIN 39 // VN  - Filter resonance
#define POT_VOLUME_PIN   34  // GPIO34 - Master volume
#define POT_ENV_PIN      35  // GPIO35 - Filter envelope amount
#define POT_ATTACK_PIN   32  // GPIO32 - Attack time (shared control)
#define POT_RELEASE_PIN  33  // GPIO33 - Release time (shared control)

// Rotary encoder (direct GPIO, not via PCF8574 for better response)
#define ENC_A_PIN   25   // Encoder A
#define ENC_B_PIN   26   // Encoder B
#define ENC_SW_PIN  27   // Encoder switch

class PCF8574Expander {
private:
  uint8_t i2cAddress;
  uint8_t lastState;
  uint8_t currentState;
  uint8_t buttonPressed;
  uint8_t buttonReleased;
  unsigned long lastReadTime;
  static const unsigned long readInterval = 10; // Read every 10ms

public:
  PCF8574Expander(uint8_t addr = PCF8574_ADDR) :
    i2cAddress(addr), lastState(0xFF), currentState(0xFF),
    buttonPressed(0), buttonReleased(0), lastReadTime(0) {}

  bool init() {
    Wire.begin();
    Wire.beginTransmission(i2cAddress);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      // PCF8574 found, set all pins as inputs with pull-ups
      write(0xFF);
      delay(10);
      read();
      Serial.printf("PCF8574 initialized at 0x%02X\n", i2cAddress);
      return true;
    } else {
      Serial.printf("PCF8574 not found at 0x%02X (error: %d)\n", i2cAddress, error);
      return false;
    }
  }

  uint8_t read() {
    Wire.requestFrom(i2cAddress, (uint8_t)1);
    if (Wire.available()) {
      currentState = Wire.read();
      return currentState;
    }
    return 0xFF;
  }

  void write(uint8_t value) {
    Wire.beginTransmission(i2cAddress);
    Wire.write(value);
    Wire.endTransmission();
  }

  void update() {
    unsigned long now = millis();
    if (now - lastReadTime < readInterval) return;

    lastReadTime = now;
    uint8_t state = read();

    // Detect button changes (active low - button pressed = 0)
    buttonPressed = ~state & (state ^ lastState);   // Newly pressed
    buttonReleased = state & (state ^ lastState);   // Newly released

    lastState = state;
  }

  bool isButtonPressed(uint8_t pin) {
    return (buttonPressed & (1 << pin)) != 0;
  }

  bool isButtonReleased(uint8_t pin) {
    return (buttonReleased & (1 << pin)) != 0;
  }

  bool isButtonHeld(uint8_t pin) {
    return (currentState & (1 << pin)) == 0;  // Active low
  }

  uint8_t getState() {
    return currentState;
  }
};

class AnalogPots {
private:
  struct Pot {
    uint8_t pin;
    int rawValue;
    float smoothedValue;
    float lastOutputValue;
    static const float smoothingFactor = 0.15f;  // Lower = smoother
    static const float deadzone = 0.005f;        // Ignore changes < 0.5%
  };

  Pot pots[6];
  unsigned long lastReadTime;
  static const unsigned long readInterval = 50; // Read every 50ms
  int currentPotIndex;

public:
  // Pot indices
  enum PotIndex {
    POT_CUTOFF = 0,
    POT_RESONANCE = 1,
    POT_VOLUME = 2,
    POT_ENV_AMT = 3,
    POT_ATTACK = 4,
    POT_RELEASE = 5
  };

  AnalogPots() : lastReadTime(0), currentPotIndex(0) {
    pots[POT_CUTOFF].pin = POT_CUTOFF_PIN;
    pots[POT_RESONANCE].pin = POT_RESONANCE_PIN;
    pots[POT_VOLUME].pin = POT_VOLUME_PIN;
    pots[POT_ENV_AMT].pin = POT_ENV_PIN;
    pots[POT_ATTACK].pin = POT_ATTACK_PIN;
    pots[POT_RELEASE].pin = POT_RELEASE_PIN;
  }

  void init() {
    // Configure ADC
    analogReadResolution(12);  // 12-bit resolution (0-4095)
    analogSetAttenuation(ADC_11db); // Full 3.3V range

    // Initialize all pots
    for (int i = 0; i < 6; i++) {
      pinMode(pots[i].pin, INPUT);
      pots[i].rawValue = analogRead(pots[i].pin);
      pots[i].smoothedValue = pots[i].rawValue / 4095.0f;
      pots[i].lastOutputValue = pots[i].smoothedValue;
    }

    Serial.println("Analog pots initialized (6 channels)");
  }

  void update() {
    unsigned long now = millis();
    if (now - lastReadTime < readInterval) return;

    lastReadTime = now;

    // Read one pot per update cycle to spread ADC load
    currentPotIndex = (currentPotIndex + 1) % 6;
    Pot& pot = pots[currentPotIndex];

    // Read and smooth
    pot.rawValue = analogRead(pot.pin);
    float newValue = pot.rawValue / 4095.0f;

    // Exponential smoothing
    pot.smoothedValue = pot.smoothedValue * (1.0f - Pot::smoothingFactor) +
                        newValue * Pot::smoothingFactor;
  }

  float getValue(PotIndex index) {
    return pots[index].smoothedValue;
  }

  bool hasChanged(PotIndex index) {
    Pot& pot = pots[index];
    float diff = abs(pot.smoothedValue - pot.lastOutputValue);

    if (diff > Pot::deadzone) {
      pot.lastOutputValue = pot.smoothedValue;
      return true;
    }
    return false;
  }

  int getRawValue(PotIndex index) {
    return pots[index].rawValue;
  }

  // Get value scaled to specific range
  float getScaled(PotIndex index, float min, float max) {
    float normalized = pots[index].smoothedValue;
    return min + normalized * (max - min);
  }

  int getScaledInt(PotIndex index, int min, int max) {
    float normalized = pots[index].smoothedValue;
    return (int)(min + normalized * (max - min));
  }
};

// Rotary encoder class (from zombie_encoder.h but adapted)
class RotaryEncoderDirect {
private:
  int position;
  int lastPosition;
  volatile int encoderCount;
  bool lastA;
  bool lastB;
  unsigned long lastDebounceTime;
  static const unsigned long debounceDelay = 5;

public:
  RotaryEncoderDirect() : position(0), lastPosition(0), encoderCount(0),
                          lastA(false), lastB(false), lastDebounceTime(0) {}

  void init() {
    pinMode(ENC_A_PIN, INPUT_PULLUP);
    pinMode(ENC_B_PIN, INPUT_PULLUP);
    pinMode(ENC_SW_PIN, INPUT_PULLUP);

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

  bool isSwitchPressed() {
    static bool lastSwitchState = HIGH;
    static bool switchPressed = false;

    bool currentState = digitalRead(ENC_SW_PIN);

    if (currentState == LOW && lastSwitchState == HIGH) {
      switchPressed = true;
    } else {
      switchPressed = false;
    }

    lastSwitchState = currentState;
    return switchPressed;
  }

  bool isSwitchHeld() {
    return digitalRead(ENC_SW_PIN) == LOW;
  }
};

#endif
