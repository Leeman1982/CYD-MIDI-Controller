#ifndef UI_CHORD_H
#define UI_CHORD_H

#include "config.h"
#include "oled_ui.h"
#include "synth_engine.h"

// ── Chord Pad UI ──────────────────────────────────────────────────────────────
// Choose root + chord type, encoder to select, press to play
// 8 chord types × 12 roots × octave control

extern SynthEngine synthEngine;
extern int lastPlayedMidiNote;

namespace UIChord {

  struct ChordType {
    const char* name;
    int intervals[5];
    int numNotes;
  };

  static const ChordType chordTypes[] = {
    {"MAJ",  {0, 4, 7, -1, -1}, 3},
    {"MIN",  {0, 3, 7, -1, -1}, 3},
    {"7th",  {0, 4, 7, 10, -1}, 4},
    {"MAJ7", {0, 4, 7, 11, -1}, 4},
    {"MIN7", {0, 3, 7, 10, -1}, 4},
    {"DIM",  {0, 3, 6, -1, -1}, 3},
    {"AUG",  {0, 4, 8, -1, -1}, 3},
    {"SUS4", {0, 5, 7, -1, -1}, 3},
  };
  static const int NUM_CHORD_TYPES = 8;

  static const char* rootNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

  enum ChordFocus { CF_ROOT, CF_TYPE, CF_OCTAVE };

  static int  rootNote = 0;     // 0=C .. 11=B
  static int  typeIdx  = 0;     // 0=MAJ
  static int  octave   = 4;
  static bool holdMode = false;
  static ChordFocus focus = CF_ROOT;
  static int  activeNotes[5];
  static int  activeCount = 0;

  void enter() {
    focus = CF_ROOT;
    activeCount = 0;
  }

  void allOff() {
    for (int i = 0; i < activeCount; i++)
      if (activeNotes[i] >= 0) synthEngine.noteOff(activeNotes[i]);
    activeCount = 0;
  }

  void playChord() {
    if (!holdMode) allOff();

    const ChordType& ct = chordTypes[typeIdx];
    int baseNote = (octave + 1) * 12 + rootNote;

    activeCount = 0;
    for (int i = 0; i < ct.numNotes; i++) {
      int note = baseNote + ct.intervals[i];
      if (note >= 0 && note <= 127) {
        synthEngine.noteOn(note, 100);
        activeNotes[activeCount++] = note;
        lastPlayedMidiNote = note;
      }
    }
  }

  void draw() {
    OledUI::clear();

    // Header
    char chordName[16];
    snprintf(chordName, sizeof(chordName), "%s %s", rootNames[rootNote], chordTypes[typeIdx].name);
    OledUI::drawHeader("CHORD", chordName);

    int y = HEADER_H + 4;
    display.setFont(FONT_MEDIUM);

    // Root note row
    {
      char buf[16];
      snprintf(buf, sizeof(buf), "Root: %s", rootNames[rootNote]);
      bool sel = (focus == CF_ROOT);
      if (sel) {
        display.drawBox(0, y, 128, LINE_H);
        display.setDrawColor(0);
        display.drawStr(2, y + LINE_H - 2, buf);
        display.setDrawColor(1);
      } else {
        display.drawStr(2, y + LINE_H - 2, buf);
      }
    }

    y += LINE_H + 2;

    // Chord type row
    {
      char buf[16];
      snprintf(buf, sizeof(buf), "Type: %s", chordTypes[typeIdx].name);
      bool sel = (focus == CF_TYPE);
      if (sel) {
        display.drawBox(0, y, 128, LINE_H);
        display.setDrawColor(0);
        display.drawStr(2, y + LINE_H - 2, buf);
        display.setDrawColor(1);
      } else {
        display.drawStr(2, y + LINE_H - 2, buf);
      }
    }

    y += LINE_H + 2;

    // Octave row
    {
      char buf[16];
      snprintf(buf, sizeof(buf), "Octave: %d", octave);
      bool sel = (focus == CF_OCTAVE);
      if (sel) {
        display.drawBox(0, y, 128, LINE_H);
        display.setDrawColor(0);
        display.drawStr(2, y + LINE_H - 2, buf);
        display.setDrawColor(1);
      } else {
        display.drawStr(2, y + LINE_H - 2, buf);
      }
    }

    y += LINE_H + 4;

    // Hold mode indicator
    display.setFont(FONT_SMALL);
    char holdBuf[16];
    snprintf(holdBuf, sizeof(holdBuf), "Hold: %s", holdMode ? "ON" : "OFF");
    display.drawStr(2, y + 7, holdBuf);

    // Active notes display
    if (activeCount > 0) {
      char noteBuf[32] = "";
      for (int i = 0; i < activeCount; i++) {
        if (i > 0) strcat(noteBuf, " ");
        strcat(noteBuf, OledUI::noteName(activeNotes[i]));
      }
      display.drawStr(50, y + 7, noteBuf);
    }

    OledUI::drawStatusBar("Enc:sel Press:play Cfm:hold");

    OledUI::send();
  }

  bool handleInput(InputEvent evt) {
    switch (evt) {
      case EVT_ENC_CW:
        switch (focus) {
          case CF_ROOT:   rootNote = (rootNote + 1) % 12; break;
          case CF_TYPE:   typeIdx = (typeIdx + 1) % NUM_CHORD_TYPES; break;
          case CF_OCTAVE: octave = min(octave + 1, 7); break;
        }
        break;

      case EVT_ENC_CCW:
        switch (focus) {
          case CF_ROOT:   rootNote = (rootNote + 11) % 12; break;
          case CF_TYPE:   typeIdx = (typeIdx + NUM_CHORD_TYPES - 1) % NUM_CHORD_TYPES; break;
          case CF_OCTAVE: octave = max(octave - 1, 2); break;
        }
        break;

      case EVT_ENC_PRESS:
        // Play the chord
        playChord();
        break;

      case EVT_CONFIRM_PRESS:
        // Toggle hold mode
        holdMode = !holdMode;
        if (!holdMode) allOff();
        break;

      case EVT_BACK_PRESS:
        allOff();
        // Cycle focus or exit
        if (focus > CF_ROOT) {
          focus = (ChordFocus)((int)focus - 1);
        } else {
          return false; // Exit to menu
        }
        break;

      default: break;
    }

    // Advance focus with confirm could also be done here
    // For simplicity, back goes up, and turning changes the focused param
    // To move focus down, we use a simple heuristic: after playing, advance focus
    return true;
  }

} // namespace UIChord

#endif
