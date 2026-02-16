#ifndef MIDI_HANDLER_H
#define MIDI_HANDLER_H

// ═══════════════════════════════════════════════════════════════════════════════
// ZOMBIE SS – MIDI Input/Output Handler (RP2040 Port)
// ═══════════════════════════════════════════════════════════════════════════════
//
// Combined MIDI IN + OUT using Serial1 (UART0).
// MIDI IN:  GPIO 1 (RX) – 5-pin DIN via optocoupler
// MIDI OUT: GPIO 0 (TX) – 5-pin DIN via buffer circuit
//
// Changes from ESP32:
//   - Uses Serial1 (UART0) instead of HardwareSerial(1)/HardwareSerial(2)
//   - Single UART for both TX and RX (full duplex)
//   - No heap allocation
// ═══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>
#include "config.h"

// ── MIDI Output ─────────────────────────────────────────────────────────────
class MIDIOutput {
  bool _active;

public:
  MIDIOutput() : _active(false) {}

  void init() {
    // Serial1 is already initialized by MIDIInput::init()
    _active = true;
  }

  bool isActive() { return _active; }

  void noteOn(uint8_t ch, uint8_t note, uint8_t vel) {
    if (!_active) return;
    Serial1.write(0x90 | (ch & 0x0F));
    Serial1.write(note & 0x7F);
    Serial1.write(vel & 0x7F);
  }

  void noteOff(uint8_t ch, uint8_t note) {
    if (!_active) return;
    Serial1.write(0x80 | (ch & 0x0F));
    Serial1.write(note & 0x7F);
    Serial1.write((uint8_t)0x00);
  }

  void cc(uint8_t ch, uint8_t ccNum, uint8_t val) {
    if (!_active) return;
    Serial1.write(0xB0 | (ch & 0x0F));
    Serial1.write(ccNum & 0x7F);
    Serial1.write(val & 0x7F);
  }

  void pitchBend(uint8_t ch, int16_t bend) {
    if (!_active) return;
    uint16_t pb = (uint16_t)(bend + 8192);
    Serial1.write(0xE0 | (ch & 0x0F));
    Serial1.write(pb & 0x7F);
    Serial1.write((pb >> 7) & 0x7F);
  }

  void allNotesOff(uint8_t ch) { cc(ch, 123, 0); }

  void programChange(uint8_t ch, uint8_t prog) {
    if (!_active) return;
    Serial1.write(0xC0 | (ch & 0x0F));
    Serial1.write(prog & 0x7F);
  }

  void clockTick() { if (_active) Serial1.write(0xF8); }
  void start()     { if (_active) Serial1.write(0xFA); }
  void stop()      { if (_active) Serial1.write(0xFC); }
};

// Output routing modes (used by sequencer)
#define SEQ_OUT_INTERNAL 0
#define SEQ_OUT_EXTERNAL 1
#define SEQ_OUT_BOTH     2
static const char* seqOutNames[] = {"INT", "EXT", "BOTH"};

// ── MIDI Input ──────────────────────────────────────────────────────────────
class MIDIInput {
private:
  uint8_t midiStatus;
  uint8_t midiData1;
  uint8_t midiData2;
  uint8_t midiDataCount;
  bool    runningStatus;

  uint32_t lastClockMicros;
  uint32_t clockIntervalMicros;
  bool     clockRunning;

  // Callbacks
  void (*noteOnCallback)      (uint8_t ch, uint8_t note, uint8_t vel);
  void (*noteOffCallback)     (uint8_t ch, uint8_t note, uint8_t vel);
  void (*ccCallback)          (uint8_t ch, uint8_t cc,   uint8_t val);
  void (*pitchBendCallback)   (uint8_t ch, int16_t bend);
  void (*aftertouchCallback)  (uint8_t ch, uint8_t val);
  void (*polyATCallback)      (uint8_t ch, uint8_t note, uint8_t val);
  void (*clockCallback)       ();
  void (*startCallback)       ();
  void (*stopCallback)        ();
  void (*continueCallback)    ();

  void processMIDIByte(uint8_t b) {
    // Real-time messages (no running status reset)
    if (b >= 0xF8) {
      switch (b) {
        case 0xF8: {
          uint32_t now = micros();
          uint32_t interval = now - lastClockMicros;
          if (lastClockMicros && interval < 500000u)
            clockIntervalMicros = (clockIntervalMicros * 7 + interval) >> 3;
          lastClockMicros = now;
          if (clockCallback) clockCallback();
          break;
        }
        case 0xFA: clockRunning = true;  if (startCallback)    startCallback();    break;
        case 0xFB:                        if (continueCallback) continueCallback(); break;
        case 0xFC: clockRunning = false; if (stopCallback)     stopCallback();     break;
      }
      return;
    }

    // System messages (0xF0-0xF7) — absorb SysEx
    if (b >= 0xF0) {
      midiStatus = 0;
      runningStatus = false;
      return;
    }

    // Status bytes
    if (b >= 0x80) {
      midiStatus = b;
      midiDataCount = 0;
      runningStatus = true;
      return;
    }

    // Data bytes
    if (!runningStatus) return;

    if (midiDataCount == 0) {
      midiData1 = b;
      uint8_t msgType = midiStatus & 0xF0;
      if (msgType == 0xC0 || msgType == 0xD0) {
        processCompleteMessage();
        midiDataCount = 0;
      } else {
        midiDataCount = 1;
      }
    } else {
      midiData2 = b;
      midiDataCount = 0;
      processCompleteMessage();
    }
  }

  void processCompleteMessage() {
    uint8_t msgType = midiStatus & 0xF0;
    uint8_t channel = midiStatus & 0x0F;

    switch (msgType) {
      case 0x80:
        if (noteOffCallback) noteOffCallback(channel, midiData1, midiData2);
        break;
      case 0x90:
        if (midiData2 == 0) {
          if (noteOffCallback) noteOffCallback(channel, midiData1, 0);
        } else {
          if (noteOnCallback) noteOnCallback(channel, midiData1, midiData2);
        }
        break;
      case 0xA0:
        if (polyATCallback) polyATCallback(channel, midiData1, midiData2);
        break;
      case 0xB0:
        if (ccCallback) ccCallback(channel, midiData1, midiData2);
        break;
      case 0xD0:
        if (aftertouchCallback) aftertouchCallback(channel, midiData1);
        break;
      case 0xE0: {
        int16_t bend = ((int16_t)midiData2 << 7) | midiData1;
        bend -= 8192;
        if (pitchBendCallback) pitchBendCallback(channel, bend);
        break;
      }
    }
  }

public:
  MIDIInput() {
    midiStatus = 0; midiData1 = 0; midiData2 = 0;
    midiDataCount = 0; runningStatus = false;
    lastClockMicros = 0; clockIntervalMicros = 0; clockRunning = false;
    noteOnCallback = nullptr; noteOffCallback = nullptr;
    ccCallback = nullptr; pitchBendCallback = nullptr;
    aftertouchCallback = nullptr; polyATCallback = nullptr;
    clockCallback = nullptr; startCallback = nullptr;
    stopCallback = nullptr; continueCallback = nullptr;
  }

  void init() {
    Serial1.setRX(MIDI_RX_PIN);
    Serial1.setTX(MIDI_TX_PIN);
    Serial1.begin(MIDI_BAUD);
  }

  void update() {
    while (Serial1.available()) {
      processMIDIByte((uint8_t)Serial1.read());
    }
  }

  float getClockBPM() {
    if (!clockRunning || clockIntervalMicros == 0) return 0.0f;
    if (micros() - lastClockMicros > 2000000u) return 0.0f;
    return 60000000.0f / ((float)clockIntervalMicros * 24.0f);
  }

  bool isClockRunning() { return clockRunning; }

  // Callback setters
  void setNoteOnCallback      (void (*cb)(uint8_t,uint8_t,uint8_t))  { noteOnCallback      = cb; }
  void setNoteOffCallback     (void (*cb)(uint8_t,uint8_t,uint8_t))  { noteOffCallback     = cb; }
  void setCCCallback          (void (*cb)(uint8_t,uint8_t,uint8_t))  { ccCallback          = cb; }
  void setPitchBendCallback   (void (*cb)(uint8_t,int16_t))          { pitchBendCallback   = cb; }
  void setAftertouchCallback  (void (*cb)(uint8_t,uint8_t))          { aftertouchCallback  = cb; }
  void setPolyATCallback      (void (*cb)(uint8_t,uint8_t,uint8_t))  { polyATCallback      = cb; }
  void setClockCallback       (void (*cb)())                          { clockCallback       = cb; }
  void setStartCallback       (void (*cb)())                          { startCallback       = cb; }
  void setStopCallback        (void (*cb)())                          { stopCallback        = cb; }
  void setContinueCallback    (void (*cb)())                          { continueCallback    = cb; }
};

#endif
