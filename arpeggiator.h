#ifndef ARPEGGIATOR_H
#define ARPEGGIATOR_H

#include <Arduino.h>
#include "synth_config.h"

// ============================================================================
// Arpeggiator with 100 Patterns
// ============================================================================
// Full-featured arpeggiator with multiple modes, patterns, gate, and swing
// ============================================================================

#define MAX_ARP_NOTES 16
#define NUM_ARP_PATTERNS 100

class Arpeggiator {
private:
  bool enabled;
  ArpMode mode;
  int patternIndex;
  float tempo;            // BPM
  float gateLength;       // 0.0 - 1.0
  float swing;            // 0.0 - 1.0 (0.5 = no swing)
  int octaveRange;        // 1-4 octaves

  // Note buffer
  uint8_t heldNotes[MAX_ARP_NOTES];
  uint8_t velocities[MAX_ARP_NOTES];
  int noteCount;

  // Sequencer state
  int currentStep;
  int currentOctave;
  bool ascending;
  unsigned long lastStepTime;
  unsigned long stepDuration;
  bool gateActive;

  // Pattern definitions (100 patterns, 16 steps each, -1 = rest)
  const int8_t patterns[NUM_ARP_PATTERNS][16] = {
    // 0-9: Basic patterns
    {0, 1, 2, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},  // 0: Up 4
    {3, 2, 1, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},  // 1: Down 4
    {0, 1, 2, 3, 2, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},    // 2: Up-Down 6
    {0, 2, 1, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},  // 3: Alt 4
    {0, 0, 1, 1, 2, 2, 3, 3, -1, -1, -1, -1, -1, -1, -1, -1},      // 4: Double
    {0, 3, 1, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},  // 5: Random 1
    {0, 2, 3, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},  // 6: Random 2
    {0, 1, 0, 2, 0, 1, 0, 3, -1, -1, -1, -1, -1, -1, -1, -1},      // 7: Bouncing
    {0, -1, 1, -1, 2, -1, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1},  // 8: Gated
    {0, 1, 2, -1, 1, 2, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1},    // 9: Triplet

    // 10-19: Rhythmic patterns
    {0, -1, 0, 1, -1, 1, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1},   // 10: Syncopated
    {0, 1, -1, -1, 2, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},  // 11: Breakbeat
    {0, 0, -1, 1, 1, -1, 2, 2, -1, -1, -1, -1, -1, -1, -1, -1},    // 12: Stutter
    {0, 1, 2, 1, 0, 1, 2, 3, -1, -1, -1, -1, -1, -1, -1, -1},      // 13: Forward Ping
    {3, 2, 1, 2, 3, 2, 1, 0, -1, -1, -1, -1, -1, -1, -1, -1},      // 14: Backward Ping
    {0, -1, -1, 1, -1, -1, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 15: Sparse
    {0, 0, 1, -1, 0, 0, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1},    // 16: Repetitive
    {0, 3, -1, 1, 2, -1, -1, 3, -1, -1, -1, -1, -1, -1, -1, -1},   // 17: Jump
    {0, 1, 2, 3, 3, 2, 1, 0, -1, -1, -1, -1, -1, -1, -1, -1},      // 18: Pyramid
    {0, 2, 1, 3, 2, 0, 3, 1, -1, -1, -1, -1, -1, -1, -1, -1},      // 19: Woven

    // 20-29: Complex patterns
    {0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 3},              // 20: Bass Pulse
    {0, -1, 2, -1, 1, -1, 3, -1, 0, -1, 2, -1, 1, -1, 3, -1},      // 21: Four on Floor
    {0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, -1, -1, -1, -1},          // 22: Triplet Run
    {0, 1, 2, 0, 1, 3, 0, 1, 2, 0, 1, 3, -1, -1, -1, -1},          // 23: Chord Roll
    {0, 3, 2, 1, 0, 3, 2, 1, -1, -1, -1, -1, -1, -1, -1, -1},      // 24: Cascade Down
    {0, -1, 1, 2, -1, 3, -1, 2, 1, -1, -1, -1, -1, -1, -1, -1},    // 25: Random Walk
    {0, 2, -1, 3, 1, -1, 2, 0, -1, -1, -1, -1, -1, -1, -1, -1},    // 26: Skip Pattern
    {0, 0, 1, 2, 2, 3, 1, 1, -1, -1, -1, -1, -1, -1, -1, -1},      // 27: Grouping
    {0, 1, 2, 3, 2, 1, 0, 1, 2, 3, -1, -1, -1, -1, -1, -1},        // 28: Wave Up
    {3, 2, 1, 0, 1, 2, 3, 2, 1, 0, -1, -1, -1, -1, -1, -1},        // 29: Wave Down

    // 30-39: Melodic patterns
    {0, 2, 3, 2, 0, 1, 2, 1, -1, -1, -1, -1, -1, -1, -1, -1},      // 30: Melody 1
    {0, 1, 3, 2, 1, 0, 2, 3, -1, -1, -1, -1, -1, -1, -1, -1},      // 31: Melody 2
    {0, -1, 2, 1, -1, 3, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1},   // 32: Melody 3
    {0, 1, 0, 3, 2, 3, 2, 1, -1, -1, -1, -1, -1, -1, -1, -1},      // 33: Folk Pattern
    {0, 2, 0, 3, 1, 3, 1, 2, -1, -1, -1, -1, -1, -1, -1, -1},      // 34: Country Lick
    {0, 0, 2, 3, 3, 2, 0, 1, -1, -1, -1, -1, -1, -1, -1, -1},      // 35: Blues Riff
    {0, 3, 0, 2, 0, 1, 0, 3, -1, -1, -1, -1, -1, -1, -1, -1},      // 36: Root Return
    {0, 1, 2, -1, 3, -1, 2, 1, 0, -1, -1, -1, -1, -1, -1, -1},     // 37: Ascending Seq
    {3, -1, 2, 1, -1, 0, 1, -1, 2, -1, -1, -1, -1, -1, -1, -1},    // 38: Descending Seq
    {0, 2, 1, 3, 0, 2, 1, 3, -1, -1, -1, -1, -1, -1, -1, -1},      // 39: Alternating

    // 40-49: Techno/Electronic patterns
    {0, -1, -1, -1, 0, -1, -1, -1, 1, -1, -1, -1, 2, -1, -1, -1},  // 40: Techno Bass
    {0, 0, -1, 0, 1, 1, -1, 1, 2, 2, -1, 2, 3, 3, -1, 3},          // 41: TB-303 Style
    {0, 3, 0, 3, 1, 3, 1, 3, 2, 3, 2, 3, -1, -1, -1, -1},          // 42: Acid Line
    {0, -1, 1, -1, -1, 2, -1, 3, 0, -1, 1, -1, -1, 2, -1, -1},     // 43: Trance Gate
    {0, 1, -1, 2, -1, -1, 3, -1, 2, -1, 1, -1, -1, 0, -1, -1},     // 44: Prog House
    {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 2, 2, 1, 1, 0, 0},              // 45: Build Up
    {0, -1, 2, 1, -1, 3, -1, 2, -1, 1, 3, -1, 2, -1, 0, -1},       // 46: Glitch
    {0, 1, 2, 3, -1, -1, -1, -1, 0, 1, 2, 3, -1, -1, -1, -1},      // 47: Half Time
    {0, -1, 0, -1, 1, -1, 1, -1, 2, -1, 2, -1, 3, -1, 3, -1},      // 48: 16th Pulse
    {0, 2, 3, -1, 1, -1, 2, 3, -1, 0, -1, 1, 2, -1, 3, -1},        // 49: Elektron Style

    // 50-99: Additional creative patterns (simplified for space)
    {0, 1, 2, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},  // 50-99: Various patterns
    {0, 2, 1, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 0, 1, 1, 2, 2, 3, 3, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 0, 2, 0, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, -1, 1, -1, 2, -1, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 2, 1, 0, 1, 2, 3, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 2, 0, 3, 0, 1, 0, 3, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 2, 3, 3, 2, 1, 0, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 0, 1, 2, 3, 2, 1, 0, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, -1, 2, -1, 3, -1, 2, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 2, 1, 3, 2, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 2, -1, 3, -1, 2, 1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, -1, 0, 1, -1, 1, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 2, 3, 2, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 2, -1, 1, 3, -1, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 0, 2, 1, 2, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 3, 2, 1, 0, 1, 2, 3, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 0, 2, 2, 1, 1, 3, 3, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, -1, -1, 2, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, -1, 2, -1, 1, -1, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 2, 0, 1, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 2, 3, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 0, 3, 0, 2, 0, 3, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 2, 1, 2, 3, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 2, 3, 3, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, -1, 1, 2, -1, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, -1, 2, 3, -1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 2, 0, 1, 0, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 2, 1, 3, 1, 2, 1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 3, 1, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 0, 1, 0, 2, 0, 3, 0, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 2, 3, -1, 2, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 2, -1, 3, -1, 1, -1, 2, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 3, 2, 0, 1, 3, 2, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, -1, -1, 1, -1, -1, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 2, 1, 0, 3, 1, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 2, 2, 3, 3, 2, 1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 3, -1, 2, -1, 1, -1, 0, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 0, 2, 0, 3, 0, 2, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 2, 3, -1, 1, -1, -1, 3, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, -1, 3, -1, 2, -1, 1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 0, 0, 1, 1, 2, 2, 3, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 2, 1, 3, 1, 2, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 2, -1, -1, 3, 2, 1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, -1, 2, 3, -1, 1, -1, 2, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 3, 1, 2, 1, 3, 1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 2, 0, 3, 1, 3, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 1, 2, 0, 3, 0, 2, 1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 3, 2, -1, 1, -1, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0, 0, 1, 1, 2, 2, 3, 3, 2, 2, 1, 1, 0, 0, -1, -1}
  };

public:
  Arpeggiator() :
    enabled(false),
    mode(ARP_UP),
    patternIndex(0),
    tempo(120.0f),
    gateLength(0.75f),
    swing(0.5f),
    octaveRange(1),
    noteCount(0),
    currentStep(0),
    currentOctave(0),
    ascending(true),
    lastStepTime(0),
    stepDuration(0),
    gateActive(false) {
    memset(heldNotes, 0, sizeof(heldNotes));
    memset(velocities, 0, sizeof(velocities));
    updateStepDuration();
  }

  void setEnabled(bool enable) {
    enabled = enable;
    if (!enabled) {
      currentStep = 0;
      gateActive = false;
    }
  }

  void setMode(ArpMode m) { mode = m; }
  void setPattern(int index) { patternIndex = constrain(index, 0, NUM_ARP_PATTERNS - 1); }
  void setTempo(float bpm) { tempo = constrain(bpm, 30.0f, 300.0f); updateStepDuration(); }
  void setGateLength(float gate) { gateLength = constrain(gate, 0.1f, 1.0f); }
  void setSwing(float sw) { swing = constrain(sw, 0.0f, 1.0f); }
  void setOctaveRange(int octaves) { octaveRange = constrain(octaves, 1, 4); }

  bool isEnabled() const { return enabled; }
  float getTempo() const { return tempo; }
  float getGateLength() const { return gateLength; }
  float getSwing() const { return swing; }
  int getPattern() const { return patternIndex; }

  void noteOn(uint8_t note, uint8_t velocity) {
    // Add note to buffer if not already present
    for (int i = 0; i < noteCount; i++) {
      if (heldNotes[i] == note) return;
    }

    if (noteCount < MAX_ARP_NOTES) {
      heldNotes[noteCount] = note;
      velocities[noteCount] = velocity;
      noteCount++;
      sortNotes();
    }
  }

  void noteOff(uint8_t note) {
    // Remove note from buffer
    for (int i = 0; i < noteCount; i++) {
      if (heldNotes[i] == note) {
        for (int j = i; j < noteCount - 1; j++) {
          heldNotes[j] = heldNotes[j + 1];
          velocities[j] = velocities[j + 1];
        }
        noteCount--;
        if (currentStep >= noteCount && noteCount > 0) {
          currentStep = noteCount - 1;
        }
        break;
      }
    }
  }

  void allNotesOff() {
    noteCount = 0;
    currentStep = 0;
    currentOctave = 0;
    ascending = true;
    gateActive = false;
  }

  // Returns: {note, velocity, gate_on/off}
  // Call this periodically to get arp events
  struct ArpEvent {
    bool hasEvent;
    uint8_t note;
    uint8_t velocity;
    bool gateOn;
  };

  ArpEvent process() {
    ArpEvent event = {false, 0, 0, false};

    if (!enabled || noteCount == 0) {
      return event;
    }

    unsigned long currentTime = millis();
    unsigned long timeSinceStep = currentTime - lastStepTime;

    // Apply swing to even steps
    unsigned long adjustedDuration = stepDuration;
    if (currentStep % 2 == 1) {
      adjustedDuration = stepDuration * (1.0f + swing);
    }

    // Check if it's time for next step
    if (timeSinceStep >= adjustedDuration) {
      lastStepTime = currentTime;

      // Get next note from pattern or mode
      int noteIndex = getNextNoteIndex();

      if (noteIndex >= 0 && noteIndex < noteCount) {
        int octaveOffset = currentOctave * 12;
        event.hasEvent = true;
        event.note = heldNotes[noteIndex] + octaveOffset;
        event.velocity = velocities[noteIndex];
        event.gateOn = true;
        gateActive = true;
      }

      currentStep++;
    }

    // Handle gate off
    if (gateActive && timeSinceStep >= (adjustedDuration * gateLength)) {
      event.hasEvent = true;
      event.gateOn = false;
      gateActive = false;
    }

    return event;
  }

private:
  void updateStepDuration() {
    // Duration of one 16th note in milliseconds
    stepDuration = (60000.0f / tempo) / 4.0f;
  }

  void sortNotes() {
    // Simple bubble sort for held notes
    for (int i = 0; i < noteCount - 1; i++) {
      for (int j = 0; j < noteCount - i - 1; j++) {
        if (heldNotes[j] > heldNotes[j + 1]) {
          uint8_t tempNote = heldNotes[j];
          heldNotes[j] = heldNotes[j + 1];
          heldNotes[j + 1] = tempNote;

          uint8_t tempVel = velocities[j];
          velocities[j] = velocities[j + 1];
          velocities[j + 1] = tempVel;
        }
      }
    }
  }

  int getNextNoteIndex() {
    if (mode == ARP_PATTERN) {
      // Use pattern-based sequencing
      const int8_t* pattern = patterns[patternIndex];
      int patternStep = currentStep % 16;
      return pattern[patternStep];
    }

    // Use algorithmic modes
    int index = 0;

    switch (mode) {
      case ARP_UP:
        index = currentStep % noteCount;
        if (index == 0 && currentStep > 0) {
          currentOctave = (currentOctave + 1) % octaveRange;
        }
        break;

      case ARP_DOWN:
        index = (noteCount - 1) - (currentStep % noteCount);
        if (index == noteCount - 1 && currentStep > 0) {
          currentOctave = (currentOctave + 1) % octaveRange;
        }
        break;

      case ARP_UP_DOWN:
        if (ascending) {
          index = currentStep % (noteCount * 2 - 2);
          if (index >= noteCount) {
            index = (noteCount * 2 - 2) - index;
            ascending = false;
          }
        } else {
          index = (noteCount * 2 - 2) - (currentStep % (noteCount * 2 - 2));
          if (index < 0) {
            ascending = true;
            index = 0;
          }
        }
        break;

      case ARP_RANDOM:
        index = random(noteCount);
        break;

      default:
        index = currentStep % noteCount;
        break;
    }

    return index;
  }
};

#endif // ARPEGGIATOR_H
