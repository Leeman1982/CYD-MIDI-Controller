#ifndef ZOMBIE_ARP_MODE_H
#define ZOMBIE_ARP_MODE_H

#include "common_definitions.h"
#include "ui_elements.h"
#include "arpeggiator_patterns.h"
#include "zombie_synth_mode.h"

// ZOMBIE SS Arpeggiator UI with 50 patterns

static Arpeggiator* zombieArp = NULL;
static int arpPatternPage = 0;
static bool arpRunning = false;
static bool arpNeedsRedraw = true;

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
  arpNeedsRedraw = true;

  tft.fillScreen(THEME_BG);
}

void zombieArpDraw() {
  if (!arpNeedsRedraw) return;

  // Header
  tft.fillRect(0, 0, 320, 50, THEME_BG);
  tft.drawRect(0, 0, 320, 50, THEME_OUTLINE);
  tft.drawRect(1, 1, 318, 48, THEME_OUTLINE);

  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawString("ZOMBIE SS", 95, 8, 6);  // Large bold font
  tft.setTextColor(THEME_ACCENT, THEME_BG);
  tft.drawString("ARPEGGIATOR", 95, 35, 2);

  // BACK button
  tft.fillRoundRect(5, 5, 55, 20, 4, THEME_PRIMARY);
  tft.drawRoundRect(5, 5, 55, 20, 4, THEME_OUTLINE);
  tft.setTextColor(THEME_BG, THEME_PRIMARY);
  tft.drawString("BACK", 15, 8, 2);

  // Status
  tft.fillRect(0, 55, 320, 25, THEME_BG);
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  char statusBuf[50];
  sprintf(statusBuf, "BPM:%.0f | NOTES:%d | %s",
          zombieArp->getBPM(),
          zombieArp->getNoteCount(),
          arpRunning ? "RUNNING" : "STOPPED");
  tft.drawCentreString(statusBuf, 160, 60, 2);

  // Pattern list - 5 patterns per page
  tft.fillRect(0, 85, 320, 110, THEME_BG);
  int startPattern = arpPatternPage * 5;
  for (int i = 0; i < 5; i++) {
    int patternIdx = startPattern + i;
    if (patternIdx >= NUM_ARP_PATTERNS) break;

    int x = 10;
    int y = 88 + i * 21;
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
    tft.drawString(buf, x + 8, y + 3, 2);
  }

  // Page navigation
  tft.fillRect(0, 200, 320, 40, THEME_BG);

  // < PG button
  uint16_t pgPrevBg = THEME_BG;
  tft.fillRoundRect(10, 205, 60, 30, 4, pgPrevBg);
  tft.drawRoundRect(10, 205, 60, 30, 4, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, pgPrevBg);
  tft.drawCentreString("< PG", 40, 213, 2);

  // > PG button
  uint16_t pgNextBg = THEME_BG;
  tft.fillRoundRect(75, 205, 60, 30, 4, pgNextBg);
  tft.drawRoundRect(75, 205, 60, 30, 4, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, pgNextBg);
  tft.drawCentreString("> PG", 105, 213, 2);

  // Page indicator
  char pageBuf[20];
  sprintf(pageBuf, "PAGE %d/10", arpPatternPage + 1);
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString(pageBuf, 180, 213, 2);

  // PLAY/STOP button
  uint16_t playBg = arpRunning ? THEME_PRIMARY : THEME_BG;
  uint16_t playTxt = arpRunning ? THEME_BG : THEME_PRIMARY;
  tft.fillRoundRect(250, 205, 60, 30, 4, playBg);
  tft.drawRoundRect(250, 205, 60, 30, 4, THEME_OUTLINE);
  tft.setTextColor(playTxt, playBg);
  tft.drawCentreString(arpRunning ? "STOP" : "PLAY", 280, 213, 2);

  arpNeedsRedraw = false;
}

void zombieArpHandleTouch() {
  if (!touch.justPressed) return;

  // BACK button
  if (isButtonPressed(5, 5, 55, 20)) {
    exitToMenu();
    return;
  }

  // Pattern selection
  int startPattern = arpPatternPage * 5;
  for (int i = 0; i < 5; i++) {
    int patternIdx = startPattern + i;
    if (patternIdx >= NUM_ARP_PATTERNS) break;

    int x = 10;
    int y = 88 + i * 21;
    int w = 300;
    int h = 18;

    if (isButtonPressed(x, y, w, h)) {
      zombieArp->setPattern((ArpPattern)patternIdx);
      arpNeedsRedraw = true;
      return;
    }
  }

  // Page navigation
  if (isButtonPressed(10, 205, 60, 30)) {
    arpPatternPage = (arpPatternPage - 1 + 10) % 10;
    arpNeedsRedraw = true;
  }
  if (isButtonPressed(75, 205, 60, 30)) {
    arpPatternPage = (arpPatternPage + 1) % 10;
    arpNeedsRedraw = true;
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
    arpNeedsRedraw = true;
  }
}

void zombieArpUpdate() {
  if (!zombieArp || !arpRunning) return;

  int note = zombieArp->update(millis());
  if (note >= 0 && getZombieSynth()) {
    getZombieSynth()->noteOn(note, 100);
  }
}

Arpeggiator* getZombieArp() {
  return zombieArp;
}

#endif
