#ifndef ARPEGGIATOR_PATTERNS_H
#define ARPEGGIATOR_PATTERNS_H

// ═══════════════════════════════════════════════════════════════════════════════
// ZOMBIE SS – 50-Pattern Arpeggiator (RP2040 Port)
// Prophet-8 inspired, identical logic to ESP32 version.
// ═══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>
#include "config.h"

#define MAX_ARP_NOTES    16
#define NUM_ARP_PATTERNS 50

enum ArpPattern {
  ARP_UP, ARP_DOWN, ARP_UP_DOWN, ARP_DOWN_UP,
  ARP_UP_DOWN_INC, ARP_DOWN_UP_INC, ARP_RANDOM, ARP_ORDER_PLAYED,
  ARP_CONVERGE, ARP_DIVERGE, ARP_THIRDS_UP, ARP_THIRDS_DOWN,
  ARP_FOURTHS_UP, ARP_FOURTHS_DOWN, ARP_OCTAVES,
  ARP_REPEAT_2X, ARP_REPEAT_3X, ARP_PENDULUM,
  ARP_PING_PONG_2, ARP_PING_PONG_3,
  ARP_STEP_2, ARP_STEP_3, ARP_ZIGZAG, ARP_FIBONACCI,
  ARP_SPIRAL_UP, ARP_SPIRAL_DOWN, ARP_BOUNCE, ARP_STUTTER,
  ARP_GATES, ARP_TRIPLETS, ARP_DOTTED, ARP_SWING,
  ARP_EUCLIDEAN_5_8, ARP_EUCLIDEAN_7_12,
  ARP_POLYRHYTHM_3_4, ARP_POLYRHYTHM_5_4,
  ARP_CHORD_INVERSION, ARP_ALTERNATING, ARP_PYRAMID, ARP_CASCADE,
  ARP_SINE_WAVE, ARP_TRIANGLE_WAVE, ARP_SQUARE_WAVE, ARP_SAW_WAVE,
  ARP_PROBABILISTIC, ARP_FRACTAL, ARP_CHAOS,
  ARP_RHYTHMIC_1, ARP_RHYTHMIC_2, ARP_CLASSIC_808
};

static const char* arpPatternNames[NUM_ARP_PATTERNS] = {
  "Up", "Down", "Up/Down", "Down/Up",
  "Up/Dn Inc", "Dn/Up Inc", "Random", "As Played",
  "Converge", "Diverge", "Thirds Up", "Thirds Dn",
  "4ths Up", "4ths Dn", "Octaves", "Repeat 2x",
  "Repeat 3x", "Pendulum", "PingPong2", "PingPong3",
  "Step 2", "Step 3", "Zigzag", "Fibonacci",
  "Spiral Up", "Spiral Dn", "Bounce", "Stutter",
  "Gates", "Triplets", "Dotted", "Swing",
  "Euclid 5/8", "Euclid 7/12", "Poly 3/4", "Poly 5/4",
  "Inversions", "Alternate", "Pyramid", "Cascade",
  "Sine Wave", "Triangle", "Square", "Sawtooth",
  "Probabilis", "Fractal", "Chaos", "Rhythmic 1",
  "Rhythmic 2", "808 Style"
};

class Arpeggiator {
private:
  int heldNotes[MAX_ARP_NOTES];
  int noteCount;
  int currentStep;
  int currentOctave;
  int maxOctaves;
  unsigned long lastStepTime;
  float bpm;
  ArpPattern pattern;
  bool isPlaying;
  int gateLength;

  int repeatCounter;
  int direction;
  int patternPhase;

  bool euclideanRhythm[16];
  int  euclideanPos;

  void generateEuclideanRhythm(int steps, int pulses) {
    for (int i = 0; i < 16; i++) euclideanRhythm[i] = false;
    if (pulses == 0 || steps == 0) return;
    int bucket = 0;
    for (int i = 0; i < steps; i++) {
      bucket += pulses;
      if (bucket >= steps) {
        bucket -= steps;
        if (i < 16) euclideanRhythm[i] = true;
      }
    }
  }

  int getNextNoteIndex() {
    if (noteCount == 0) return -1;
    int idx = 0;

    switch (pattern) {
      case ARP_UP:
        idx = currentStep % noteCount;
        break;
      case ARP_DOWN:
        idx = noteCount - 1 - (currentStep % noteCount);
        break;
      case ARP_UP_DOWN: {
        int cycle = (noteCount - 1) * 2;
        if (cycle <= 0) cycle = 1;
        int pos = currentStep % cycle;
        idx = (pos < noteCount) ? pos : (cycle - pos);
        break;
      }
      case ARP_DOWN_UP: {
        int cycle = (noteCount - 1) * 2;
        if (cycle <= 0) cycle = 1;
        int pos = currentStep % cycle;
        idx = (pos < noteCount) ? (noteCount - 1 - pos) : (pos - noteCount + 1);
        break;
      }
      case ARP_UP_DOWN_INC: {
        int cycle = noteCount * 2;
        int pos = currentStep % cycle;
        idx = (pos < noteCount) ? pos : (cycle - pos - 1);
        break;
      }
      case ARP_RANDOM:
        idx = random(noteCount);
        break;
      case ARP_ORDER_PLAYED:
        idx = currentStep % noteCount;
        break;
      case ARP_CONVERGE: {
        int half = noteCount / 2;
        if (half <= 0) half = 1;
        if ((currentStep / half) % 2 == 0)
          idx = currentStep % half;
        else
          idx = noteCount - 1 - (currentStep % half);
        break;
      }
      case ARP_DIVERGE: {
        int half = noteCount / 2;
        int pos = currentStep % noteCount;
        idx = (pos % 2 == 0) ? (half + pos / 2) : (half - 1 - pos / 2);
        break;
      }
      case ARP_THIRDS_UP:
        idx = ((currentStep % noteCount) + (currentStep / noteCount) * 2) % noteCount;
        break;
      case ARP_FOURTHS_UP:
        idx = ((currentStep % noteCount) + (currentStep / noteCount) * 3) % noteCount;
        break;
      case ARP_OCTAVES:
        idx = currentStep % noteCount;
        currentOctave = (currentStep / noteCount) % maxOctaves;
        break;
      case ARP_REPEAT_2X:
        idx = (currentStep / 2) % noteCount;
        break;
      case ARP_REPEAT_3X:
        idx = (currentStep / 3) % noteCount;
        break;
      case ARP_PENDULUM:
        if (currentStep % 2 == 0) idx = 0;
        else idx = min((currentStep / 2) + 1, noteCount - 1);
        break;
      case ARP_ZIGZAG: {
        int base = currentStep / 2;
        idx = (currentStep % 2 == 0) ? base : (base + noteCount / 2);
        idx = idx % noteCount;
        break;
      }
      case ARP_BOUNCE:
        idx = abs((currentStep % (noteCount * 2)) - noteCount);
        break;
      case ARP_STUTTER:
        idx = (currentStep / 2) % noteCount;
        break;
      case ARP_ALTERNATING:
        if (noteCount < 2) idx = 0;
        else idx = (currentStep % 2 == 0) ? 0 : (noteCount - 1);
        break;
      case ARP_PYRAMID: {
        int level = (currentStep / noteCount) % (noteCount + 1);
        if (level == 0) level = 1;
        idx = currentStep % level;
        break;
      }
      case ARP_SINE_WAVE: {
        float ph = (currentStep * TWO_PI_F) / noteCount;
        idx = (int)((sinf(ph) + 1.0f) * (noteCount - 1) / 2.0f);
        break;
      }
      case ARP_PROBABILISTIC:
        idx = (random(100) < 70) ? (currentStep % noteCount) : random(noteCount);
        break;
      default:
        idx = currentStep % noteCount;
        break;
    }

    return constrain(idx, 0, noteCount - 1);
  }

public:
  Arpeggiator() {
    noteCount = 0; currentStep = 0; currentOctave = 0;
    maxOctaves = 1; lastStepTime = 0; bpm = 120.0f;
    pattern = ARP_UP; isPlaying = false; gateLength = 80;
    repeatCounter = 0; direction = 1; patternPhase = 0;
    euclideanPos = 0;
    generateEuclideanRhythm(8, 5);
  }

  void noteOn(int note) {
    bool found = false;
    for (int i = 0; i < noteCount; i++)
      if (heldNotes[i] == note) { found = true; break; }

    if (!found && noteCount < MAX_ARP_NOTES) {
      heldNotes[noteCount++] = note;
      for (int i = 0; i < noteCount - 1; i++)
        for (int j = i + 1; j < noteCount; j++)
          if (heldNotes[i] > heldNotes[j]) {
            int t = heldNotes[i]; heldNotes[i] = heldNotes[j]; heldNotes[j] = t;
          }
    }

    if (!isPlaying && noteCount > 0) {
      isPlaying = true;
      currentStep = 0;
      lastStepTime = millis();
    }
  }

  void noteOff(int note) {
    for (int i = 0; i < noteCount; i++) {
      if (heldNotes[i] == note) {
        for (int j = i; j < noteCount - 1; j++) heldNotes[j] = heldNotes[j + 1];
        noteCount--;
        break;
      }
    }
    if (noteCount == 0) { isPlaying = false; currentStep = 0; }
  }

  void allNotesOff() {
    noteCount = 0; isPlaying = false; currentStep = 0;
  }

  int update(unsigned long currentTime) {
    if (!isPlaying || noteCount == 0) return -1;

    float stepInterval = (60000.0f / bpm) / 4.0f;

    if (currentTime - lastStepTime >= (unsigned long)stepInterval) {
      lastStepTime = currentTime;
      int idx = getNextNoteIndex();
      if (idx >= 0 && idx < noteCount) {
        int note = heldNotes[idx];
        if (pattern == ARP_OCTAVES) note += currentOctave * 12;
        currentStep++;
        return note;
      }
      currentStep++;
    }
    return -1;
  }

  void setPattern(ArpPattern p) {
    pattern = p; currentStep = 0;
    if (pattern == ARP_EUCLIDEAN_5_8) generateEuclideanRhythm(8, 5);
    else if (pattern == ARP_EUCLIDEAN_7_12) generateEuclideanRhythm(12, 7);
  }

  void setBPM(float newBpm) { bpm = constrain(newBpm, 30.0f, 300.0f); }
  void setOctaveRange(int oct) { maxOctaves = constrain(oct, 1, 4); }
  void setGateLength(int len) { gateLength = constrain(len, 10, 100); }

  ArpPattern getPattern() { return pattern; }
  float getBPM() { return bpm; }
  int getNoteCount() { return noteCount; }
  bool getIsPlaying() { return isPlaying; }
  int getGateLength() { return gateLength; }
  int getMaxOctaves() { return maxOctaves; }
};

#endif
