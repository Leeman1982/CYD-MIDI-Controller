#ifndef MIDI_HANDLER_H
#define MIDI_HANDLER_H

#include <Arduino.h>
#include "config.h"

// ── MIDI Input / Output — RP2040 UART ─────────────────────────────────────────
// MIDI IN:  UART0 RX on GPIO 1 (31250 baud)
// MIDI OUT: UART0 TX on GPIO 0 (31250 baud)

class MIDIHandler {
private:
  // Parser state
  uint8_t midiStatus;
  uint8_t midiData1;
  uint8_t midiData2;
  uint8_t midiDataCount;
  bool    runningStatus;

  // Clock sync
  uint32_t lastClockMicros;
  uint32_t clockIntervalMicros;
  bool     clockRunning;

  // Callbacks
  void (*noteOnCB)    (uint8_t ch, uint8_t note, uint8_t vel);
  void (*noteOffCB)   (uint8_t ch, uint8_t note, uint8_t vel);
  void (*ccCB)        (uint8_t ch, uint8_t cc,   uint8_t val);
  void (*pitchBendCB) (uint8_t ch, int16_t bend);
  void (*aftertouchCB)(uint8_t ch, uint8_t val);
  void (*polyATCB)    (uint8_t ch, uint8_t note, uint8_t val);
  void (*clockCB)     ();
  void (*startCB)     ();
  void (*stopCB)      ();
  void (*continueCB)  ();

  void processByte(uint8_t b) {
    // Real-time messages (interleaved, no running status reset)
    if (b >= 0xF8) {
      switch (b) {
        case 0xF8: { // Clock
          uint32_t now = micros();
          uint32_t interval = now - lastClockMicros;
          if (lastClockMicros && interval < 500000u)
            clockIntervalMicros = (clockIntervalMicros * 7 + interval) >> 3;
          lastClockMicros = now;
          if (clockCB) clockCB();
          break;
        }
        case 0xFA: clockRunning = true;  if (startCB)    startCB();    break;
        case 0xFB:                        if (continueCB) continueCB(); break;
        case 0xFC: clockRunning = false; if (stopCB)     stopCB();     break;
      }
      return;
    }

    // System messages — absorb SysEx
    if (b >= 0xF0) { midiStatus = 0; runningStatus = false; return; }

    // Status bytes
    if (b >= 0x80) { midiStatus = b; midiDataCount = 0; runningStatus = true; return; }

    // Data bytes
    if (!runningStatus) return;

    if (midiDataCount == 0) {
      midiData1 = b;
      uint8_t msgType = midiStatus & 0xF0;
      if (msgType == 0xC0 || msgType == 0xD0) {
        processComplete();
        midiDataCount = 0;
      } else {
        midiDataCount = 1;
      }
    } else {
      midiData2 = b;
      midiDataCount = 0;
      processComplete();
    }
  }

  void processComplete() {
    uint8_t msgType = midiStatus & 0xF0;
    uint8_t channel = midiStatus & 0x0F;

    switch (msgType) {
      case 0x80:
        if (noteOffCB) noteOffCB(channel, midiData1, midiData2);
        break;
      case 0x90:
        if (midiData2 == 0) { if (noteOffCB) noteOffCB(channel, midiData1, 0); }
        else                { if (noteOnCB)  noteOnCB(channel, midiData1, midiData2); }
        break;
      case 0xA0:
        if (polyATCB) polyATCB(channel, midiData1, midiData2);
        break;
      case 0xB0:
        if (ccCB) ccCB(channel, midiData1, midiData2);
        break;
      case 0xD0:
        if (aftertouchCB) aftertouchCB(channel, midiData1);
        break;
      case 0xE0: {
        int16_t bend = ((int16_t)midiData2 << 7) | midiData1;
        bend -= 8192;
        if (pitchBendCB) pitchBendCB(channel, bend);
        break;
      }
    }
  }

public:
  MIDIHandler() {
    midiStatus = 0; midiData1 = 0; midiData2 = 0;
    midiDataCount = 0; runningStatus = false;
    lastClockMicros = 0; clockIntervalMicros = 0; clockRunning = false;
    noteOnCB = nullptr; noteOffCB = nullptr; ccCB = nullptr;
    pitchBendCB = nullptr; aftertouchCB = nullptr; polyATCB = nullptr;
    clockCB = nullptr; startCB = nullptr; stopCB = nullptr; continueCB = nullptr;
  }

  void init() {
    // Configure Serial1 (UART0) for MIDI
    Serial1.setRX(MIDI_RX_PIN);
    Serial1.setTX(MIDI_TX_PIN);
    Serial1.begin(MIDI_BAUD);
  }

  void update() {
    while (Serial1.available()) {
      processByte((uint8_t)Serial1.read());
    }
  }

  // ── BPM from clock ────────────────────────────────────────────────────────
  float getClockBPM() {
    if (!clockRunning || clockIntervalMicros == 0) return 0.0f;
    if (micros() - lastClockMicros > 2000000u) return 0.0f;
    return 60000000.0f / ((float)clockIntervalMicros * 24.0f);
  }
  bool isClockRunning() { return clockRunning; }

  // ── MIDI OUT ──────────────────────────────────────────────────────────────
  void sendNoteOn(uint8_t ch, uint8_t note, uint8_t vel) {
    Serial1.write(0x90 | (ch & 0x0F));
    Serial1.write(note & 0x7F);
    Serial1.write(vel & 0x7F);
  }
  void sendNoteOff(uint8_t ch, uint8_t note) {
    Serial1.write(0x80 | (ch & 0x0F));
    Serial1.write(note & 0x7F);
    Serial1.write((uint8_t)0x00);
  }
  void sendCC(uint8_t ch, uint8_t cc, uint8_t val) {
    Serial1.write(0xB0 | (ch & 0x0F));
    Serial1.write(cc & 0x7F);
    Serial1.write(val & 0x7F);
  }
  void sendAllNotesOff(uint8_t ch) { sendCC(ch, 123, 0); }
  void sendClockTick() { Serial1.write(0xF8); }
  void sendStart()     { Serial1.write(0xFA); }
  void sendStop()      { Serial1.write(0xFC); }

  // ── Callback setters ──────────────────────────────────────────────────────
  void setNoteOnCallback     (void (*cb)(uint8_t, uint8_t, uint8_t)) { noteOnCB     = cb; }
  void setNoteOffCallback    (void (*cb)(uint8_t, uint8_t, uint8_t)) { noteOffCB    = cb; }
  void setCCCallback         (void (*cb)(uint8_t, uint8_t, uint8_t)) { ccCB         = cb; }
  void setPitchBendCallback  (void (*cb)(uint8_t, int16_t))          { pitchBendCB  = cb; }
  void setAftertouchCallback (void (*cb)(uint8_t, uint8_t))          { aftertouchCB = cb; }
  void setPolyATCallback     (void (*cb)(uint8_t, uint8_t, uint8_t)) { polyATCB     = cb; }
  void setClockCallback      (void (*cb)())                           { clockCB      = cb; }
  void setStartCallback      (void (*cb)())                           { startCB      = cb; }
  void setStopCallback       (void (*cb)())                           { stopCB       = cb; }
  void setContinueCallback   (void (*cb)())                           { continueCB   = cb; }
};

#endif
