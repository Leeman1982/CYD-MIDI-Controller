#ifndef MIDI_OUTPUT_H
#define MIDI_OUTPUT_H

#include <Arduino.h>
#include <HardwareSerial.h>

// MIDI OUT via GPIO 4 using UART1 (MIDI IN uses UART2 on GPIO 35)
// Connect GPIO 4 → MIDI OUT circuit (220Ω + optocoupler or direct 3.5mm TRS)
#define MIDI_TX_PIN 4

class MIDIOutput {
  HardwareSerial* _serial;
  bool _active;

public:
  MIDIOutput() : _serial(NULL), _active(false) {}

  void init() {
    _serial = new HardwareSerial(1);  // UART1
    _serial->begin(31250, SERIAL_8N1, -1, MIDI_TX_PIN);
    _active = true;
    Serial.printf("MIDI OUT init on GPIO %d (UART1)\n", MIDI_TX_PIN);
  }

  bool isActive() { return _active; }

  void noteOn(uint8_t ch, uint8_t note, uint8_t vel) {
    if (!_active || !_serial) return;
    _serial->write(0x90 | (ch & 0x0F));
    _serial->write(note & 0x7F);
    _serial->write(vel & 0x7F);
  }

  void noteOff(uint8_t ch, uint8_t note) {
    if (!_active || !_serial) return;
    _serial->write(0x80 | (ch & 0x0F));
    _serial->write(note & 0x7F);
    _serial->write(0x00);
  }

  void cc(uint8_t ch, uint8_t ccNum, uint8_t val) {
    if (!_active || !_serial) return;
    _serial->write(0xB0 | (ch & 0x0F));
    _serial->write(ccNum & 0x7F);
    _serial->write(val & 0x7F);
  }

  void pitchBend(uint8_t ch, int16_t bend) {
    if (!_active || !_serial) return;
    uint16_t pb = (uint16_t)(bend + 8192);
    _serial->write(0xE0 | (ch & 0x0F));
    _serial->write(pb & 0x7F);
    _serial->write((pb >> 7) & 0x7F);
  }

  void allNotesOff(uint8_t ch) {
    cc(ch, 123, 0);
  }

  void programChange(uint8_t ch, uint8_t prog) {
    if (!_active || !_serial) return;
    _serial->write(0xC0 | (ch & 0x0F));
    _serial->write(prog & 0x7F);
  }

  void clockTick() {
    if (!_active || !_serial) return;
    _serial->write(0xF8);
  }

  void start() {
    if (!_active || !_serial) return;
    _serial->write(0xFA);
  }

  void stop() {
    if (!_active || !_serial) return;
    _serial->write(0xFC);
  }
};

// Forward declaration — defined in ZombieSynth.ino
extern MIDIOutput midiOut;

// Output routing modes
#define SEQ_OUT_INTERNAL 0
#define SEQ_OUT_EXTERNAL 1
#define SEQ_OUT_BOTH     2
static const char* seqOutNames[] = {"INT", "EXT", "BOTH"};

#endif
