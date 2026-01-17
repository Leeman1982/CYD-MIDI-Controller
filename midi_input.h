#ifndef MIDI_INPUT_H
#define MIDI_INPUT_H

#include <Arduino.h>
#include "synth_config.h"
#include "synth_engine.h"

// ============================================================================
// Hardware MIDI Input (5-pin DIN)
// ============================================================================
// Handles MIDI input via UART2
// Standard MIDI baud rate: 31250
// ============================================================================

class MIDIInput {
private:
  SynthEngine* synthEngine;
  uint8_t midiChannel;

  // MIDI message parsing
  uint8_t midiStatus;
  uint8_t midiData1;
  uint8_t midiData2;
  uint8_t dataIndex;
  bool runningStatus;

public:
  MIDIInput(SynthEngine* synth, uint8_t channel = 0) :
    synthEngine(synth),
    midiChannel(channel),
    midiStatus(0),
    midiData1(0),
    midiData2(0),
    dataIndex(0),
    runningStatus(false) {}

  void init() {
    // Initialize UART2 for MIDI
    MIDI_SERIAL.begin(MIDI_BAUD_RATE, SERIAL_8N1, MIDI_RX_PIN, MIDI_TX_PIN);
    Serial.println("Hardware MIDI input initialized on UART2");
    Serial.printf("MIDI RX: GPIO %d, TX: GPIO %d\n", MIDI_RX_PIN, MIDI_TX_PIN);
  }

  void setChannel(uint8_t channel) {
    midiChannel = channel; // 0 = omni, 1-16 = specific channel
  }

  uint8_t getChannel() const {
    return midiChannel;
  }

  void process() {
    while (MIDI_SERIAL.available()) {
      uint8_t byte = MIDI_SERIAL.read();

      // Status byte (MSB set)
      if (byte & 0x80) {
        midiStatus = byte;
        dataIndex = 0;
        runningStatus = true;

        // System Real-Time messages (single byte)
        if (midiStatus >= 0xF8) {
          handleSystemRealTime(midiStatus);
          continue;
        }

        // System Common messages
        if (midiStatus >= 0xF0) {
          runningStatus = false;
          // We don't handle SysEx for now
          continue;
        }

      } else {
        // Data byte
        if (!runningStatus) continue;

        if (dataIndex == 0) {
          midiData1 = byte;
          dataIndex = 1;

          // Program Change and Channel Pressure are 2-byte messages
          if ((midiStatus & 0xF0) == 0xC0 || (midiStatus & 0xF0) == 0xD0) {
            handleMIDIMessage();
            dataIndex = 0;
          }

        } else if (dataIndex == 1) {
          midiData2 = byte;
          handleMIDIMessage();
          dataIndex = 0;
        }
      }
    }
  }

private:
  void handleMIDIMessage() {
    uint8_t messageType = midiStatus & 0xF0;
    uint8_t channel = (midiStatus & 0x0F) + 1; // 1-16

    // Check channel (0 = omni mode)
    if (midiChannel != 0 && channel != midiChannel) {
      return;
    }

    switch (messageType) {
      case 0x80: // Note Off
        handleNoteOff(midiData1, midiData2);
        break;

      case 0x90: // Note On
        if (midiData2 == 0) {
          handleNoteOff(midiData1, 0);
        } else {
          handleNoteOn(midiData1, midiData2);
        }
        break;

      case 0xB0: // Control Change
        handleControlChange(midiData1, midiData2);
        break;

      case 0xC0: // Program Change
        handleProgramChange(midiData1);
        break;

      case 0xE0: // Pitch Bend
        handlePitchBend((midiData2 << 7) | midiData1);
        break;
    }
  }

  void handleNoteOn(uint8_t note, uint8_t velocity) {
    synthEngine->noteOn(note, velocity);
  }

  void handleNoteOff(uint8_t note, uint8_t velocity) {
    synthEngine->noteOff(note);
  }

  void handleControlChange(uint8_t cc, uint8_t value) {
    float normalizedValue = value / 127.0f;

    // Map common MIDI CCs to synth parameters
    switch (cc) {
      case 1: // Modulation Wheel
        synthEngine->setLFOAmount(normalizedValue);
        break;

      case 7: // Volume
        synthEngine->setMasterVolume(normalizedValue);
        break;

      case 71: // Filter Resonance (Timbre/Harmonic Content)
        synthEngine->setFilterResonance(normalizedValue * 10.0f);
        break;

      case 74: // Filter Cutoff (Brightness)
        synthEngine->setFilterCutoff(20.0f + normalizedValue * 10000.0f);
        break;

      case 73: // Attack Time
        synthEngine->setAmpAttack(0.001f + normalizedValue * 2.0f);
        break;

      case 75: // Decay Time
        synthEngine->setAmpDecay(0.001f + normalizedValue * 2.0f);
        break;

      case 70: // Sustain Level
        synthEngine->setAmpSustain(normalizedValue);
        break;

      case 72: // Release Time
        synthEngine->setAmpRelease(0.001f + normalizedValue * 3.0f);
        break;

      case 123: // All Notes Off
        synthEngine->allNotesOff();
        break;
    }
  }

  void handleProgramChange(uint8_t program) {
    // Could use this to select presets in the future
    Serial.printf("Program change: %d\n", program);
  }

  void handlePitchBend(uint16_t value) {
    // Pitch bend implementation (optional)
    // Could modulate oscillator pitch globally
  }

  void handleSystemRealTime(uint8_t byte) {
    switch (byte) {
      case 0xFA: // Start
        Serial.println("MIDI Start");
        break;
      case 0xFB: // Continue
        Serial.println("MIDI Continue");
        break;
      case 0xFC: // Stop
        Serial.println("MIDI Stop");
        synthEngine->allNotesOff();
        break;
      case 0xF8: // Timing Clock
        // Could sync arpeggiator to external clock
        break;
    }
  }
};

#endif // MIDI_INPUT_H
