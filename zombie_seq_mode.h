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

  tft.fillScreen(THEME_BG);
}

void drawSeqStep(int x, int y, int w, int h, bool active, bool playing) {
  uint16_t bgColor = THEME_BG;
  uint16_t borderColor = THEME_OUTLINE;

  if (active && playing) {
    bgColor = THEME_ACCENT;  // White when active and playing
  } else if (active) {
    bgColor = THEME_PRIMARY;  // Red when active
  } else if (playing) {
    borderColor = THEME_ACCENT;  // White border when playing
  }

  tft.fillRect(x, y, w, h, bgColor);
  tft.drawRect(x, y, w, h, borderColor);
}

void zombieSeqDraw() {
  static int lastStep = -1;
  static bool lastPlaying = false;

  int currentStep = zombieSeq->getCurrentStep();
  bool isPlaying = zombieSeq->getIsPlaying();

  // Header
  tft.fillRect(0, 0, 320, 45, THEME_BG);
  tft.drawRect(0, 0, 320, 45, THEME_OUTLINE);
  tft.drawRect(1, 1, 318, 43, THEME_OUTLINE);

  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString("ZOMBIE SS", 160, 8, 4);
  tft.setTextColor(THEME_ACCENT, THEME_BG);
  tft.drawCentreString("STEP SEQUENCER", 160, 28, 2);

  // Status
  tft.fillRect(0, 50, 320, 25, THEME_BG);
  char statusBuf[50];
  sprintf(statusBuf, "BPM:%.0f | TRACK:%d | %s",
          zombieSeq->getBPM(),
          seqEditTrack + 1,
          isPlaying ? "PLAYING" : "STOPPED");
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawCentreString(statusBuf, 160, 55, 2);

  // Track selector
  tft.fillRect(0, 80, 320, 30, THEME_BG);
  for (int t = 0; t < 4; t++) {
    int x = 10 + t * 75;
    bool selected = (t == seqEditTrack);
    drawButton(x, 85, 70, 25, String("T" + String(t + 1)).c_str(), selected);
  }

  // 16-step grid
  bool needsRedraw = (lastStep != currentStep || lastPlaying != isPlaying);
  if (needsRedraw) {
    tft.fillRect(0, 115, 320, 75, THEME_BG);

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
        tft.setTextColor(active ? THEME_BG : THEME_PRIMARY, active ? THEME_PRIMARY : THEME_BG);
        tft.drawCentreString(String(s + 1).c_str(), x + w / 2, y + h / 2 - 7, 2);
      }
    }

    lastStep = currentStep;
    lastPlaying = isPlaying;
  }

  // Controls
  tft.fillRect(0, 195, 320, 45, THEME_BG);

  drawButton(10, 200, 60, 35, isPlaying ? "STOP" : "PLAY", isPlaying);
  drawButton(75, 200, 60, 35, "CLEAR", false);
  drawButton(140, 200, 60, 35, "RAND", false);
  drawButton(205, 200, 50, 35, "< BPM", false);
  drawButton(260, 200, 50, 35, "BPM >", false);
}

void zombieSeqHandleTouch() {
  if (!touch.justPressed) return;

  // Track selection
  for (int t = 0; t < 4; t++) {
    int x = 10 + t * 75;
    if (isButtonPressed(x, 85, 70, 25)) {
      seqEditTrack = t;
      zombieSeq->setActiveTrack(t);
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
      return;
    }
  }

  // Play/Stop
  if (isButtonPressed(10, 200, 60, 35)) {
    if (zombieSeq->getIsPlaying()) {
      zombieSeq->stop();
    } else {
      zombieSeq->play();
    }
  }

  // Clear
  if (isButtonPressed(75, 200, 60, 35)) {
    zombieSeq->clearTrack(seqEditTrack);
  }

  // Randomize
  if (isButtonPressed(140, 200, 60, 35)) {
    zombieSeq->randomizeTrack(seqEditTrack);
  }

  // BPM controls
  if (isButtonPressed(205, 200, 50, 35)) {
    float bpm = zombieSeq->getBPM();
    zombieSeq->setBPM(bpm - 10.0f);
  }
  if (isButtonPressed(260, 200, 50, 35)) {
    float bpm = zombieSeq->getBPM();
    zombieSeq->setBPM(bpm + 10.0f);
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
