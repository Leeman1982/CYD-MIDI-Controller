#ifndef MIDI_INPUT_H
#define MIDI_INPUT_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include <USB.h>
#include <USBMIDI.h>

// MIDI input handling for both USB and 5-pin DIN
// 5-pin DIN MIDI uses GPIO 16 (RX2)

#define MIDI_SERIAL_RX 16  // 5-pin DIN MIDI input
#define MIDI_BAUD_RATE 31250

class MIDIInput {
private:
  HardwareSerial* midiSerial;
  USBMIDI usbMIDI;

  // MIDI parser state
  uint8_t midiStatus;
  uint8_t midiData1;
  uint8_t midiData2;
  uint8_t midiDataCount;
  bool runningStatus;

  // Callback function pointers
  void (*noteOnCallback)(uint8_t channel, uint8_t note, uint8_t velocity);
  void (*noteOffCallback)(uint8_t channel, uint8_t note, uint8_t velocity);
  void (*ccCallback)(uint8_t channel, uint8_t cc, uint8_t value);
  void (*pitchBendCallback)(uint8_t channel, int16_t bend);

  void processMIDIByte(uint8_t byte) {
    if (byte >= 0xF8) {
      // Real-time messages - ignore for now
      return;
    }

    if (byte >= 0x80) {
      // Status byte
      midiStatus = byte;
      midiDataCount = 0;
      runningStatus = true;

      // Determine expected data bytes
      uint8_t msgType = midiStatus & 0xF0;
      if (msgType == 0xC0 || msgType == 0xD0) {
        // Program change and channel pressure: 1 data byte
        midiDataCount = 0;
      }
    } else {
      // Data byte
      if (!runningStatus) return;

      if (midiDataCount == 0) {
        midiData1 = byte;
        midiDataCount = 1;

        uint8_t msgType = midiStatus & 0xF0;
        if (msgType == 0xC0 || msgType == 0xD0) {
          // Single data byte messages - process immediately
          midiDataCount = 0;
        }
      } else if (midiDataCount == 1) {
        midiData2 = byte;
        midiDataCount = 0;

        // Process complete message
        processCompleteMessage();
      }
    }
  }

  void processCompleteMessage() {
    uint8_t msgType = midiStatus & 0xF0;
    uint8_t channel = midiStatus & 0x0F;

    switch (msgType) {
      case 0x80: // Note Off
        if (noteOffCallback) {
          noteOffCallback(channel, midiData1, midiData2);
        }
        break;

      case 0x90: // Note On
        if (midiData2 == 0) {
          // Note on with velocity 0 is note off
          if (noteOffCallback) {
            noteOffCallback(channel, midiData1, midiData2);
          }
        } else {
          if (noteOnCallback) {
            noteOnCallback(channel, midiData1, midiData2);
          }
        }
        break;

      case 0xB0: // Control Change
        if (ccCallback) {
          ccCallback(channel, midiData1, midiData2);
        }
        break;

      case 0xE0: // Pitch Bend
        if (pitchBendCallback) {
          int16_t bend = ((int16_t)midiData2 << 7) | midiData1;
          bend -= 8192; // Center at 0
          pitchBendCallback(channel, bend);
        }
        break;
    }
  }

public:
  MIDIInput() {
    midiSerial = NULL;
    midiStatus = 0;
    midiData1 = 0;
    midiData2 = 0;
    midiDataCount = 0;
    runningStatus = false;

    noteOnCallback = NULL;
    noteOffCallback = NULL;
    ccCallback = NULL;
    pitchBendCallback = NULL;
  }

  void init() {
    // Initialize 5-pin DIN MIDI on Serial2
    midiSerial = new HardwareSerial(2);
    midiSerial->begin(MIDI_BAUD_RATE, SERIAL_8N1, MIDI_SERIAL_RX, -1);

    // Initialize USB MIDI
    USB.begin();
    usbMIDI.begin();

    Serial.println("MIDI Input initialized (USB + 5-pin DIN)");
  }

  void update() {
    // Process 5-pin DIN MIDI
    if (midiSerial != NULL) {
      while (midiSerial->available()) {
        uint8_t byte = midiSerial->read();
        processMIDIByte(byte);
      }
    }

    // Process USB MIDI
    midiEvent_t event;
    while (usbMIDI.readEvent(&event)) {
      uint8_t msgType = event.header & 0x0F;
      uint8_t channel = event.data[0] & 0x0F;

      switch (msgType) {
        case 0x08: // Note Off
          if (noteOffCallback) {
            noteOffCallback(channel, event.data[1], event.data[2]);
          }
          break;

        case 0x09: // Note On
          if (event.data[2] == 0) {
            if (noteOffCallback) {
              noteOffCallback(channel, event.data[1], event.data[2]);
            }
          } else {
            if (noteOnCallback) {
              noteOnCallback(channel, event.data[1], event.data[2]);
            }
          }
          break;

        case 0x0B: // Control Change
          if (ccCallback) {
            ccCallback(channel, event.data[1], event.data[2]);
          }
          break;

        case 0x0E: // Pitch Bend
          if (pitchBendCallback) {
            int16_t bend = ((int16_t)event.data[2] << 7) | event.data[1];
            bend -= 8192;
            pitchBendCallback(channel, bend);
          }
          break;
      }
    }
  }

  // Set callbacks
  void setNoteOnCallback(void (*callback)(uint8_t, uint8_t, uint8_t)) {
    noteOnCallback = callback;
  }

  void setNoteOffCallback(void (*callback)(uint8_t, uint8_t, uint8_t)) {
    noteOffCallback = callback;
  }

  void setCCCallback(void (*callback)(uint8_t, uint8_t, uint8_t)) {
    ccCallback = callback;
  }

  void setPitchBendCallback(void (*callback)(uint8_t, int16_t)) {
    pitchBendCallback = callback;
  }
};

#endif
