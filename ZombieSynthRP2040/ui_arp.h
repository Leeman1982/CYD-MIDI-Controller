#ifndef UI_ARP_H
#define UI_ARP_H

#include "config.h"
#include "oled_ui.h"
#include "arpeggiator_patterns.h"
#include "synth_engine.h"

// ── Arpeggiator UI ────────────────────────────────────────────────────────────
// Encoder: scroll patterns / adjust BPM
// Confirm: play/stop
// Back: exit

extern SynthEngine synthEngine;
extern Arpeggiator arpeggiator;
extern bool        arpRunning;

namespace UIArp {

  enum ArpFocus { FOCUS_PATTERN, FOCUS_BPM, FOCUS_OCTAVE, FOCUS_GATE };
  static ArpFocus  focus = FOCUS_PATTERN;
  static int       patternScroll = 0;
  static bool      editing = false;

  void enter() {
    focus = FOCUS_PATTERN;
    patternScroll = (int)arpeggiator.getPattern();
    if (patternScroll > 2) patternScroll -= 2;
    else patternScroll = 0;
    editing = false;
  }

  void draw() {
    OledUI::clear();

    // Header
    char hdr[20];
    snprintf(hdr, sizeof(hdr), "%s", arpRunning ? "PLAY" : "STOP");
    OledUI::drawHeader("ARP", hdr);

    // BPM display
    char bpmBuf[16];
    snprintf(bpmBuf, sizeof(bpmBuf), "%.0f BPM", arpeggiator.getBPM());

    int y = HEADER_H + 2;
    display.setFont(FONT_MEDIUM);

    bool bpmSel = (focus == FOCUS_BPM);
    if (bpmSel && editing) {
      int bw = display.getStrWidth(bpmBuf);
      display.drawBox(0, y, bw + 4, LINE_H);
      display.setDrawColor(0);
      display.drawStr(2, y + LINE_H - 2, bpmBuf);
      display.setDrawColor(1);
    } else if (bpmSel) {
      display.drawBox(0, y, 128, LINE_H);
      display.setDrawColor(0);
      display.drawStr(2, y + LINE_H - 2, bpmBuf);
      display.setDrawColor(1);
    } else {
      display.drawStr(2, y + LINE_H - 2, bpmBuf);
    }

    // Octave + Gate on same line
    char octBuf[8], gateBuf[8];
    snprintf(octBuf, sizeof(octBuf), "Oct:%d", 2);
    snprintf(gateBuf, sizeof(gateBuf), "G:%d%%", arpeggiator.getGateLength());
    display.drawStr(70, y + LINE_H - 2, octBuf);
    display.drawStr(100, y + LINE_H - 2, gateBuf);

    // Pattern list (4 visible patterns)
    int curPat = (int)arpeggiator.getPattern();
    y = HEADER_H + 2 + LINE_H + 2;

    for (int i = 0; i < 4; i++) {
      int pIdx = patternScroll + i;
      if (pIdx >= NUM_ARP_PATTERNS) break;

      int ly = y + i * LINE_H;
      bool isCurrent = (pIdx == curPat);
      bool isSel = (focus == FOCUS_PATTERN && pIdx == curPat);

      char line[24];
      snprintf(line, sizeof(line), "%02d %s", pIdx, arpPatternNames[pIdx]);

      if (isCurrent) {
        display.drawBox(0, ly, 128, LINE_H);
        display.setDrawColor(0);
        display.drawStr(2, ly + LINE_H - 2, line);
        display.setDrawColor(1);
      } else {
        display.drawStr(2, ly + LINE_H - 2, line);
      }
    }

    // Status bar
    char status[24];
    snprintf(status, sizeof(status), "Notes:%d %s",
             arpeggiator.getNoteCount(),
             arpRunning ? "RUN" : "---");
    OledUI::drawStatusBar(status);

    OledUI::send();
  }

  bool handleInput(InputEvent evt) {
    switch (evt) {
      case EVT_ENC_CW:
        if (focus == FOCUS_BPM && editing) {
          arpeggiator.setBPM(arpeggiator.getBPM() + 1.0f);
        } else if (focus == FOCUS_PATTERN) {
          int p = (int)arpeggiator.getPattern() + 1;
          if (p < NUM_ARP_PATTERNS) {
            arpeggiator.setPattern((ArpPattern)p);
            if (p >= patternScroll + 4) patternScroll = p - 3;
          }
        } else {
          // Cycle focus
          focus = (ArpFocus)((int)focus + 1);
          if ((int)focus > FOCUS_GATE) focus = FOCUS_PATTERN;
        }
        break;

      case EVT_ENC_CCW:
        if (focus == FOCUS_BPM && editing) {
          arpeggiator.setBPM(arpeggiator.getBPM() - 1.0f);
        } else if (focus == FOCUS_PATTERN) {
          int p = (int)arpeggiator.getPattern() - 1;
          if (p >= 0) {
            arpeggiator.setPattern((ArpPattern)p);
            if (p < patternScroll) patternScroll = p;
          }
        } else {
          focus = (ArpFocus)((int)focus - 1);
          if ((int)focus < 0) focus = FOCUS_GATE;
        }
        break;

      case EVT_ENC_PRESS:
        if (focus == FOCUS_BPM) editing = !editing;
        else if (focus == FOCUS_PATTERN) {
          // Toggle between BPM and pattern focus
          focus = FOCUS_BPM;
        }
        break;

      case EVT_CONFIRM_PRESS:
        arpRunning = !arpRunning;
        if (!arpRunning) {
          arpeggiator.allNotesOff();
          synthEngine.allNotesOff();
        }
        break;

      case EVT_BACK_PRESS:
        if (editing) {
          editing = false;
        } else {
          return false; // Exit to menu
        }
        break;

      default: break;
    }
    return true;
  }

} // namespace UIArp

#endif
