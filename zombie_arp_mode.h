#ifndef ZOMBIE_ARP_MODE_H
#define ZOMBIE_ARP_MODE_H

#include "common_definitions.h"
#include "ui_elements.h"
#include "arpeggiator_patterns.h"
#include "zombie_synth_mode.h"

// ZOMBIE SS Arpeggiator UI with 50 patterns

static Arpeggiator* zombieArp = NULL;
static int arpPatternPage = 0;  // 0-9 (5 patterns per page)
static bool arpRunning = false;

void zombieArpInit() {
  if (zombieArp == NULL) {
    zombieArp = new Arpeggiator();
  }

  zombieArp->setBPM(120.0f);
  zombieArp->setPattern(ARP_UP);
  zombieArp->setOctaveRange(2);
  zombieArp->setGateLength(80);

  arpPatternPage = 0;
  arpRunning = false;

  tft.fillScreen(THEME_BG);
}

void zombieArpDraw() {
  static int lastPattern = -1;
  static int lastPage = -1;

  // Header
  tft.fillRect(0, 0, 320, 45, THEME_BG);
  tft.drawRect(0, 0, 320, 45, THEME_OUTLINE);
  tft.drawRect(1, 1, 318, 43, THEME_OUTLINE);

  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString("ZOMBIE SS", 160, 8, 4);
  tft.setTextColor(THEME_ACCENT, THEME_BG);
  tft.drawCentreString("ARPEGGIATOR", 160, 28, 2);

  // Status
  tft.fillRect(0, 50, 320, 30, THEME_BG);
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  char statusBuf[50];
  sprintf(statusBuf, "BPM:%.0f | NOTES:%d | %s",
          zombieArp->getBPM(),
          zombieArp->getNoteCount(),
          arpRunning ? "RUNNING" : "STOPPED");
  tft.drawCentreString(statusBuf, 160, 55, 2);

  // Pattern selection (5 patterns per page, 10 pages total)
  if (lastPage != arpPatternPage) {
    lastPage = arpPatternPage;
    tft.fillRect(0, 85, 320, 110, THEME_BG);

    int startPattern = arpPatternPage * 5;
    for (int i = 0; i < 5; i++) {
      int patternIdx = startPattern + i;
      if (patternIdx >= NUM_ARP_PATTERNS) break;

      int x = 10;
      int y = 90 + i * 21;
      int w = 300;
      int h = 18;

      bool selected = (patternIdx == (int)zombieArp->getPattern());
      uint16_t bgColor = selected ? THEME_PRIMARY : THEME_BG;
      uint16_t textColor = selected ? THEME_BG : THEME_PRIMARY;

      tft.fillRoundRect(x, y, w, h, 3, bgColor);
      tft.drawRoundRect(x, y, w, h, 3, THEME_OUTLINE);

      tft.setTextColor(textColor, bgColor);
      char buf[50];
      sprintf(buf, "%02d: %s", patternIdx, arpPatternNames[patternIdx]);
      tft.drawString(buf, x + 5, y + 3, 2);
    }
  }

  // Page navigation
  tft.fillRect(0, 200, 320, 40, THEME_BG);
  drawButton(10, 205, 60, 30, "< PG", false);
  drawButton(75, 205, 60, 30, "> PG", false);

  char pageBuf[20];
  sprintf(pageBuf, "PAGE %d/10", arpPatternPage + 1);
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString(pageBuf, 180, 212, 2);

  drawButton(250, 205, 60, 30, arpRunning ? "STOP" : "PLAY", arpRunning);
}

void zombieArpHandleTouch() {
  if (!touch.justPressed) return;

  // Pattern selection
  int startPattern = arpPatternPage * 5;
  for (int i = 0; i < 5; i++) {
    int patternIdx = startPattern + i;
    if (patternIdx >= NUM_ARP_PATTERNS) break;

    int x = 10;
    int y = 90 + i * 21;
    int w = 300;
    int h = 18;

    if (isButtonPressed(x, y, w, h)) {
      zombieArp->setPattern((ArpPattern)patternIdx);
      return;
    }
  }

  // Page navigation
  if (isButtonPressed(10, 205, 60, 30)) {
    arpPatternPage = (arpPatternPage - 1 + 10) % 10;
  }
  if (isButtonPressed(75, 205, 60, 30)) {
    arpPatternPage = (arpPatternPage + 1) % 10;
  }

  // Play/Stop
  if (isButtonPressed(250, 205, 60, 30)) {
    arpRunning = !arpRunning;
    if (!arpRunning) {
      zombieArp->allNotesOff();
      if (getZombieSynth()) {
        getZombieSynth()->allNotesOff();
      }
    }
  }
}

void zombieArpUpdate() {
  if (!zombieArp || !arpRunning) return;

  int note = zombieArp->update(millis());
  if (note >= 0 && getZombieSynth()) {
    getZombieSynth()->noteOn(note, 100);

    // Schedule note off based on gate length
    // This is simplified - in production would use proper timing
  }
}

Arpeggiator* getZombieArp() {
  return zombieArp;
}

#endif
