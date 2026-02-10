#ifndef ZOMBIE_STEP_SEQUENCER_H
#define ZOMBIE_STEP_SEQUENCER_H

#include "synth_engine.h"

// 16-step sequencer with synth engine integration
// ZOMBIE SS style
#define MAX_SEQ_STEPS 16
#define MAX_SEQ_TRACKS 4

struct SequencerStep {
  bool active;
  int note;          // MIDI note number
  int velocity;
  int gate;          // 1-16 (step length)
  bool tie;          // Tie to next step
};

typedef void (*SeqNoteOnCB)(int trackIdx, int noteNum, int vel);
typedef void (*SeqNoteOffCB)(int trackIdx, int noteNum);

struct SequencerTrack {
  SequencerStep steps[MAX_SEQ_STEPS];
  int currentStep;
  bool muted;
  int octave;
  int scale;
  int soundPatchIdx;  // 0-9 = factory preset index for this track

  void init() {
    currentStep = 0;
    muted = false;
    octave = 3;
    scale = 0;
    soundPatchIdx = 0;

    for (int i = 0; i < MAX_SEQ_STEPS; i++) {
      steps[i].active = false;
      steps[i].note = 60 + (i % 12);
      steps[i].velocity = 100;
      steps[i].gate = 8;
      steps[i].tie = false;
    }
  }

  void clear() {
    for (int i = 0; i < MAX_SEQ_STEPS; i++) {
      steps[i].active = false;
    }
  }

  void randomize() {
    for (int i = 0; i < MAX_SEQ_STEPS; i++) {
      steps[i].active = (random(100) < 60);
      steps[i].note = 36 + random(36);
      steps[i].velocity = 80 + random(47);
      steps[i].gate = 4 + random(9);
    }
  }
};

class ZombieSequencer {
private:
  SequencerTrack tracks[MAX_SEQ_TRACKS];
  int currentStep;
  unsigned long lastStepTime;
  float bpm;
  bool isPlaying;
  bool isRecording;
  int activeTrack;
  int stepsPerBar;
  int swing;  // 50-75% swing

  SynthEngine* synth;
  SeqNoteOnCB  _noteOnCB;
  SeqNoteOffCB _noteOffCB;

  // Note tracking for note-off
  int activeNotes[MAX_SEQ_TRACKS];

public:
  ZombieSequencer() {
    currentStep = 0;
    lastStepTime = 0;
    bpm = 120.0f;
    isPlaying = false;
    isRecording = false;
    activeTrack = 0;
    stepsPerBar = 16;
    swing = 50;
    synth = NULL;
    _noteOnCB  = NULL;
    _noteOffCB = NULL;

    for (int i = 0; i < MAX_SEQ_TRACKS; i++) {
      tracks[i].init();
      tracks[i].soundPatchIdx = i % 10; // default: each track uses a different factory preset
      activeNotes[i] = -1;
    }
  }

  void setCallbacks(SeqNoteOnCB onCB, SeqNoteOffCB offCB) {
    _noteOnCB  = onCB;
    _noteOffCB = offCB;
  }

  void setSynthEngine(SynthEngine* s) {
    synth = s;
  }

  void init() {
    for (int i = 0; i < MAX_SEQ_TRACKS; i++) {
      tracks[i].init();
    }
  }

  void play() {
    isPlaying = true;
    currentStep = 0;
    lastStepTime = millis();
  }

  void stop() {
    isPlaying = false;
    currentStep = 0;

    // Stop all active notes
    for (int t = 0; t < MAX_SEQ_TRACKS; t++) {
      if (activeNotes[t] >= 0) {
        if (_noteOffCB) _noteOffCB(t, activeNotes[t]);
        else if (synth) synth->noteOff(activeNotes[t]);
        activeNotes[t] = -1;
      }
    }
  }

  void pause() {
    isPlaying = !isPlaying;
  }

  void update(unsigned long currentTime) {
    if (!isPlaying || !synth) return;

    float stepInterval = (60000.0f / bpm) / 4.0f; // 16th notes

    // Apply swing to odd steps
    if ((currentStep % 2) == 1 && swing > 50) {
      float swingAmount = (swing - 50) / 50.0f;
      stepInterval *= (1.0f + swingAmount * 0.5f);
    }

    if (currentTime - lastStepTime >= (unsigned long)stepInterval) {
      lastStepTime = currentTime;

      // Process each track
      for (int t = 0; t < MAX_SEQ_TRACKS; t++) {
        if (tracks[t].muted) continue;

        SequencerStep& step = tracks[t].steps[currentStep];

        // Note off for previous step if not tied
        if (activeNotes[t] >= 0 && !step.tie) {
          if (_noteOffCB) _noteOffCB(t, activeNotes[t]);
          else if (synth) synth->noteOff(activeNotes[t]);
          activeNotes[t] = -1;
        }

        // Note on for active steps
        if (step.active) {
          int note = step.note + (tracks[t].octave * 12);
          if (_noteOnCB) _noteOnCB(t, note, step.velocity);
          else if (synth) synth->noteOn(note, step.velocity);
          activeNotes[t] = note;
        }
      }

      // Advance step
      currentStep = (currentStep + 1) % stepsPerBar;
    }
  }

  void setStep(int track, int step, bool active, int note, int velocity) {
    if (track >= 0 && track < MAX_SEQ_TRACKS && step >= 0 && step < MAX_SEQ_STEPS) {
      tracks[track].steps[step].active = active;
      tracks[track].steps[step].note = note;
      tracks[track].steps[step].velocity = velocity;
    }
  }

  void toggleStep(int track, int step) {
    if (track >= 0 && track < MAX_SEQ_TRACKS && step >= 0 && step < MAX_SEQ_STEPS) {
      tracks[track].steps[step].active = !tracks[track].steps[step].active;
    }
  }

  void clearTrack(int track) {
    if (track >= 0 && track < MAX_SEQ_TRACKS) {
      tracks[track].clear();
    }
  }

  void clearAll() {
    for (int i = 0; i < MAX_SEQ_TRACKS; i++) {
      tracks[i].clear();
    }
  }

  void randomizeTrack(int track) {
    if (track >= 0 && track < MAX_SEQ_TRACKS) {
      tracks[track].randomize();
    }
  }

  void setBPM(float newBpm) {
    bpm = constrain(newBpm, 40.0f, 300.0f);
  }

  void setSwing(int swingPercent) {
    swing = constrain(swingPercent, 50, 75);
  }

  void setActiveTrack(int track) {
    activeTrack = constrain(track, 0, MAX_SEQ_TRACKS - 1);
  }

  void muteTrack(int track, bool mute) {
    if (track >= 0 && track < MAX_SEQ_TRACKS) {
      tracks[track].muted = mute;
    }
  }

  // Pattern management
  void shiftTrackLeft(int track) {
    if (track >= 0 && track < MAX_SEQ_TRACKS) {
      SequencerStep first = tracks[track].steps[0];
      for (int i = 0; i < MAX_SEQ_STEPS - 1; i++) {
        tracks[track].steps[i] = tracks[track].steps[i + 1];
      }
      tracks[track].steps[MAX_SEQ_STEPS - 1] = first;
    }
  }

  void shiftTrackRight(int track) {
    if (track >= 0 && track < MAX_SEQ_STEPS) {
      SequencerStep last = tracks[track].steps[MAX_SEQ_STEPS - 1];
      for (int i = MAX_SEQ_STEPS - 1; i > 0; i--) {
        tracks[track].steps[i] = tracks[track].steps[i - 1];
      }
      tracks[track].steps[0] = last;
    }
  }

  void reverseTrack(int track) {
    if (track >= 0 && track < MAX_SEQ_TRACKS) {
      for (int i = 0; i < MAX_SEQ_STEPS / 2; i++) {
        SequencerStep temp = tracks[track].steps[i];
        tracks[track].steps[i] = tracks[track].steps[MAX_SEQ_STEPS - 1 - i];
        tracks[track].steps[MAX_SEQ_STEPS - 1 - i] = temp;
      }
    }
  }

  // Getters
  int getCurrentStep() { return currentStep; }
  bool getIsPlaying() { return isPlaying; }
  float getBPM() { return bpm; }
  int getActiveTrack() { return activeTrack; }
  SequencerTrack* getTrack(int track) {
    if (track >= 0 && track < MAX_SEQ_TRACKS) {
      return &tracks[track];
    }
    return NULL;
  }
  bool isStepActive(int track, int step) {
    if (track >= 0 && track < MAX_SEQ_TRACKS && step >= 0 && step < MAX_SEQ_STEPS) {
      return tracks[track].steps[step].active;
    }
    return false;
  }
};

#endif
