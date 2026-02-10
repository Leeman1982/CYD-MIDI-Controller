#ifndef ZOMBIE_SEQ_MODE_H
#define ZOMBIE_SEQ_MODE_H

#include "common_definitions.h"
#include "ui_elements.h"
#include "zombie_step_sequencer.h"
#include "zombie_synth_mode.h"
#include "zombie_presets.h"   // for factoryPresets[]

// ZOMBIE SS Step Sequencer UI – v2 with per-track sound select

static ZombieSequencer* zombieSeq = NULL;
static int  seqEditTrack    = 0;
static int  seqEditNote     = 60;
static bool seqNeedsRedraw  = true;
static int  seqLastStep     = -1;
static bool seqLastPlaying  = false;
static bool seqSoundSelect  = false;  // sound sub-menu overlay active

// ── Sequencer note-on/off callbacks (apply per-track patch) ────────────────
void seqNoteOnHandler(int trackIdx, int noteNum, int vel) {
  SynthEngine* synth = getZombieSynth();
  if (!synth) return;
  SequencerTrack* t = zombieSeq ? zombieSeq->getTrack(trackIdx) : NULL;
  if (t) {
    int pIdx = constrain(t->soundPatchIdx, 0, NUM_FACTORY - 1);
    const SynthPatch& p = factoryPresets[pIdx];
    // Apply track's chosen oscillator / filter settings
    synth->setOsc1Waveform((WaveformType)p.osc1Wave);
    synth->setOsc2Waveform((WaveformType)p.osc2Wave);
    synth->setFilterType((FilterType)p.filterType);
    synth->setFilterCutoff(p.filterCutoff);
    synth->setFilterResonance(p.filterResonance);
  }
  synth->noteOn(noteNum, vel);
}

void seqNoteOffHandler(int trackIdx, int noteNum) {
  SynthEngine* synth = getZombieSynth();
  if (synth) synth->noteOff(noteNum);
}

// ── Step cell drawing ──────────────────────────────────────────────────────
void drawSeqStep(int x, int y, int w, int h, bool active, bool playing) {
  uint16_t bg  = THEME_BG;
  uint16_t brd = THEME_OUTLINE;
  if      (active && playing)  { bg = THEME_ACCENT;   }
  else if (active)             { bg = THEME_PRIMARY;  }
  else if (playing)            { brd = THEME_ACCENT;  }
  tft.fillRoundRect(x, y, w, h, 3, bg);
  tft.drawRoundRect(x, y, w, h, 3, brd);
}

// ── Sound select overlay ───────────────────────────────────────────────────
void drawSeqSoundSelect() {
  // Semi-opaque background panel
  tft.fillRect(20, 50, 280, 175, THEME_SURFACE);
  tft.drawRect(20, 50, 280, 175, THEME_OUTLINE);
  tft.drawRect(21, 51, 278, 173, THEME_OUTLINE);

  char title[30];
  snprintf(title, sizeof(title), "SOUND FOR TRACK %d", seqEditTrack + 1);
  tft.setTextColor(THEME_PRIMARY, THEME_SURFACE);
  tft.drawCentreString(title, 160, 55, 2);

  // 10 factory preset names (2 columns of 5)
  for (int i = 0; i < NUM_FACTORY; i++) {
    int col = i / 5;
    int row = i % 5;
    int x = 25 + col * 138;
    int y = 70 + row * 27;
    int w = 133, h = 24;

    SequencerTrack* tr = zombieSeq->getTrack(seqEditTrack);
    bool sel = tr && (tr->soundPatchIdx == i);

    uint16_t bg  = sel ? THEME_PRIMARY : THEME_BG;
    uint16_t txt = sel ? THEME_BG : THEME_PRIMARY;
    tft.fillRoundRect(x, y, w, h, 3, bg);
    tft.drawRoundRect(x, y, w, h, 3, THEME_OUTLINE);
    tft.setTextColor(txt, bg);
    tft.drawString(factoryPresets[i].name, x + 4, y + 5, 2);
  }

  // CLOSE button
  tft.fillRoundRect(110, 213, 100, 24, 4, THEME_PRIMARY);
  tft.drawRoundRect(110, 213, 100, 24, 4, THEME_OUTLINE);
  tft.setTextColor(THEME_BG, THEME_PRIMARY);
  tft.drawCentreString("CLOSE", 160, 219, 2);
}

// ── Main seq draw ─────────────────────────────────────────────────────────
void zombieSeqDraw() {
  int  currentStep = zombieSeq->getCurrentStep();
  bool isPlaying   = zombieSeq->getIsPlaying();

  bool needsUpdate = seqNeedsRedraw || (currentStep != seqLastStep) || (isPlaying != seqLastPlaying);
  if (!needsUpdate) return;

  if (seqSoundSelect) {
    drawSeqSoundSelect();
    seqNeedsRedraw = false;
    return;
  }

  // ── Header ───────────────────────────────────────────────────────────
  if (seqNeedsRedraw) {
    tft.fillRect(0, 0, 320, 50, THEME_BG);
    tft.drawRect(0, 0, 320, 50, THEME_OUTLINE);
    tft.drawRect(1, 1, 318, 48, THEME_OUTLINE);
    tft.setTextColor(THEME_PRIMARY, THEME_BG);
    tft.drawString("ZOMBIE SS", 95, 8, 6);
    tft.setTextColor(THEME_ACCENT, THEME_BG);
    tft.drawString("SEQUENCER", 95, 35, 2);
    tft.fillRoundRect(5, 5, 55, 20, 4, THEME_PRIMARY);
    tft.drawRoundRect(5, 5, 55, 20, 4, THEME_OUTLINE);
    tft.setTextColor(THEME_BG, THEME_PRIMARY);
    tft.drawString("BACK", 15, 8, 2);
  }

  // ── Status bar ───────────────────────────────────────────────────────
  tft.fillRect(0, 53, 320, 22, THEME_BG);
  char statusBuf[50];
  sprintf(statusBuf, "BPM:%.0f | T%d | STEP:%d | %s",
          zombieSeq->getBPM(), seqEditTrack+1, currentStep+1,
          isPlaying ? "PLAYING" : "STOPPED");
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawCentreString(statusBuf, 160, 57, 2);

  // ── Track selector row with SOUND button ─────────────────────────────
  if (seqNeedsRedraw) {
    tft.fillRect(0, 78, 320, 30, THEME_BG);

    for (int t = 0; t < 4; t++) {
      int x = 5 + t * 62;
      bool sel = (t == seqEditTrack);
      uint16_t bg  = sel ? THEME_PRIMARY : THEME_BG;
      uint16_t txt = sel ? THEME_BG : THEME_PRIMARY;
      tft.fillRoundRect(x, 80, 58, 25, 4, bg);
      tft.drawRoundRect(x, 80, 58, 25, 4, THEME_OUTLINE);
      tft.setTextColor(txt, bg);
      // Show track number + first 3 chars of sound name
      char tbuf[10];
      SequencerTrack* tr = zombieSeq->getTrack(t);
      int pIdx = tr ? constrain(tr->soundPatchIdx, 0, NUM_FACTORY-1) : 0;
      snprintf(tbuf, sizeof(tbuf), "T%d %c%c%c", t+1,
               factoryPresets[pIdx].name[0],
               factoryPresets[pIdx].name[1],
               factoryPresets[pIdx].name[2]);
      tft.drawString(tbuf, x + 2, 87, 2);
    }

    // SOUND button (opens sub-menu for current track)
    tft.fillRoundRect(254, 80, 62, 25, 4, THEME_BG);
    tft.drawRoundRect(254, 80, 62, 25, 4, THEME_OUTLINE);
    tft.setTextColor(THEME_PRIMARY, THEME_BG);
    tft.drawCentreString("SOUND", 285, 87, 2);
  }

  // ── 16-step grid (always update for playback position) ────────────────
  tft.fillRect(0, 112, 320, 80, THEME_BG);
  SequencerTrack* track = zombieSeq->getTrack(seqEditTrack);
  if (track) {
    for (int s = 0; s < 16; s++) {
      int x = 5 + (s % 8) * 38;
      int y = 115 + (s / 8) * 36;
      int w = 36, h = 32;
      bool active  = track->steps[s].active;
      bool playing = (s == currentStep && isPlaying);
      drawSeqStep(x, y, w, h, active, playing);
      uint16_t nc = (active && !playing) ? THEME_BG : THEME_PRIMARY;
      if (active && playing) nc = THEME_BG;
      uint16_t nb = active ? (playing ? THEME_ACCENT : THEME_PRIMARY) : THEME_BG;
      tft.setTextColor(nc, nb);
      tft.drawCentreString(String(s+1).c_str(), x + w/2, y + h/2 - 7, 2);
    }
  }

  // ── Controls ──────────────────────────────────────────────────────────
  if (seqNeedsRedraw) {
    tft.fillRect(0, 198, 320, 42, THEME_BG);

    // PLAY/STOP
    uint16_t playBg  = isPlaying ? THEME_PRIMARY : THEME_BG;
    uint16_t playTxt = isPlaying ? THEME_BG : THEME_PRIMARY;
    tft.fillRoundRect(5,  202, 58, 33, 4, playBg);
    tft.drawRoundRect(5,  202, 58, 33, 4, THEME_OUTLINE);
    tft.setTextColor(playTxt, playBg);
    tft.drawCentreString(isPlaying ? "STOP" : "PLAY", 34, 211, 2);

    // CLEAR
    tft.fillRoundRect(68, 202, 55, 33, 4, THEME_BG);
    tft.drawRoundRect(68, 202, 55, 33, 4, THEME_OUTLINE);
    tft.setTextColor(THEME_PRIMARY, THEME_BG);
    tft.drawCentreString("CLEAR", 95, 211, 2);

    // RAND
    tft.fillRoundRect(128,202, 55, 33, 4, THEME_BG);
    tft.drawRoundRect(128,202, 55, 33, 4, THEME_OUTLINE);
    tft.setTextColor(THEME_PRIMARY, THEME_BG);
    tft.drawCentreString("RAND", 155, 211, 2);

    // < BPM
    tft.fillRoundRect(188,202, 58, 33, 4, THEME_BG);
    tft.drawRoundRect(188,202, 58, 33, 4, THEME_OUTLINE);
    tft.setTextColor(THEME_PRIMARY, THEME_BG);
    tft.drawCentreString("< BPM", 217, 211, 2);

    // BPM >
    tft.fillRoundRect(250,202, 65, 33, 4, THEME_BG);
    tft.drawRoundRect(250,202, 65, 33, 4, THEME_OUTLINE);
    tft.setTextColor(THEME_PRIMARY, THEME_BG);
    tft.drawCentreString("BPM >", 282, 211, 2);
  }

  seqNeedsRedraw  = false;
  seqLastStep     = currentStep;
  seqLastPlaying  = isPlaying;
}

// ── Touch handler ─────────────────────────────────────────────────────────
void zombieSeqHandleTouch() {
  if (!touch.justPressed) return;

  // ── Sound select overlay takes priority ──────────────────────────────
  if (seqSoundSelect) {
    // CLOSE button
    if (isButtonPressed(110, 213, 100, 24)) {
      seqSoundSelect = false;
      tft.fillScreen(THEME_BG);
      seqNeedsRedraw = true;
      return;
    }
    // Preset selection (2 columns of 5)
    for (int i = 0; i < NUM_FACTORY; i++) {
      int col = i / 5;
      int row = i % 5;
      int x = 25 + col * 138;
      int y = 70 + row * 27;
      if (isButtonPressed(x, y, 133, 24)) {
        SequencerTrack* tr = zombieSeq->getTrack(seqEditTrack);
        if (tr) tr->soundPatchIdx = i;
        seqSoundSelect = false;
        tft.fillScreen(THEME_BG);
        seqNeedsRedraw = true;
        return;
      }
    }
    return;
  }

  // BACK
  if (isButtonPressed(5, 5, 55, 20)) { exitToMenu(); return; }

  // Track selection
  for (int t = 0; t < 4; t++) {
    if (isButtonPressed(5 + t*62, 80, 58, 25)) {
      seqEditTrack = t;
      zombieSeq->setActiveTrack(t);
      seqNeedsRedraw = true;
      return;
    }
  }

  // SOUND button → open sound select sub-menu
  if (isButtonPressed(254, 80, 62, 25)) {
    seqSoundSelect = true;
    seqNeedsRedraw = true;
    return;
  }

  // Step grid
  for (int s = 0; s < 16; s++) {
    int x = 5 + (s % 8) * 38;
    int y = 115 + (s / 8) * 36;
    if (isButtonPressed(x, y, 36, 32)) {
      zombieSeq->toggleStep(seqEditTrack, s);
      SequencerTrack* track = zombieSeq->getTrack(seqEditTrack);
      if (track && track->steps[s].active) {
        track->steps[s].note     = seqEditNote;
        track->steps[s].velocity = 100;
      }
      seqNeedsRedraw = true;
      return;
    }
  }

  // PLAY/STOP
  if (isButtonPressed(5, 202, 58, 33)) {
    zombieSeq->getIsPlaying() ? zombieSeq->stop() : zombieSeq->play();
    seqNeedsRedraw = true;
  }
  // CLEAR
  if (isButtonPressed(68, 202, 55, 33)) { zombieSeq->clearTrack(seqEditTrack); seqNeedsRedraw = true; }
  // RAND
  if (isButtonPressed(128,202, 55, 33)) { zombieSeq->randomizeTrack(seqEditTrack); seqNeedsRedraw = true; }
  // BPM controls
  if (isButtonPressed(188,202, 58, 33)) { zombieSeq->setBPM(zombieSeq->getBPM() - 10.0f); seqNeedsRedraw = true; }
  if (isButtonPressed(250,202, 65, 33)) { zombieSeq->setBPM(zombieSeq->getBPM() + 10.0f); seqNeedsRedraw = true; }
}

void zombieSeqUpdate() {
  if (zombieSeq) zombieSeq->update(millis());
}

void zombieSeqInit() {
  if (zombieSeq == NULL) {
    zombieSeq = new ZombieSequencer();
    zombieSeq->setSynthEngine(getZombieSynth());
    zombieSeq->setCallbacks(seqNoteOnHandler, seqNoteOffHandler);
  }

  zombieSeq->init();
  zombieSeq->setBPM(120.0f);
  zombieSeq->setActiveTrack(0);
  // Assign distinct default sounds to each track
  for (int t = 0; t < 4; t++) {
    SequencerTrack* tr = zombieSeq->getTrack(t);
    if (tr) tr->soundPatchIdx = t;  // T1=PROPHET PAD, T2=BASS STAB, T3=LEAD SAW, T4=ZOMBIE PLUCK
  }

  seqEditTrack   = 0;
  seqEditNote    = 60;
  seqNeedsRedraw = true;
  seqLastStep    = -1;
  seqLastPlaying = false;
  seqSoundSelect = false;

  tft.fillScreen(THEME_BG);
}

ZombieSequencer* getZombieSeq() {
  return zombieSeq;
}

#endif
