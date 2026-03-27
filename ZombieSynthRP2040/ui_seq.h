#ifndef UI_SEQ_H
#define UI_SEQ_H

#include "config.h"
#include "oled_ui.h"
#include "zombie_step_sequencer.h"
#include "synth_engine.h"

// ── Sequencer UI ──────────────────────────────────────────────────────────────
// Views: GRID (step toggle), EDIT (step params), TRACK (track config)
// Encoder: select step/param, confirm: play/stop, back: exit/back view

extern SynthEngine    synthEngine;
extern ZombieSequencer sequencer;

namespace UISeq {

  enum SeqView   { VIEW_GRID, VIEW_EDIT, VIEW_TRACK };
  enum EditParam { EP_NOTE, EP_VEL, EP_GATE, EP_PROB, EP_TIE, EP_COUNT };

  static SeqView  view = VIEW_GRID;
  static int      cursorStep = 0;
  static int      editParam = 0;
  static bool     editing = false;
  static int      trackConfigParam = 0;

  void enter() {
    view = VIEW_GRID;
    cursorStep = 0;
    editParam = 0;
    editing = false;
    trackConfigParam = 0;
  }

  void drawGrid() {
    OledUI::clear();

    // Header
    char hdr[20];
    int at = sequencer.getActiveTrack();
    snprintf(hdr, sizeof(hdr), "T%d %.0fBPM", at + 1, sequencer.getBPM());
    OledUI::drawHeader("SEQ", hdr);

    // 4 track grids (each 8px tall)
    int trackY = HEADER_H + 2;
    for (int t = 0; t < MAX_SEQ_TRACKS; t++) {
      SequencerTrack* tr = sequencer.getTrack(t);
      int y = trackY + t * 12;

      // Track label
      display.setFont(FONT_SMALL);
      char lbl[4];
      snprintf(lbl, sizeof(lbl), "%d", t + 1);
      if (t == at) {
        display.drawBox(0, y, 8, 10);
        display.setDrawColor(0);
        display.drawStr(1, y + 8, lbl);
        display.setDrawColor(1);
      } else {
        display.drawStr(1, y + 8, lbl);
      }

      // Step grid
      bool playing = sequencer.getIsPlaying();
      int curStep = sequencer.getTrackStep(t);
      for (int s = 0; s < tr->trackLength && s < 16; s++) {
        int x = 10 + s * 7;
        if (tr->steps[s].active) {
          display.drawBox(x, y, 6, 8);
        } else {
          display.drawFrame(x, y, 6, 8);
        }
        // Muted track: diagonal line through active steps
        if (tr->muted && tr->steps[s].active) {
          display.drawLine(x, y, x + 5, y + 7);
        }
        // Playhead
        if (playing && s == curStep) {
          display.drawHLine(x, y + 9, 6);
        }
        // Cursor (grid view on active track)
        if (t == at && s == cursorStep) {
          display.drawFrame(x - 1, y - 1, 8, 10);
        }
      }
    }

    // Status bar
    char status[32];
    SequencerTrack* atr = sequencer.getTrack(at);
    snprintf(status, sizeof(status), "%s Sw:%d %s",
             sequencer.getIsPlaying() ? "PLAY" : "STOP",
             sequencer.getSwing(),
             atr->muted ? "MUTE" : "");
    OledUI::drawStatusBar(status);

    OledUI::send();
  }

  void drawEdit() {
    OledUI::clear();

    int at = sequencer.getActiveTrack();
    SequencerTrack* tr = sequencer.getTrack(at);
    SequencerStep& step = tr->steps[cursorStep];

    char hdr[20];
    snprintf(hdr, sizeof(hdr), "T%d S%d", at + 1, cursorStep + 1);
    OledUI::drawHeader("EDIT", hdr);

    const char* epNames[] = {"Note","Velocity","Gate %","Prob %","Tie"};
    char valBuf[16];

    for (int i = 0; i < EP_COUNT; i++) {
      switch (i) {
        case EP_NOTE: snprintf(valBuf, sizeof(valBuf), "%s", OledUI::noteName(step.note)); break;
        case EP_VEL:  snprintf(valBuf, sizeof(valBuf), "%d", step.velocity); break;
        case EP_GATE: snprintf(valBuf, sizeof(valBuf), "%d%%", step.gatePercent); break;
        case EP_PROB: snprintf(valBuf, sizeof(valBuf), "%d%%", step.probability); break;
        case EP_TIE:  snprintf(valBuf, sizeof(valBuf), "%s", step.tie ? "ON" : "OFF"); break;
      }
      OledUI::drawParamRow(i, epNames[i], valBuf,
                           i == editParam, editing && i == editParam);
    }

    char status[20];
    snprintf(status, sizeof(status), "%s", step.active ? "ACTIVE" : "OFF");
    OledUI::drawStatusBar(status);

    OledUI::send();
  }

  void drawTrackConfig() {
    OledUI::clear();

    int at = sequencer.getActiveTrack();
    SequencerTrack* tr = sequencer.getTrack(at);

    char hdr[16];
    snprintf(hdr, sizeof(hdr), "Track %d", at + 1);
    OledUI::drawHeader("TRACK", hdr);

    const char* tcNames[] = {"MIDI Ch","Output","Length","Octave","Mute","Solo"};
    char valBuf[16];

    int tcCount = 6;
    for (int i = 0; i < min(tcCount, MAX_VISIBLE); i++) {
      switch (i) {
        case 0: snprintf(valBuf, sizeof(valBuf), "%d", tr->midiChannel); break;
        case 1: snprintf(valBuf, sizeof(valBuf), "%s", seqOutNames[tr->midiOutput]); break;
        case 2: snprintf(valBuf, sizeof(valBuf), "%d", tr->trackLength); break;
        case 3: snprintf(valBuf, sizeof(valBuf), "%+d", tr->octave); break;
        case 4: snprintf(valBuf, sizeof(valBuf), "%s", tr->muted ? "ON" : "OFF"); break;
        case 5: snprintf(valBuf, sizeof(valBuf), "%s", tr->solo ? "ON" : "OFF"); break;
      }
      OledUI::drawParamRow(i, tcNames[i], valBuf,
                           i == trackConfigParam, editing && i == trackConfigParam);
    }

    OledUI::send();
  }

  void draw() {
    switch (view) {
      case VIEW_GRID:  drawGrid(); break;
      case VIEW_EDIT:  drawEdit(); break;
      case VIEW_TRACK: drawTrackConfig(); break;
    }
  }

  bool handleGridInput(InputEvent evt) {
    int at = sequencer.getActiveTrack();
    SequencerTrack* tr = sequencer.getTrack(at);

    switch (evt) {
      case EVT_ENC_CW:
        cursorStep = min(cursorStep + 1, (int)tr->trackLength - 1);
        break;
      case EVT_ENC_CCW:
        cursorStep = max(cursorStep - 1, 0);
        break;
      case EVT_ENC_PRESS:
        sequencer.toggleStep(at, cursorStep);
        break;
      case EVT_CONFIRM_PRESS:
        if (sequencer.getIsPlaying()) sequencer.stop();
        else sequencer.play();
        break;
      case EVT_BACK_PRESS:
        return false; // Exit to menu
      default: break;
    }
    return true;
  }

  bool handleEditInput(InputEvent evt) {
    int at = sequencer.getActiveTrack();
    SequencerTrack* tr = sequencer.getTrack(at);
    SequencerStep& step = tr->steps[cursorStep];

    switch (evt) {
      case EVT_ENC_CW:
        if (editing) {
          switch (editParam) {
            case EP_NOTE: step.note = min((int)step.note + 1, 127); break;
            case EP_VEL:  step.velocity = min((int)step.velocity + 1, 127); break;
            case EP_GATE: step.gatePercent = min((int)step.gatePercent + 5, 100); break;
            case EP_PROB: step.probability = min((int)step.probability + 5, 100); break;
            case EP_TIE:  step.tie = !step.tie; break;
          }
        } else {
          editParam = min(editParam + 1, (int)EP_COUNT - 1);
        }
        break;
      case EVT_ENC_CCW:
        if (editing) {
          switch (editParam) {
            case EP_NOTE: step.note = max((int)step.note - 1, 0); break;
            case EP_VEL:  step.velocity = max((int)step.velocity - 1, 1); break;
            case EP_GATE: step.gatePercent = max((int)step.gatePercent - 5, 1); break;
            case EP_PROB: step.probability = max((int)step.probability - 5, 1); break;
            case EP_TIE:  step.tie = !step.tie; break;
          }
        } else {
          editParam = max(editParam - 1, 0);
        }
        break;
      case EVT_ENC_PRESS:
        editing = !editing;
        break;
      case EVT_BACK_PRESS:
        if (editing) editing = false;
        else { view = VIEW_GRID; editParam = 0; }
        break;
      default: break;
    }
    return true;
  }

  bool handleTrackInput(InputEvent evt) {
    int at = sequencer.getActiveTrack();
    SequencerTrack* tr = sequencer.getTrack(at);

    switch (evt) {
      case EVT_ENC_CW:
        if (editing) {
          switch (trackConfigParam) {
            case 0: tr->midiChannel = min((int)tr->midiChannel + 1, 16); break;
            case 1: tr->midiOutput = (tr->midiOutput + 1) % 3; break;
            case 2: tr->trackLength = min((int)tr->trackLength + 1, 16); break;
            case 3: tr->octave = min((int)tr->octave + 1, 3); break;
            case 4: tr->muted = !tr->muted; break;
            case 5: tr->solo = !tr->solo; break;
          }
        } else {
          trackConfigParam = min(trackConfigParam + 1, 5);
        }
        break;
      case EVT_ENC_CCW:
        if (editing) {
          switch (trackConfigParam) {
            case 0: tr->midiChannel = max((int)tr->midiChannel - 1, 1); break;
            case 1: tr->midiOutput = (tr->midiOutput + 2) % 3; break;
            case 2: tr->trackLength = max((int)tr->trackLength - 1, 1); break;
            case 3: tr->octave = max((int)tr->octave - 1, -3); break;
            case 4: tr->muted = !tr->muted; break;
            case 5: tr->solo = !tr->solo; break;
          }
        } else {
          trackConfigParam = max(trackConfigParam - 1, 0);
        }
        break;
      case EVT_ENC_PRESS:
        editing = !editing;
        break;
      case EVT_BACK_PRESS:
        if (editing) editing = false;
        else { view = VIEW_GRID; trackConfigParam = 0; }
        break;
      default: break;
    }
    return true;
  }

  bool handleInput(InputEvent evt) {
    // Long-press confirm in grid view: switch active track
    // Double-press enc in grid: enter step edit
    // These are approximated with confirm for play, enc for toggle/edit

    // Global: confirm + back combos for track switching
    // Use confirm button in GRID to play/stop
    // Use enc long-press or double-click heuristics are complex;
    // instead: confirm cycles tracks when in grid (holding confirm + enc = track)

    switch (view) {
      case VIEW_GRID:
        // Special: if confirm pressed while on a step, enter edit for that step
        if (evt == EVT_CONFIRM_PRESS) {
          // If already playing, pressing confirm toggles play
          // If stopped, first press enters step edit
          if (!sequencer.getIsPlaying()) {
            view = VIEW_EDIT;
            editParam = 0;
            editing = false;
            return true;
          } else {
            sequencer.stop();
            return true;
          }
        }
        if (evt == EVT_ENC_PRESS) {
          // Toggle step active state
          int at = sequencer.getActiveTrack();
          sequencer.toggleStep(at, cursorStep);
          return true;
        }
        return handleGridInput(evt);

      case VIEW_EDIT:
        if (evt == EVT_CONFIRM_PRESS) {
          // In edit: confirm enters track config
          view = VIEW_TRACK;
          trackConfigParam = 0;
          editing = false;
          return true;
        }
        return handleEditInput(evt);

      case VIEW_TRACK:
        if (evt == EVT_CONFIRM_PRESS) {
          // Cycle active track
          int next = (sequencer.getActiveTrack() + 1) % MAX_SEQ_TRACKS;
          sequencer.setActiveTrack(next);
          view = VIEW_GRID;
          return true;
        }
        return handleTrackInput(evt);
    }
    return true;
  }

} // namespace UISeq

#endif
