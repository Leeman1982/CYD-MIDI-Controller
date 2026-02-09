#ifndef ZOMBIE_SEQ_MODE_H
#define ZOMBIE_SEQ_MODE_H

#include "common_definitions.h"
#include "ui_elements.h"
#include "zombie_step_sequencer.h"
#include "zombie_synth_mode.h"

// ZOMBIE SS Step Sequencer UI

static ZombieSequencer* zombieSeq = NULL;
static int seqEditTrack = 0;
static int seqEditNote = 60;
static bool seqNeedsRedraw = true;
static int seqLastStep = -1;
static bool seqLastPlaying = false;

void zombieSeqInit() {
  if (zombieSeq == NULL) {
    zombieSeq = new ZombieSequencer();
    zombieSeq->setSynthEngine(getZombieSynth());
  }

  zombieSeq->init();
  zombieSeq->setBPM(120.0f);
  zombieSeq->setActiveTrack(0);

  seqEditTrack = 0;
  seqEditNote = 60;
  seqNeedsRedraw = true;
  seqLastStep = -1;
  seqLastPlaying = false;

  tft.fillScreen(THEME_BG);
}

void drawSeqStep(int x, int y, int w, int h, bool active, bool playing) {
  uint16_t bgColor = THEME_BG;
  uint16_t borderColor = THEME_OUTLINE;
  uint16_t textColor = THEME_PRIMARY;

  if (active && playing) {
    bgColor = THEME_ACCENT;  // White when active and playing
    textColor = THEME_BG;
  } else if (active) {
    bgColor = THEME_PRIMARY;  // Red when active
    textColor = THEME_BG;
  } else if (playing) {
    borderColor = THEME_ACCENT;  // White border when playing
  }

  tft.fillRoundRect(x, y, w, h, 3, bgColor);
  tft.drawRoundRect(x, y, w, h, 3, borderColor);
}

void zombieSeqDraw() {
  int currentStep = zombieSeq->getCurrentStep();
  bool isPlaying = zombieSeq->getIsPlaying();

  // Check if we need to redraw
  bool needsUpdate = seqNeedsRedraw || (currentStep != seqLastStep) || (isPlaying != seqLastPlaying);
  if (!needsUpdate) return;

  // Header
  if (seqNeedsRedraw) {
    tft.fillRect(0, 0, 320, 50, THEME_BG);
    tft.drawRect(0, 0, 320, 50, THEME_OUTLINE);
    tft.drawRect(1, 1, 318, 48, THEME_OUTLINE);

    tft.setTextColor(THEME_PRIMARY, THEME_BG);
    tft.drawString("ZOMBIE SS", 95, 8, 6);  // Large bold font
    tft.setTextColor(THEME_ACCENT, THEME_BG);
    tft.drawString("SEQUENCER", 95, 35, 2);

    // BACK button
    tft.fillRoundRect(5, 5, 55, 20, 4, THEME_PRIMARY);
    tft.drawRoundRect(5, 5, 55, 20, 4, THEME_OUTLINE);
    tft.setTextColor(THEME_BG, THEME_PRIMARY);
    tft.drawString("BACK", 15, 8, 2);
  }

  // Status - always update to show current step
  tft.fillRect(0, 55, 320, 25, THEME_BG);
  char statusBuf[50];
  sprintf(statusBuf, "BPM:%.0f | TRACK:%d | STEP:%d | %s",
          zombieSeq->getBPM(),
          seqEditTrack + 1,
          currentStep + 1,
          isPlaying ? "PLAYING" : "STOPPED");
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawCentreString(statusBuf, 160, 60, 2);

  // Track selector
  if (seqNeedsRedraw) {
    tft.fillRect(0, 82, 320, 30, THEME_BG);
    for (int t = 0; t < 4; t++) {
      int x = 10 + t * 75;
      bool selected = (t == seqEditTrack);
      uint16_t bgColor = selected ? THEME_PRIMARY : THEME_BG;
      uint16_t textColor = selected ? THEME_BG : THEME_PRIMARY;

      tft.fillRoundRect(x, 87, 70, 25, 4, bgColor);
      tft.drawRoundRect(x, 87, 70, 25, 4, THEME_OUTLINE);
      tft.setTextColor(textColor, bgColor);
      char buf[10];
      sprintf(buf, "T%d", t + 1);
      tft.drawCentreString(buf, x + 35, 94, 2);
    }
  }

  // 16-step grid - always update to show playback position
  tft.fillRect(0, 117, 320, 75, THEME_BG);
  SequencerTrack* track = zombieSeq->getTrack(seqEditTrack);
  if (track) {
    for (int s = 0; s < 16; s++) {
      int x = 5 + (s % 8) * 38;
      int y = 120 + (s / 8) * 35;
      int w = 36;
      int h = 30;

      bool active = track->steps[s].active;
      bool playing = (s == currentStep && isPlaying);

      drawSeqStep(x, y, w, h, active, playing);

      // Step number
      uint16_t numColor = (active && !playing) ? THEME_BG : THEME_PRIMARY;
      if (active && playing) numColor = THEME_BG;

      tft.setTextColor(numColor, active ? (playing ? THEME_ACCENT : THEME_PRIMARY) : THEME_BG);
      tft.drawCentreString(String(s + 1).c_str(), x + w / 2, y + h / 2 - 7, 2);
    }
  }

  // Controls
  if (seqNeedsRedraw) {
    tft.fillRect(0, 197, 320, 43, THEME_BG);

    // PLAY/STOP
    uint16_t playBg = isPlaying ? THEME_PRIMARY : THEME_BG;
    uint16_t playTxt = isPlaying ? THEME_BG : THEME_PRIMARY;
    tft.fillRoundRect(10, 202, 60, 33, 4, playBg);
    tft.drawRoundRect(10, 202, 60, 33, 4, THEME_OUTLINE);
    tft.setTextColor(playTxt, playBg);
    tft.drawCentreString(isPlaying ? "STOP" : "PLAY", 40, 211, 2);

    // CLEAR
    tft.fillRoundRect(75, 202, 60, 33, 4, THEME_BG);
    tft.drawRoundRect(75, 202, 60, 33, 4, THEME_OUTLINE);
    tft.setTextColor(THEME_PRIMARY, THEME_BG);
    tft.drawCentreString("CLEAR", 105, 211, 2);

    // RAND
    tft.fillRoundRect(140, 202, 60, 33, 4, THEME_BG);
    tft.drawRoundRect(140, 202, 60, 33, 4, THEME_OUTLINE);
    tft.setTextColor(THEME_PRIMARY, THEME_BG);
    tft.drawCentreString("RAND", 170, 211, 2);

    // BPM -
    tft.fillRoundRect(205, 202, 50, 33, 4, THEME_BG);
    tft.drawRoundRect(205, 202, 50, 33, 4, THEME_OUTLINE);
    tft.setTextColor(THEME_PRIMARY, THEME_BG);
    tft.drawCentreString("< BPM", 230, 211, 2);

    // BPM +
    tft.fillRoundRect(260, 202, 50, 33, 4, THEME_BG);
    tft.drawRoundRect(260, 202, 50, 33, 4, THEME_OUTLINE);
    tft.setTextColor(THEME_PRIMARY, THEME_BG);
    tft.drawCentreString("BPM >", 285, 211, 2);
  }

  seqNeedsRedraw = false;
  seqLastStep = currentStep;
  seqLastPlaying = isPlaying;
}

void zombieSeqHandleTouch() {
  if (!touch.justPressed) return;

  // BACK button
  if (isButtonPressed(5, 5, 55, 20)) {
    exitToMenu();
    return;
  }

  // Track selection
  for (int t = 0; t < 4; t++) {
    int x = 10 + t * 75;
    if (isButtonPressed(x, 87, 70, 25)) {
      seqEditTrack = t;
      zombieSeq->setActiveTrack(t);
      seqNeedsRedraw = true;
      return;
    }
  }

  // Step grid
  for (int s = 0; s < 16; s++) {
    int x = 5 + (s % 8) * 38;
    int y = 120 + (s / 8) * 35;
    int w = 36;
    int h = 30;

    if (isButtonPressed(x, y, w, h)) {
      zombieSeq->toggleStep(seqEditTrack, s);
      SequencerTrack* track = zombieSeq->getTrack(seqEditTrack);
      if (track && track->steps[s].active) {
        track->steps[s].note = seqEditNote;
        track->steps[s].velocity = 100;
      }
      seqNeedsRedraw = true;
      return;
    }
  }

  // Play/Stop
  if (isButtonPressed(10, 202, 60, 33)) {
    if (zombieSeq->getIsPlaying()) {
      zombieSeq->stop();
    } else {
      zombieSeq->play();
    }
    seqNeedsRedraw = true;
  }

  // Clear
  if (isButtonPressed(75, 202, 60, 33)) {
    zombieSeq->clearTrack(seqEditTrack);
    seqNeedsRedraw = true;
  }

  // Randomize
  if (isButtonPressed(140, 202, 60, 33)) {
    zombieSeq->randomizeTrack(seqEditTrack);
    seqNeedsRedraw = true;
  }

  // BPM controls
  if (isButtonPressed(205, 202, 50, 33)) {
    float bpm = zombieSeq->getBPM();
    zombieSeq->setBPM(bpm - 10.0f);
    seqNeedsRedraw = true;
  }
  if (isButtonPressed(260, 202, 50, 33)) {
    float bpm = zombieSeq->getBPM();
    zombieSeq->setBPM(bpm + 10.0f);
    seqNeedsRedraw = true;
  }
}

void zombieSeqUpdate() {
  if (zombieSeq) {
    zombieSeq->update(millis());
  }
}

ZombieSequencer* getZombieSeq() {
  return zombieSeq;
}

#endif
