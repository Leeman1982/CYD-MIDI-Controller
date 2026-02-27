#ifndef ZOMBIE_STEP_SEQUENCER_H
#define ZOMBIE_STEP_SEQUENCER_H

#include "synth_engine.h"

// ZOMBIE SS – Commercial-grade 16-step sequencer engine
// 4 tracks · per-step pitch/vel/gate/probability · polyrhythm
// Scale quantize · MIDI OUT routing · live recording · swing

#define MAX_SEQ_STEPS  16
#define MAX_SEQ_TRACKS 4

// ── Scale definitions ────────────────────────────────────────────────────────
#define NUM_SCALES_SEQ 9
static const char* seqScaleNames[NUM_SCALES_SEQ] = {
  "CHROM","MAJOR","MINOR","DORIA","PHRYG","LYDIA","MIXO","PENTA+","PENTA-"
};
static const int8_t seqScaleIntervals[NUM_SCALES_SEQ][12] = {
  {0,1,2,3,4,5,6,7,8,9,10,11},
  {0,2,4,5,7,9,11,-1,-1,-1,-1,-1},
  {0,2,3,5,7,8,10,-1,-1,-1,-1,-1},
  {0,2,3,5,7,9,10,-1,-1,-1,-1,-1},
  {0,1,3,5,7,8,10,-1,-1,-1,-1,-1},
  {0,2,4,6,7,9,11,-1,-1,-1,-1,-1},
  {0,2,4,5,7,9,10,-1,-1,-1,-1,-1},
  {0,2,4,7,9,-1,-1,-1,-1,-1,-1,-1},
  {0,3,5,7,10,-1,-1,-1,-1,-1,-1,-1},
};

static const char* seqRootNames[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

inline bool seqNoteInScale(int pc, int key, int scale) {
  int rel = ((pc - key) % 12 + 12) % 12;
  for (int i = 0; i < 12 && seqScaleIntervals[scale][i] >= 0; i++)
    if (seqScaleIntervals[scale][i] == rel) return true;
  return false;
}

inline int seqQuantizeNote(int note, int key, int scale) {
  if (scale == 0) return note;
  int oct = note / 12;
  int best = note, bestDist = 127;
  for (int o = -1; o <= 1; o++) {
    for (int i = 0; i < 12 && seqScaleIntervals[scale][i] >= 0; i++) {
      int cand = (oct + o)*12 + ((key + seqScaleIntervals[scale][i]) % 12);
      int d = abs(cand - note);
      if (d < bestDist) { bestDist = d; best = cand; }
    }
  }
  return constrain(best, 0, 127);
}

// ── Per-step data ─────────────────────────────────────────────────────────────
struct SequencerStep {
  bool    active;
  uint8_t note;        // MIDI note 0-127
  uint8_t velocity;    // 1-127
  uint8_t gatePercent; // 1-100
  uint8_t probability; // 1-100  (100 = always fires)
  bool    tie;
};

// ── Callbacks ─────────────────────────────────────────────────────────────────
typedef void (*SeqNoteOnCB)(int trackIdx, int noteNum, int vel);
typedef void (*SeqNoteOffCB)(int trackIdx, int noteNum);

// ── Per-track config ──────────────────────────────────────────────────────────
struct SequencerTrack {
  SequencerStep steps[MAX_SEQ_STEPS];
  bool    muted;
  bool    solo;
  int8_t  octave;       // -3..+3 octave offset
  uint8_t soundPatchIdx;
  uint8_t midiChannel;  // 1-16
  uint8_t midiOutput;   // 0=INT 1=EXT 2=BOTH
  uint8_t trackLength;  // 1-16 (polyrhythm)

  void init(int idx = 0) {
    muted = false; solo = false; octave = 0;
    soundPatchIdx = idx % 10;
    midiChannel   = (uint8_t)(idx + 1);
    midiOutput    = 0;
    trackLength   = 16;
    for (int i = 0; i < MAX_SEQ_STEPS; i++) {
      steps[i] = {false, 60, 100, 75, 100, false};
    }
  }

  void clear() {
    for (int i = 0; i < MAX_SEQ_STEPS; i++) steps[i].active = false;
  }

  void randomize() {
    for (int i = 0; i < MAX_SEQ_STEPS; i++) {
      steps[i].active      = (random(100) < 60);
      steps[i].note        = 36 + random(36);
      steps[i].velocity    = 70 + random(57);
      steps[i].gatePercent = 40 + random(60);
      steps[i].probability = 100;
      steps[i].tie         = false;
    }
  }

  void copyTo(SequencerTrack& dst) {
    memcpy(dst.steps, steps, sizeof(steps));
    dst.trackLength = trackLength;
    dst.octave      = octave;
  }
};

// ── ZombieSequencer ───────────────────────────────────────────────────────────
class ZombieSequencer {
private:
  SequencerTrack tracks[MAX_SEQ_TRACKS];
  int   currentStep;
  int   trackStep[MAX_SEQ_TRACKS];
  unsigned long lastStepTime;
  float bpm;
  bool  isPlaying;
  bool  isRecording;
  int   activeTrack;
  int   swing;
  int   globalKey;
  int   globalScale;
  bool  scaleLock;

  SynthEngine*  synth;
  SeqNoteOnCB   _noteOnCB;
  SeqNoteOffCB  _noteOffCB;

  int8_t         activeNotes[MAX_SEQ_TRACKS];
  unsigned long  noteOnTimes[MAX_SEQ_TRACKS];
  unsigned long  noteDurations[MAX_SEQ_TRACKS];

  SequencerTrack clipboard;
  bool           hasClipboard;

public:
  ZombieSequencer() {
    currentStep = 0; lastStepTime = 0;
    bpm = 120.0f; swing = 50;
    isPlaying = false; isRecording = false;
    activeTrack = 0;
    globalKey = 0; globalScale = 1; scaleLock = false;
    synth = NULL; _noteOnCB = NULL; _noteOffCB = NULL;
    hasClipboard = false;
    for (int i = 0; i < MAX_SEQ_TRACKS; i++) {
      tracks[i].init(i);
      trackStep[i] = 0;
      activeNotes[i] = -1;
      noteOnTimes[i] = noteDurations[i] = 0;
    }
  }

  void setCallbacks(SeqNoteOnCB on, SeqNoteOffCB off) { _noteOnCB = on; _noteOffCB = off; }
  void setSynthEngine(SynthEngine* s) { synth = s; }

  void init() {
    for (int i = 0; i < MAX_SEQ_TRACKS; i++) { tracks[i].init(i); trackStep[i] = 0; }
    currentStep = 0;
  }

  void setBPM(float v)       { bpm = constrain(v, 30.0f, 300.0f); }
  void setSwing(int v)       { swing = constrain(v, 50, 75); }
  void setActiveTrack(int t) { activeTrack = constrain(t, 0, MAX_SEQ_TRACKS-1); }
  void setGlobalKey(int k)   { globalKey = constrain(k, 0, 11); }
  void setGlobalScale(int s) { globalScale = constrain(s, 0, NUM_SCALES_SEQ-1); }
  void setScaleLock(bool v)  { scaleLock = v; }

  void play() {
    isPlaying = true; currentStep = 0; lastStepTime = millis();
    for (int i = 0; i < MAX_SEQ_TRACKS; i++) trackStep[i] = 0;
  }

  void stop() {
    isPlaying = false; currentStep = 0;
    for (int i = 0; i < MAX_SEQ_TRACKS; i++) {
      trackStep[i] = 0;
      if (activeNotes[i] >= 0) {
        if (_noteOffCB) _noteOffCB(i, activeNotes[i]);
        else if (synth) synth->noteOff(activeNotes[i]);
        activeNotes[i] = -1; noteDurations[i] = 0;
      }
    }
  }

  void startRecording() { isRecording = true; }
  void stopRecording()  { isRecording = false; }
  bool getIsRecording() { return isRecording; }

  // Called from MIDI IN noteOn callback when recording
  void recordNote(uint8_t note, uint8_t vel) {
    if (!isRecording || !isPlaying) return;
    int ts = trackStep[activeTrack];
    SequencerStep& s = tracks[activeTrack].steps[ts];
    s.active      = true;
    s.note        = scaleLock ? seqQuantizeNote(note, globalKey, globalScale) : note;
    s.velocity    = vel;
    s.gatePercent = 75;
    s.probability = 100;
  }

  void update(unsigned long now) {
    if (!isPlaying) return;

    // Gate expiry
    for (int t = 0; t < MAX_SEQ_TRACKS; t++) {
      if (activeNotes[t] >= 0 && noteDurations[t] > 0 &&
          now - noteOnTimes[t] >= noteDurations[t]) {
        if (_noteOffCB) _noteOffCB(t, activeNotes[t]);
        else if (synth) synth->noteOff(activeNotes[t]);
        activeNotes[t] = -1; noteDurations[t] = 0;
      }
    }

    float stepMs = (60000.0f / bpm) / 4.0f;
    float thisMs = stepMs;
    if ((currentStep % 2) == 1 && swing > 50)
      thisMs *= 1.0f + ((swing - 50) / 50.0f) * 0.5f;

    if (now - lastStepTime < (unsigned long)thisMs) return;
    lastStepTime = now;

    bool anySolo = false;
    for (int t = 0; t < MAX_SEQ_TRACKS; t++) if (tracks[t].solo) { anySolo = true; break; }

    for (int t = 0; t < MAX_SEQ_TRACKS; t++) {
      bool play = !tracks[t].muted && (!anySolo || tracks[t].solo);
      int  ts   = trackStep[t];
      SequencerStep& step = tracks[t].steps[ts];

      // Note off (unless tied)
      if (activeNotes[t] >= 0 && !step.tie) {
        if (_noteOffCB) _noteOffCB(t, activeNotes[t]);
        else if (synth) synth->noteOff(activeNotes[t]);
        activeNotes[t] = -1; noteDurations[t] = 0;
      }

      if (play && step.active && ((int)random(100) < step.probability)) {
        int note = constrain((int)step.note + (int)tracks[t].octave * 12, 0, 127);
        if (scaleLock) note = seqQuantizeNote(note, globalKey, globalScale);

        if (_noteOnCB) _noteOnCB(t, note, step.velocity);
        else if (synth) synth->noteOn(note, step.velocity);

        activeNotes[t]   = (int8_t)note;
        noteOnTimes[t]   = now;
        noteDurations[t] = (unsigned long)(stepMs * step.gatePercent / 100.0f);
        if (noteDurations[t] < 5) noteDurations[t] = 5;
      }

      trackStep[t] = (ts + 1) % tracks[t].trackLength;
    }

    currentStep = (currentStep + 1) % MAX_SEQ_STEPS;
  }

  // Step editing
  void toggleStep(int t, int s) {
    if (t<0||t>=MAX_SEQ_TRACKS||s<0||s>=MAX_SEQ_STEPS) return;
    tracks[t].steps[s].active = !tracks[t].steps[s].active;
  }
  void clearTrack(int t)    { if (t>=0&&t<MAX_SEQ_TRACKS) tracks[t].clear(); }
  void randomizeTrack(int t){ if (t>=0&&t<MAX_SEQ_TRACKS) tracks[t].randomize(); }

  void copyTrack(int t)  { if (t>=0&&t<MAX_SEQ_TRACKS) { tracks[t].copyTo(clipboard); hasClipboard=true; } }
  void pasteTrack(int t) { if (hasClipboard&&t>=0&&t<MAX_SEQ_TRACKS) clipboard.copyTo(tracks[t]); }

  void shiftLeft(int t) {
    if (t<0||t>=MAX_SEQ_TRACKS) return;
    SequencerStep f = tracks[t].steps[0];
    for (int i=0;i<MAX_SEQ_STEPS-1;i++) tracks[t].steps[i]=tracks[t].steps[i+1];
    tracks[t].steps[MAX_SEQ_STEPS-1]=f;
  }
  void shiftRight(int t) {
    if (t<0||t>=MAX_SEQ_TRACKS) return;
    SequencerStep l = tracks[t].steps[MAX_SEQ_STEPS-1];
    for (int i=MAX_SEQ_STEPS-1;i>0;i--) tracks[t].steps[i]=tracks[t].steps[i-1];
    tracks[t].steps[0]=l;
  }
  void reverseTrack(int t) {
    if (t<0||t>=MAX_SEQ_TRACKS) return;
    for (int i=0;i<MAX_SEQ_STEPS/2;i++) {
      SequencerStep tmp=tracks[t].steps[i];
      tracks[t].steps[i]=tracks[t].steps[MAX_SEQ_STEPS-1-i];
      tracks[t].steps[MAX_SEQ_STEPS-1-i]=tmp;
    }
  }

  // Getters
  int  getCurrentStep()  { return currentStep; }
  int  getTrackStep(int t){ return (t>=0&&t<MAX_SEQ_TRACKS) ? trackStep[t] : 0; }
  bool getIsPlaying()    { return isPlaying; }
  float getBPM()         { return bpm; }
  int  getSwing()        { return swing; }
  int  getActiveTrack()  { return activeTrack; }
  int  getGlobalKey()    { return globalKey; }
  int  getGlobalScale()  { return globalScale; }
  bool getScaleLock()    { return scaleLock; }
  bool getHasClipboard() { return hasClipboard; }

  SequencerTrack* getTrack(int t) {
    return (t>=0&&t<MAX_SEQ_TRACKS) ? &tracks[t] : NULL;
  }
};

#endif
