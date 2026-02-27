#ifndef MIDI_INPUT_H
#define MIDI_INPUT_H

#include <Arduino.h>
#include <HardwareSerial.h>

// MIDI input handling for 5-pin DIN on GPIO 35
// MIDI OUT TX on GPIO 4 (UART1) — see midi_output.h
#define MIDI_SERIAL_RX 35
#define MIDI_BAUD_RATE 31250

class MIDIInput {
private:
  HardwareSerial* midiSerial;

  // MIDI parser state
  uint8_t midiStatus;
  uint8_t midiData1;
  uint8_t midiData2;
  uint8_t midiDataCount;
  bool    runningStatus;

  // MIDI clock sync state
  uint32_t lastClockMicros;
  uint32_t clockIntervalMicros;  // smoothed interval between 0xF8 ticks
  bool     clockRunning;

  // Callbacks
  void (*noteOnCallback)      (uint8_t ch, uint8_t note, uint8_t vel);
  void (*noteOffCallback)     (uint8_t ch, uint8_t note, uint8_t vel);
  void (*ccCallback)          (uint8_t ch, uint8_t cc,   uint8_t val);
  void (*pitchBendCallback)   (uint8_t ch, int16_t bend);
  void (*aftertouchCallback)  (uint8_t ch, uint8_t val); // channel AT (0xD0)
  void (*polyATCallback)      (uint8_t ch, uint8_t note, uint8_t val); // poly AT (0xA0)
  void (*clockCallback)       ();                         // 0xF8 tick
  void (*startCallback)       ();                         // 0xFA
  void (*stopCallback)        ();                         // 0xFC
  void (*continueCallback)    ();                         // 0xFB

  void processMIDIByte(uint8_t b) {
    // ── Real-time messages (interleaved, no running status reset) ────────────
    if (b >= 0xF8) {
      switch (b) {
        case 0xF8: // Clock
          {
            uint32_t now = micros();
            uint32_t interval = now - lastClockMicros;
            // Smooth with 1-pole IIR to reject jitter
            if (lastClockMicros && interval < 500000u) {
              clockIntervalMicros = (clockIntervalMicros * 7 + interval) >> 3;
            }
            lastClockMicros = now;
          }
          if (clockCallback) clockCallback();
          break;
        case 0xFA: clockRunning = true;  if (startCallback)    startCallback();    break;
        case 0xFB:                        if (continueCallback) continueCallback(); break;
        case 0xFC: clockRunning = false; if (stopCallback)     stopCallback();     break;
      }
      return;
    }

    // ── System messages (0xF0-0xF7) — absorb SysEx, ignore others ───────────
    if (b >= 0xF0) {
      midiStatus = 0;
      runningStatus = false;
      return;
    }

    // ── Status bytes ─────────────────────────────────────────────────────────
    if (b >= 0x80) {
      midiStatus    = b;
      midiDataCount = 0;
      runningStatus = true;
      return;
    }

    // ── Data bytes ───────────────────────────────────────────────────────────
    if (!runningStatus) return;

    if (midiDataCount == 0) {
      midiData1 = b;
      uint8_t msgType = midiStatus & 0xF0;
      if (msgType == 0xC0 || msgType == 0xD0) {
        // Single data byte — process now
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
      case 0x80: // Note Off
        if (noteOffCallback) noteOffCallback(channel, midiData1, midiData2);
        break;

      case 0x90: // Note On (vel=0 treated as note off)
        if (midiData2 == 0) {
          if (noteOffCallback) noteOffCallback(channel, midiData1, 0);
        } else {
          if (noteOnCallback) noteOnCallback(channel, midiData1, midiData2);
        }
        break;

      case 0xA0: // Polyphonic Aftertouch
        if (polyATCallback) polyATCallback(channel, midiData1, midiData2);
        break;

      case 0xB0: // Control Change
        if (ccCallback) ccCallback(channel, midiData1, midiData2);
        break;

      case 0xD0: // Channel Aftertouch (single byte)
        if (aftertouchCallback) aftertouchCallback(channel, midiData1);
        break;

      case 0xE0: // Pitch Bend
        {
          int16_t bend = ((int16_t)midiData2 << 7) | midiData1;
          bend -= 8192;
          if (pitchBendCallback) pitchBendCallback(channel, bend);
        }
        break;
    }
  }

public:
  MIDIInput() {
    midiSerial          = nullptr;
    midiStatus          = 0;
    midiData1           = 0;
    midiData2           = 0;
    midiDataCount       = 0;
    runningStatus       = false;
    lastClockMicros     = 0;
    clockIntervalMicros = 0;
    clockRunning        = false;

    noteOnCallback      = nullptr;
    noteOffCallback     = nullptr;
    ccCallback          = nullptr;
    pitchBendCallback   = nullptr;
    aftertouchCallback  = nullptr;
    polyATCallback      = nullptr;
    clockCallback       = nullptr;
    startCallback       = nullptr;
    stopCallback        = nullptr;
    continueCallback    = nullptr;
  }

  void init() {
    midiSerial = new HardwareSerial(2);
    midiSerial->begin(MIDI_BAUD_RATE, SERIAL_8N1, MIDI_SERIAL_RX, -1);
    Serial.println("MIDI Input: 5-pin DIN on GPIO 35");
  }

  void update() {
    if (!midiSerial) return;
    while (midiSerial->available()) {
      processMIDIByte((uint8_t)midiSerial->read());
    }
  }

  // ── BPM derived from clock ticks ─────────────────────────────────────────
  // Returns 0 if no clock received or clock stale (>2s)
  float getClockBPM() {
    if (!clockRunning || clockIntervalMicros == 0) return 0.0f;
    if (micros() - lastClockMicros > 2000000u) return 0.0f; // stale
    // 24 PPQN: BPM = 60e6 / (interval_us * 24)
    return 60000000.0f / ((float)clockIntervalMicros * 24.0f);
  }

  bool isClockRunning() { return clockRunning; }

  // ── Callback setters ─────────────────────────────────────────────────────
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
