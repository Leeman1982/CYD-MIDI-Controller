#ifndef ZOMBIE_SEQ_MODE_H
#define ZOMBIE_SEQ_MODE_H

#include "common_definitions.h"
#include "ui_elements.h"
#include "zombie_step_sequencer.h"
#include "zombie_synth_mode.h"
#include "zombie_presets.h"
#include "midi_output.h"

// ============================================================
//  ZOMBIE SS — Commercial-Grade Sequencer UI  v3
//  Sub-views: MAIN · STEP EDIT · TRACK CFG · SCALE PICKER
// ============================================================

static ZombieSequencer* zombieSeq = NULL;

// ── UI state ──────────────────────────────────────────────────────────────────
enum SeqView { SEQ_MAIN, SEQ_STEP_EDIT, SEQ_TRACK_CFG, SEQ_SCALE_PICK };
static SeqView  seqView         = SEQ_MAIN;
static int      seqEditTrack    = 0;
static int      seqEditStep     = 0;
static bool     seqNeedsRedraw  = true;
static int      seqLastStep     = -1;
static bool     seqLastPlaying  = false;

// ── Helper: note names ────────────────────────────────────────────────────────
static const char* seqNoteName(int note) {
  static char buf[6];
  const char* names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
  if (note < 0) { strcpy(buf, "---"); return buf; }
  int oct = note / 12 - 1;
  snprintf(buf, sizeof(buf), "%s%d", names[note % 12], oct);
  return buf;
}

// ── Sequencer callbacks ───────────────────────────────────────────────────────
void seqNoteOnHandler(int trackIdx, int noteNum, int vel) {
  SequencerTrack* t = zombieSeq ? zombieSeq->getTrack(trackIdx) : NULL;

  // Route to INTERNAL synth
  if (!t || t->midiOutput == SEQ_OUT_INTERNAL || t->midiOutput == SEQ_OUT_BOTH) {
    SynthEngine* synth = getZombieSynth();
    if (synth) {
      if (t) {
        int pIdx = constrain(t->soundPatchIdx, 0, NUM_FACTORY - 1);
        const SynthPatch& p = factoryPresets[pIdx];
        synth->setOsc1Waveform((WaveformType)p.osc1Wave);
        synth->setOsc2Waveform((WaveformType)p.osc2Wave);
        synth->setFilterType((FilterType)p.filterType);
        synth->setFilterCutoff(p.filterCutoff);
        synth->setFilterResonance(p.filterResonance);
      }
      synth->noteOn(noteNum, vel);
    }
  }

  // Route to EXTERNAL MIDI OUT
  if (t && (t->midiOutput == SEQ_OUT_EXTERNAL || t->midiOutput == SEQ_OUT_BOTH)) {
    midiOut.noteOn(t->midiChannel - 1, (uint8_t)noteNum, (uint8_t)vel);
    // Send program change (patch) to external device
    midiOut.programChange(t->midiChannel - 1, (uint8_t)t->soundPatchIdx);
  }
}

void seqNoteOffHandler(int trackIdx, int noteNum) {
  SequencerTrack* t = zombieSeq ? zombieSeq->getTrack(trackIdx) : NULL;

  if (!t || t->midiOutput == SEQ_OUT_INTERNAL || t->midiOutput == SEQ_OUT_BOTH) {
    SynthEngine* synth = getZombieSynth();
    if (synth) synth->noteOff(noteNum);
  }
  if (t && (t->midiOutput == SEQ_OUT_EXTERNAL || t->midiOutput == SEQ_OUT_BOTH)) {
    midiOut.noteOff(t->midiChannel - 1, (uint8_t)noteNum);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
//  MAIN VIEW
// ─────────────────────────────────────────────────────────────────────────────
void drawSeqMain(bool fullRedraw) {
  int  currentStep = zombieSeq->getCurrentStep();
  bool isPlaying   = zombieSeq->getIsPlaying();
  bool isRecording = zombieSeq->getIsRecording();

  // ── Header ──────────────────────────────────────────────────────────────────
  if (fullRedraw) {
    tft.fillScreen(THEME_BG);

    // Title bar
    tft.fillRect(0, 0, 320, 30, THEME_SURFACE);
    tft.drawRect(0, 0, 320, 30, THEME_OUTLINE);
    tft.setTextColor(THEME_PRIMARY, THEME_SURFACE);
    tft.drawCentreString("ZOMBIE SEQ", 160, 6, 4);

    // BACK button
    tft.fillRoundRect(2, 2, 42, 26, 3, THEME_BG);
    tft.drawRoundRect(2, 2, 42, 26, 3, THEME_OUTLINE);
    tft.setTextColor(THEME_PRIMARY, THEME_BG);
    tft.drawString("BACK", 6, 8, 2);
  }

  // ── Status bar ──────────────────────────────────────────────────────────────
  tft.fillRect(0, 32, 320, 18, THEME_BG);
  char sb[60];
  int  key   = zombieSeq->getGlobalKey();
  int  scale = zombieSeq->getGlobalScale();
  snprintf(sb, sizeof(sb), "BPM:%.0f  SW:%d  %s/%s  T%d %s",
           zombieSeq->getBPM(), zombieSeq->getSwing(),
           seqRootNames[key], seqScaleNames[scale],
           seqEditTrack+1, isPlaying ? (isRecording ? "REC" : "PLAY") : "STOP");
  tft.setTextColor(isRecording ? THEME_ACCENT : THEME_TEXT_DIM, THEME_BG);
  tft.drawCentreString(sb, 160, 34, 2);

  // ── Track tabs ──────────────────────────────────────────────────────────────
  if (fullRedraw) {
    tft.fillRect(0, 52, 320, 26, THEME_BG);
    for (int t = 0; t < MAX_SEQ_TRACKS; t++) {
      SequencerTrack* tr = zombieSeq->getTrack(t);
      bool sel = (t == seqEditTrack);
      bool mut = tr && tr->muted;
      bool sol = tr && tr->solo;

      uint16_t bg = sel ? THEME_PRIMARY : THEME_BG;
      uint16_t fg = sel ? THEME_BG : (mut ? THEME_TEXT_DIM : THEME_PRIMARY);
      int x = 2 + t * 74;
      tft.fillRoundRect(x, 53, 70, 22, 3, bg);
      tft.drawRoundRect(x, 53, 70, 22, 3, THEME_OUTLINE);
      tft.setTextColor(fg, bg);
      char tbuf[12];
      int pIdx = tr ? constrain(tr->soundPatchIdx, 0, NUM_FACTORY-1) : 0;
      snprintf(tbuf, sizeof(tbuf), "T%d %.3s%s%s",
               t+1, factoryPresets[pIdx].name,
               mut ? "M" : "", sol ? "S" : "");
      tft.drawString(tbuf, x + 3, 58, 2);
    }
    // SCALE picker shortcut button
    tft.fillRoundRect(298, 53, 20, 22, 3, THEME_BG);
    tft.drawRoundRect(298, 53, 20, 22, 3, THEME_OUTLINE);
    tft.setTextColor(THEME_ACCENT, THEME_BG);
    tft.drawCentreString("K", 308, 58, 2);
  }

  // ── 16-step grid ────────────────────────────────────────────────────────────
  tft.fillRect(0, 78, 320, 100, THEME_BG);
  SequencerTrack* track = zombieSeq->getTrack(seqEditTrack);
  if (track) {
    int trackLen = track->trackLength;
    int trackCurStep = zombieSeq->getTrackStep(seqEditTrack);
    for (int s = 0; s < 16; s++) {
      int col = s % 8, row = s / 8;
      int x = 2 + col * 39, y = 80 + row * 47;
      int w = 37, h = 44;
      bool active  = track->steps[s].active;
      bool playing = (s == trackCurStep && isPlaying);
      bool outOfRange = (s >= trackLen);

      uint16_t bg  = outOfRange ? 0x2104 /* dark gray */ :
                     (active && playing) ? THEME_ACCENT :
                     active             ? THEME_PRIMARY :
                     playing            ? THEME_SURFACE : THEME_BG;
      uint16_t brd = outOfRange ? THEME_TEXT_DIM :
                     playing    ? THEME_ACCENT  : THEME_OUTLINE;

      tft.fillRoundRect(x, y, w, h, 3, bg);
      tft.drawRoundRect(x, y, w, h, 3, brd);

      if (active) {
        // Note name
        tft.setTextColor((active && playing) ? THEME_BG : THEME_BG, bg);
        tft.drawCentreString(seqNoteName(track->steps[s].note), x + w/2, y + 4, 2);
        // Velocity bar (bottom 6px)
        int vw = (int)(track->steps[s].velocity / 127.0f * (w - 4));
        tft.fillRect(x + 2, y + h - 8, vw, 5, (active && playing) ? THEME_BG : THEME_ACCENT);
      } else {
        // Step number (dim)
        tft.setTextColor(outOfRange ? 0x2104 : THEME_TEXT_DIM, bg);
        char sn[4]; snprintf(sn, sizeof(sn), "%d", s+1);
        tft.drawCentreString(sn, x + w/2, y + h/2 - 6, 2);
      }

      // Probability indicator (top-right dot if < 100)
      if (active && track->steps[s].probability < 100) {
        tft.fillRect(x + w - 6, y + 2, 4, 4, THEME_ACCENT);
      }
      // Tie indicator
      if (active && track->steps[s].tie) {
        tft.fillRect(x + 2, y + 2, 4, 4, THEME_PRIMARY);
      }
    }
  }

  // ── Bottom controls ──────────────────────────────────────────────────────────
  if (fullRedraw) {
    tft.fillRect(0, 181, 320, 59, THEME_BG);

    // Row 1: PLAY | REC | CLR | RAND | COPY | PASTE
    int bx = 2, bw = 51, bh = 25, by = 182;
    const char* row1[] = {"PLAY","REC","CLR","RAND","COPY","PSTE"};
    for (int i = 0; i < 6; i++) {
      uint16_t bg = THEME_BG;
      if (i == 0 && zombieSeq->getIsPlaying()) bg = THEME_PRIMARY;
      if (i == 1 && zombieSeq->getIsRecording()) bg = THEME_ACCENT;
      tft.fillRoundRect(bx + i*53, by, bw, bh, 3, bg);
      tft.drawRoundRect(bx + i*53, by, bw, bh, 3, THEME_OUTLINE);
      uint16_t fg = (bg == THEME_BG) ? THEME_PRIMARY : THEME_BG;
      tft.setTextColor(fg, bg);
      tft.drawCentreString(row1[i], bx + i*53 + bw/2, by + 7, 2);
    }

    // Row 2: BPM- | BPM+ | SHFL | SHFR | REV | TRKCFG
    int by2 = 211;
    const char* row2[] = {"BPM-","BPM+","<SHF","SHF>","REV","TRKC"};
    for (int i = 0; i < 6; i++) {
      tft.fillRoundRect(bx + i*53, by2, bw, bh, 3, THEME_BG);
      tft.drawRoundRect(bx + i*53, by2, bw, bh, 3, THEME_OUTLINE);
      tft.setTextColor(THEME_PRIMARY, THEME_BG);
      tft.drawCentreString(row2[i], bx + i*53 + bw/2, by2 + 7, 2);
    }
  }
}

void touchSeqMain() {
  if (!touch.justPressed) return;

  // BACK
  if (isButtonPressed(2, 2, 42, 26)) { exitToMenu(); return; }

  // SCALE picker (K button)
  if (isButtonPressed(298, 53, 20, 22)) {
    seqView = SEQ_SCALE_PICK; seqNeedsRedraw = true; return;
  }

  // Track tabs
  for (int t = 0; t < MAX_SEQ_TRACKS; t++) {
    if (isButtonPressed(2 + t*74, 53, 70, 22)) {
      seqEditTrack = t; zombieSeq->setActiveTrack(t);
      seqNeedsRedraw = true; return;
    }
  }

  // Step grid — short tap = toggle, long-press opens step edit
  for (int s = 0; s < 16; s++) {
    int x = 2 + (s%8)*39, y = 80 + (s/8)*47;
    if (isButtonPressed(x, y, 37, 44)) {
      seqEditStep = s;
      SequencerTrack* tr = zombieSeq->getTrack(seqEditTrack);
      if (tr) {
        // Toggle on first tap, go to step edit
        if (!tr->steps[s].active) {
          tr->steps[s].active   = true;
          tr->steps[s].velocity = 100;
          tr->steps[s].gatePercent = 75;
          tr->steps[s].probability = 100;
        }
        seqView = SEQ_STEP_EDIT;
        seqNeedsRedraw = true;
      }
      return;
    }
  }

  // Bottom row 1
  int bx = 2, bw = 51, bh = 25, by = 182;
  if (isButtonPressed(bx,       by, bw, bh)) {
    if (zombieSeq->getIsPlaying()) {
      midiOut.stop();   // MIDI Stop to external synths
      zombieSeq->stop();
    } else {
      midiOut.start();  // MIDI Start to external synths
      zombieSeq->play();
    }
    seqNeedsRedraw = true;
  }
  if (isButtonPressed(bx+53,    by, bw, bh)) {
    zombieSeq->getIsRecording() ? zombieSeq->stopRecording() : zombieSeq->startRecording();
    seqNeedsRedraw = true;
  }
  if (isButtonPressed(bx+106,   by, bw, bh)) { zombieSeq->clearTrack(seqEditTrack);     seqNeedsRedraw = true; }
  if (isButtonPressed(bx+159,   by, bw, bh)) { zombieSeq->randomizeTrack(seqEditTrack); seqNeedsRedraw = true; }
  if (isButtonPressed(bx+212,   by, bw, bh)) { zombieSeq->copyTrack(seqEditTrack);      seqNeedsRedraw = true; }
  if (isButtonPressed(bx+265,   by, bw, bh)) { zombieSeq->pasteTrack(seqEditTrack);     seqNeedsRedraw = true; }

  // Bottom row 2
  int by2 = 211;
  if (isButtonPressed(bx,       by2, bw, bh)) { zombieSeq->setBPM(zombieSeq->getBPM() - 5.0f);  seqNeedsRedraw = true; }
  if (isButtonPressed(bx+53,    by2, bw, bh)) { zombieSeq->setBPM(zombieSeq->getBPM() + 5.0f);  seqNeedsRedraw = true; }
  if (isButtonPressed(bx+106,   by2, bw, bh)) { zombieSeq->shiftLeft(seqEditTrack);              seqNeedsRedraw = true; }
  if (isButtonPressed(bx+159,   by2, bw, bh)) { zombieSeq->shiftRight(seqEditTrack);             seqNeedsRedraw = true; }
  if (isButtonPressed(bx+212,   by2, bw, bh)) { zombieSeq->reverseTrack(seqEditTrack);           seqNeedsRedraw = true; }
  if (isButtonPressed(bx+265,   by2, bw, bh)) { seqView = SEQ_TRACK_CFG; seqNeedsRedraw = true; }
}

// ─────────────────────────────────────────────────────────────────────────────
//  STEP EDIT VIEW
//  Layout (320×240):
//   y=0-28:  header "STEP X / T Y  [NOTE NAME]"  + BACK + PREV + NEXT
//   y=30-90: chromatic keyboard (C..B, 2 rows: naturals / accidentals)
//   y=92-118: octave controls  [-OCT] [C-1] [+OCT]
//   y=120-158: Velocity slider
//   y=160-195: Gate % slider
//   y=197-230: Probability slider
//   y=215-237: ACTIVE toggle  TIE toggle  DONE
// ─────────────────────────────────────────────────────────────────────────────
static void drawHSlider(int x, int y, int w, int h, float val01,
                        uint16_t fg, uint16_t bg, const char* label, char* valStr) {
  tft.fillRect(x, y, w, h, bg);
  tft.drawRect(x, y, w, h, THEME_OUTLINE);
  int filled = (int)(val01 * (w - 2));
  tft.fillRect(x+1, y+1, filled, h-2, fg);
  tft.setTextColor(THEME_PRIMARY, bg);
  tft.drawString(label, x + 3, y + h/2 - 6, 2);
  tft.drawRightString(valStr, x + w - 3, y + h/2 - 6, 2);
}

void drawSeqStepEdit(bool fullRedraw) {
  SequencerTrack* track = zombieSeq ? zombieSeq->getTrack(seqEditTrack) : NULL;
  if (!track) return;
  SequencerStep& step = track->steps[seqEditStep];

  if (fullRedraw) tft.fillScreen(THEME_BG);

  // ── Header ──────────────────────────────────────────────────────────────────
  tft.fillRect(0, 0, 320, 28, THEME_SURFACE);
  tft.drawRect(0, 0, 320, 28, THEME_OUTLINE);

  char hdr[40];
  snprintf(hdr, sizeof(hdr), "STEP %d  T%d  %s", seqEditStep+1, seqEditTrack+1, seqNoteName(step.note));
  tft.setTextColor(THEME_PRIMARY, THEME_SURFACE);
  tft.drawCentreString(hdr, 160, 6, 2);

  // BACK
  tft.fillRoundRect(2, 2, 40, 24, 3, THEME_BG);
  tft.drawRoundRect(2, 2, 40, 24, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawString("BACK", 5, 7, 2);

  // PREV / NEXT
  tft.fillRoundRect(256, 2, 28, 24, 3, THEME_BG);
  tft.drawRoundRect(256, 2, 28, 24, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString("<", 270, 7, 2);

  tft.fillRoundRect(288, 2, 28, 24, 3, THEME_BG);
  tft.drawRoundRect(288, 2, 28, 24, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString(">", 302, 7, 2);

  // ── Chromatic keyboard (naturals: C D E F G A B = 7 keys) ──────────────────
  // White keys row
  const char* wNames[] = {"C","D","E","F","G","A","B"};
  const int   wNotes[] = { 0, 2, 4, 5, 7, 9,11 };  // pitch classes
  int curPC  = step.note % 12;
  int curOct = step.note / 12;
  int kbOct  = curOct;  // keyboard anchors to step's current octave

  int wkW = 40, wkH = 36, wkY = 32;
  int wkStartX = (320 - 7*wkW) / 2;

  for (int i = 0; i < 7; i++) {
    int notePC = wNotes[i];
    bool sel   = (curPC == notePC);
    uint16_t bg = sel ? THEME_ACCENT : THEME_BG;
    tft.fillRect(wkStartX + i*wkW, wkY, wkW-2, wkH, bg);
    tft.drawRect(wkStartX + i*wkW, wkY, wkW-2, wkH, THEME_OUTLINE);
    tft.setTextColor(sel ? THEME_BG : THEME_PRIMARY, bg);
    tft.drawCentreString(wNames[i], wkStartX + i*wkW + wkW/2 - 1, wkY + wkH/2 - 6, 2);
  }

  // Black keys row (C#, D#, -, F#, G#, A#, -)
  const char* bNames[] = {"C#","D#","","F#","G#","A#",""};
  const int   bNotes[] = {  1,  3, -1,  6,  8, 10, -1};
  int bkW = 32, bkH = 28, bkY = 71;

  for (int i = 0; i < 7; i++) {
    if (bNotes[i] < 0) continue;
    int notePC = bNotes[i];
    bool sel   = (curPC == notePC);
    int bkX    = wkStartX + i*wkW + wkW/2 - bkW/2;
    uint16_t bg = sel ? THEME_ACCENT : THEME_SURFACE;
    tft.fillRect(bkX, bkY, bkW-2, bkH, bg);
    tft.drawRect(bkX, bkY, bkW-2, bkH, THEME_OUTLINE);
    tft.setTextColor(sel ? THEME_BG : THEME_PRIMARY, bg);
    tft.drawCentreString(bNames[i], bkX + bkW/2 - 1, bkY + bkH/2 - 6, 2);
  }

  // ── Octave controls ──────────────────────────────────────────────────────────
  int octY = 104;
  tft.fillRect(0, octY, 320, 26, THEME_BG);
  tft.fillRoundRect(4,    octY+1, 55, 24, 3, THEME_BG);
  tft.drawRoundRect(4,    octY+1, 55, 24, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString("-OCT", 31,  octY+8, 2);

  char octBuf[8];
  snprintf(octBuf, sizeof(octBuf), "OCT %d", kbOct - 1);
  tft.fillRoundRect(62,  octY+1, 196, 24, 3, THEME_SURFACE);
  tft.drawRoundRect(62,  octY+1, 196, 24, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_SURFACE);
  tft.drawCentreString(octBuf, 160, octY+8, 2);

  tft.fillRoundRect(262, octY+1, 55, 24, 3, THEME_BG);
  tft.drawRoundRect(262, octY+1, 55, 24, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString("+OCT", 289, octY+8, 2);

  // ── Sliders ──────────────────────────────────────────────────────────────────
  char vbuf[8];

  // Velocity
  snprintf(vbuf, sizeof(vbuf), "%d", step.velocity);
  drawHSlider(4, 134, 312, 22, step.velocity / 127.0f,
              THEME_PRIMARY, THEME_BG, "VEL", vbuf);

  // Gate %
  snprintf(vbuf, sizeof(vbuf), "%d%%", step.gatePercent);
  drawHSlider(4, 160, 312, 22, step.gatePercent / 100.0f,
              THEME_ACCENT, THEME_BG, "GATE", vbuf);

  // Probability
  snprintf(vbuf, sizeof(vbuf), "%d%%", step.probability);
  drawHSlider(4, 186, 312, 22, step.probability / 100.0f,
              0x07E0 /* green */, THEME_BG, "PROB", vbuf);

  // ── Toggle buttons ──────────────────────────────────────────────────────────
  int ty = 213;
  // ACTIVE
  uint16_t actBg = step.active ? THEME_PRIMARY : THEME_BG;
  tft.fillRoundRect(4,   ty, 72, 24, 3, actBg);
  tft.drawRoundRect(4,   ty, 72, 24, 3, THEME_OUTLINE);
  tft.setTextColor(step.active ? THEME_BG : THEME_TEXT_DIM, actBg);
  tft.drawCentreString("ACTIVE", 40, ty+7, 2);

  // TIE
  uint16_t tieBg = step.tie ? THEME_ACCENT : THEME_BG;
  tft.fillRoundRect(82,  ty, 72, 24, 3, tieBg);
  tft.drawRoundRect(82,  ty, 72, 24, 3, THEME_OUTLINE);
  tft.setTextColor(step.tie ? THEME_BG : THEME_TEXT_DIM, tieBg);
  tft.drawCentreString("TIE", 118, ty+7, 2);

  // DELETE step
  tft.fillRoundRect(160, ty, 72, 24, 3, THEME_BG);
  tft.drawRoundRect(160, ty, 72, 24, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString("DEL", 196, ty+7, 2);

  // DONE
  tft.fillRoundRect(244, ty, 72, 24, 3, THEME_PRIMARY);
  tft.drawRoundRect(244, ty, 72, 24, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_BG, THEME_PRIMARY);
  tft.drawCentreString("DONE", 280, ty+7, 2);
}

void touchSeqStepEdit() {
  if (!touch.justPressed) return;
  SequencerTrack* track = zombieSeq->getTrack(seqEditTrack);
  if (!track) return;
  SequencerStep& step = track->steps[seqEditStep];

  // BACK
  if (isButtonPressed(2, 2, 40, 24)) {
    seqView = SEQ_MAIN; seqNeedsRedraw = true; return;
  }
  // PREV step
  if (isButtonPressed(256, 2, 28, 24)) {
    seqEditStep = (seqEditStep - 1 + MAX_SEQ_STEPS) % MAX_SEQ_STEPS;
    seqNeedsRedraw = true; return;
  }
  // NEXT step
  if (isButtonPressed(288, 2, 28, 24)) {
    seqEditStep = (seqEditStep + 1) % MAX_SEQ_STEPS;
    seqNeedsRedraw = true; return;
  }

  // White keys
  const int wNotes[] = { 0, 2, 4, 5, 7, 9, 11 };
  int curOct = step.note / 12;
  int wkStartX = (320 - 7*40) / 2;
  for (int i = 0; i < 7; i++) {
    if (isButtonPressed(wkStartX + i*40, 32, 38, 36)) {
      step.note   = curOct * 12 + wNotes[i];
      step.active = true;
      seqNeedsRedraw = true; return;
    }
  }
  // Black keys
  const int bNotes[] = { 1, 3, -1, 6, 8, 10, -1 };
  for (int i = 0; i < 7; i++) {
    if (bNotes[i] < 0) continue;
    int bkX = wkStartX + i*40 + 20 - 15;
    if (isButtonPressed(bkX, 71, 30, 28)) {
      step.note   = curOct * 12 + bNotes[i];
      step.active = true;
      seqNeedsRedraw = true; return;
    }
  }

  // Octave controls
  if (isButtonPressed(4,   105, 55, 24)) {
    step.note = constrain(step.note - 12, 0, 127);
    seqNeedsRedraw = true; return;
  }
  if (isButtonPressed(262, 105, 55, 24)) {
    step.note = constrain(step.note + 12, 0, 127);
    seqNeedsRedraw = true; return;
  }

  // Velocity slider touch
  if (isButtonPressed(4, 134, 312, 22)) {
    int tv = constrain((int)((touch.x - 4) / 312.0f * 127.0f), 1, 127);
    step.velocity = tv; seqNeedsRedraw = true; return;
  }
  // Gate slider
  if (isButtonPressed(4, 160, 312, 22)) {
    int tg = constrain((int)((touch.x - 4) / 312.0f * 100.0f), 1, 100);
    step.gatePercent = tg; seqNeedsRedraw = true; return;
  }
  // Probability slider
  if (isButtonPressed(4, 186, 312, 22)) {
    int tp = constrain((int)((touch.x - 4) / 312.0f * 100.0f), 1, 100);
    step.probability = tp; seqNeedsRedraw = true; return;
  }

  // Toggle buttons
  int ty = 213;
  if (isButtonPressed(4,   ty, 72, 24)) { step.active = !step.active; seqNeedsRedraw = true; return; }
  if (isButtonPressed(82,  ty, 72, 24)) { step.tie    = !step.tie;    seqNeedsRedraw = true; return; }
  if (isButtonPressed(160, ty, 72, 24)) {
    step.active = false; step.note = 60; step.velocity = 100;
    step.gatePercent = 75; step.probability = 100; step.tie = false;
    seqNeedsRedraw = true; return;
  }
  if (isButtonPressed(244, ty, 72, 24)) {
    seqView = SEQ_MAIN; seqNeedsRedraw = true; return;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
//  TRACK CONFIG VIEW
// ─────────────────────────────────────────────────────────────────────────────
void drawSeqTrackCfg(bool fullRedraw) {
  SequencerTrack* track = zombieSeq ? zombieSeq->getTrack(seqEditTrack) : NULL;
  if (!track) return;

  if (fullRedraw) tft.fillScreen(THEME_BG);

  // ── Header ──────────────────────────────────────────────────────────────────
  tft.fillRect(0, 0, 320, 28, THEME_SURFACE);
  tft.drawRect(0, 0, 320, 28, THEME_OUTLINE);
  char hdr[30];
  snprintf(hdr, sizeof(hdr), "TRACK %d CONFIG", seqEditTrack + 1);
  tft.setTextColor(THEME_PRIMARY, THEME_SURFACE);
  tft.drawCentreString(hdr, 160, 6, 2);
  tft.fillRoundRect(2, 2, 40, 24, 3, THEME_BG);
  tft.drawRoundRect(2, 2, 40, 24, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawString("BACK", 5, 7, 2);

  int y = 34, lh = 34;

  // ── SOUND select ─────────────────────────────────────────────────────────────
  tft.fillRect(0, y, 320, lh, THEME_BG);
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawString("SOUND:", 6, y+9, 2);
  int pIdx = constrain(track->soundPatchIdx, 0, NUM_FACTORY-1);
  tft.fillRoundRect(70, y+4, 180, 24, 3, THEME_SURFACE);
  tft.drawRoundRect(70, y+4, 180, 24, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_SURFACE);
  tft.drawCentreString(factoryPresets[pIdx].name, 160, y+11, 2);
  tft.fillRoundRect(254, y+4, 28, 24, 3, THEME_BG);
  tft.drawRoundRect(254, y+4, 28, 24, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString("<", 268, y+11, 2);
  tft.fillRoundRect(285, y+4, 28, 24, 3, THEME_BG);
  tft.drawRoundRect(285, y+4, 28, 24, 3, THEME_OUTLINE);
  tft.drawCentreString(">", 299, y+11, 2);
  y += lh;

  // ── MIDI Channel ──────────────────────────────────────────────────────────────
  tft.fillRect(0, y, 320, lh, THEME_BG);
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawString("MIDI CH:", 6, y+9, 2);
  tft.fillRoundRect(70, y+4, 28, 24, 3, THEME_BG);
  tft.drawRoundRect(70, y+4, 28, 24, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString("<", 84, y+11, 2);
  char chBuf[5];
  snprintf(chBuf, sizeof(chBuf), "%d", track->midiChannel);
  tft.fillRoundRect(102, y+4, 50, 24, 3, THEME_SURFACE);
  tft.drawRoundRect(102, y+4, 50, 24, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_SURFACE);
  tft.drawCentreString(chBuf, 127, y+11, 2);
  tft.fillRoundRect(156, y+4, 28, 24, 3, THEME_BG);
  tft.drawRoundRect(156, y+4, 28, 24, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString(">", 170, y+11, 2);
  y += lh;

  // ── OUTPUT routing ───────────────────────────────────────────────────────────
  tft.fillRect(0, y, 320, lh, THEME_BG);
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawString("OUTPUT:", 6, y+9, 2);
  for (int o = 0; o < 3; o++) {
    bool sel = (track->midiOutput == o);
    uint16_t bg = sel ? THEME_PRIMARY : THEME_BG;
    tft.fillRoundRect(70 + o*80, y+4, 74, 24, 3, bg);
    tft.drawRoundRect(70 + o*80, y+4, 74, 24, 3, THEME_OUTLINE);
    tft.setTextColor(sel ? THEME_BG : THEME_PRIMARY, bg);
    tft.drawCentreString(seqOutNames[o], 70 + o*80 + 37, y+11, 2);
  }
  y += lh;

  // ── Track LENGTH (polyrhythm) ─────────────────────────────────────────────────
  tft.fillRect(0, y, 320, lh, THEME_BG);
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawString("LENGTH:", 6, y+9, 2);
  tft.fillRoundRect(70, y+4, 28, 24, 3, THEME_BG);
  tft.drawRoundRect(70, y+4, 28, 24, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString("-", 84, y+11, 2);
  char lenBuf[5];
  snprintf(lenBuf, sizeof(lenBuf), "%d", track->trackLength);
  tft.fillRoundRect(102, y+4, 50, 24, 3, THEME_SURFACE);
  tft.drawRoundRect(102, y+4, 50, 24, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_SURFACE);
  tft.drawCentreString(lenBuf, 127, y+11, 2);
  tft.fillRoundRect(156, y+4, 28, 24, 3, THEME_BG);
  tft.drawRoundRect(156, y+4, 28, 24, 3, THEME_OUTLINE);
  tft.drawCentreString("+", 170, y+11, 2);
  y += lh;

  // ── OCTAVE offset ────────────────────────────────────────────────────────────
  tft.fillRect(0, y, 320, lh, THEME_BG);
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawString("OCTAVE:", 6, y+9, 2);
  tft.fillRoundRect(70, y+4, 28, 24, 3, THEME_BG);
  tft.drawRoundRect(70, y+4, 28, 24, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawCentreString("-", 84, y+11, 2);
  char octBuf2[6];
  snprintf(octBuf2, sizeof(octBuf2), "%+d", (int)track->octave);
  tft.fillRoundRect(102, y+4, 50, 24, 3, THEME_SURFACE);
  tft.drawRoundRect(102, y+4, 50, 24, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_SURFACE);
  tft.drawCentreString(octBuf2, 127, y+11, 2);
  tft.fillRoundRect(156, y+4, 28, 24, 3, THEME_BG);
  tft.drawRoundRect(156, y+4, 28, 24, 3, THEME_OUTLINE);
  tft.drawCentreString("+", 170, y+11, 2);
  y += lh;

  // ── MUTE / SOLO ──────────────────────────────────────────────────────────────
  tft.fillRect(0, y, 320, lh, THEME_BG);
  uint16_t muteBg = track->muted ? THEME_ACCENT : THEME_BG;
  tft.fillRoundRect(6,   y+4, 148, 24, 3, muteBg);
  tft.drawRoundRect(6,   y+4, 148, 24, 3, THEME_OUTLINE);
  tft.setTextColor(track->muted ? THEME_BG : THEME_PRIMARY, muteBg);
  tft.drawCentreString(track->muted ? "UNMUTE" : "MUTE", 80, y+11, 2);

  uint16_t soloBg = track->solo ? THEME_PRIMARY : THEME_BG;
  tft.fillRoundRect(166, y+4, 148, 24, 3, soloBg);
  tft.drawRoundRect(166, y+4, 148, 24, 3, THEME_OUTLINE);
  tft.setTextColor(track->solo ? THEME_BG : THEME_PRIMARY, soloBg);
  tft.drawCentreString(track->solo ? "UNSOLO" : "SOLO", 240, y+11, 2);
}

void touchSeqTrackCfg() {
  if (!touch.justPressed) return;
  SequencerTrack* track = zombieSeq->getTrack(seqEditTrack);
  if (!track) return;

  // BACK
  if (isButtonPressed(2, 2, 40, 24)) {
    seqView = SEQ_MAIN; seqNeedsRedraw = true; return;
  }

  int y = 34, lh = 34;

  // SOUND < / >
  if (isButtonPressed(254, y+4, 28, 24)) {
    track->soundPatchIdx = (track->soundPatchIdx + NUM_FACTORY - 1) % NUM_FACTORY;
    seqNeedsRedraw = true; return;
  }
  if (isButtonPressed(285, y+4, 28, 24)) {
    track->soundPatchIdx = (track->soundPatchIdx + 1) % NUM_FACTORY;
    seqNeedsRedraw = true; return;
  }
  y += lh;

  // MIDI channel < / >
  if (isButtonPressed(70, y+4, 28, 24)) {
    track->midiChannel = track->midiChannel > 1 ? track->midiChannel - 1 : 16;
    seqNeedsRedraw = true; return;
  }
  if (isButtonPressed(156, y+4, 28, 24)) {
    track->midiChannel = track->midiChannel < 16 ? track->midiChannel + 1 : 1;
    seqNeedsRedraw = true; return;
  }
  y += lh;

  // OUTPUT routing
  for (int o = 0; o < 3; o++) {
    if (isButtonPressed(70 + o*80, y+4, 74, 24)) {
      track->midiOutput = o; seqNeedsRedraw = true; return;
    }
  }
  y += lh;

  // LENGTH - / +
  if (isButtonPressed(70, y+4, 28, 24)) {
    track->trackLength = constrain(track->trackLength - 1, 1, MAX_SEQ_STEPS);
    seqNeedsRedraw = true; return;
  }
  if (isButtonPressed(156, y+4, 28, 24)) {
    track->trackLength = constrain(track->trackLength + 1, 1, MAX_SEQ_STEPS);
    seqNeedsRedraw = true; return;
  }
  y += lh;

  // OCTAVE - / +
  if (isButtonPressed(70, y+4, 28, 24)) {
    track->octave = constrain((int)track->octave - 1, -3, 3);
    seqNeedsRedraw = true; return;
  }
  if (isButtonPressed(156, y+4, 28, 24)) {
    track->octave = constrain((int)track->octave + 1, -3, 3);
    seqNeedsRedraw = true; return;
  }
  y += lh;

  // MUTE / SOLO
  if (isButtonPressed(6,   y+4, 148, 24)) { track->muted = !track->muted; seqNeedsRedraw = true; return; }
  if (isButtonPressed(166, y+4, 148, 24)) { track->solo  = !track->solo;  seqNeedsRedraw = true; return; }
}

// ─────────────────────────────────────────────────────────────────────────────
//  SCALE PICKER VIEW
// ─────────────────────────────────────────────────────────────────────────────
void drawSeqScalePick(bool fullRedraw) {
  if (fullRedraw) tft.fillScreen(THEME_BG);

  // ── Header ──────────────────────────────────────────────────────────────────
  tft.fillRect(0, 0, 320, 28, THEME_SURFACE);
  tft.drawRect(0, 0, 320, 28, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_SURFACE);
  tft.drawCentreString("SCALE & KEY", 160, 6, 2);
  tft.fillRoundRect(2, 2, 40, 24, 3, THEME_BG);
  tft.drawRoundRect(2, 2, 40, 24, 3, THEME_OUTLINE);
  tft.setTextColor(THEME_PRIMARY, THEME_BG);
  tft.drawString("BACK", 5, 7, 2);

  // ── Key row (12 chromatic roots) ─────────────────────────────────────────────
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawString("KEY:", 6, 34, 2);

  int curKey   = zombieSeq->getGlobalKey();
  int curScale = zombieSeq->getGlobalScale();
  bool scaleLock = zombieSeq->getScaleLock();

  // 12 root buttons: 2 rows (natural / sharp)
  // Row 0: C D E F G A B  (7)
  // Row 1: C# D# _ F# G# A# _ (5 actual sharps)
  // Simpler: just 12 buttons in 2 rows of 6
  for (int k = 0; k < 12; k++) {
    int col = k % 6, row = k / 6;
    int kx = 4 + col * 52, ky = 48 + row * 34;
    bool sel = (k == curKey);
    uint16_t bg = sel ? THEME_ACCENT : THEME_BG;
    tft.fillRoundRect(kx, ky, 48, 30, 3, bg);
    tft.drawRoundRect(kx, ky, 48, 30, 3, THEME_OUTLINE);
    tft.setTextColor(sel ? THEME_BG : THEME_PRIMARY, bg);
    tft.drawCentreString(seqRootNames[k], kx + 24, ky + 8, 2);
  }

  // ── Scale buttons ────────────────────────────────────────────────────────────
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawString("SCALE:", 6, 122, 2);

  // 9 scales in 3 rows of 3
  for (int s = 0; s < NUM_SCALES_SEQ; s++) {
    int col = s % 3, row = s / 3;
    int sx = 4 + col * 104, sy = 136 + row * 28;
    bool sel = (s == curScale);
    uint16_t bg = sel ? THEME_PRIMARY : THEME_BG;
    tft.fillRoundRect(sx, sy, 100, 24, 3, bg);
    tft.drawRoundRect(sx, sy, 100, 24, 3, THEME_OUTLINE);
    tft.setTextColor(sel ? THEME_BG : THEME_PRIMARY, bg);
    tft.drawCentreString(seqScaleNames[s], sx + 50, sy + 6, 2);
  }

  // ── Scale lock toggle ─────────────────────────────────────────────────────────
  uint16_t lockBg = scaleLock ? THEME_ACCENT : THEME_BG;
  tft.fillRoundRect(4,   222, 148, 14, 3, lockBg);   // thin row at bottom
  tft.drawRoundRect(4,   222, 148, 14, 3, THEME_OUTLINE);
  tft.setTextColor(scaleLock ? THEME_BG : THEME_TEXT_DIM, lockBg);
  tft.drawCentreString(scaleLock ? "SCALE LOCK: ON" : "SCALE LOCK: OFF", 78, 224, 2);

  // ── Scale note preview (highlight scale degrees in key) ─────────────────────
  // Show which of the 12 chromatic pitch classes belong to the selected scale
  tft.fillRect(156, 222, 160, 14, THEME_BG);
  tft.setTextColor(THEME_TEXT_DIM, THEME_BG);
  tft.drawString("NOTES:", 158, 224, 2);
  int nx = 210;
  for (int n = 0; n < 12; n++) {
    if (seqNoteInScale(n, curKey, curScale)) {
      // Highlight dot
      uint16_t dc = (n == curKey) ? THEME_ACCENT : THEME_PRIMARY;
      tft.fillCircle(nx, 229, 3, dc);
      nx += 8;
    }
  }
}

void touchSeqScalePick() {
  if (!touch.justPressed) return;

  // BACK
  if (isButtonPressed(2, 2, 40, 24)) {
    seqView = SEQ_MAIN; seqNeedsRedraw = true; return;
  }

  // Key buttons
  for (int k = 0; k < 12; k++) {
    int col = k % 6, row = k / 6;
    int kx = 4 + col * 52, ky = 48 + row * 34;
    if (isButtonPressed(kx, ky, 48, 30)) {
      zombieSeq->setGlobalKey(k); seqNeedsRedraw = true; return;
    }
  }

  // Scale buttons
  for (int s = 0; s < NUM_SCALES_SEQ; s++) {
    int col = s % 3, row = s / 3;
    int sx = 4 + col * 104, sy = 136 + row * 28;
    if (isButtonPressed(sx, sy, 100, 24)) {
      zombieSeq->setGlobalScale(s); seqNeedsRedraw = true; return;
    }
  }

  // Scale lock
  if (isButtonPressed(4, 222, 148, 14)) {
    zombieSeq->setScaleLock(!zombieSeq->getScaleLock());
    seqNeedsRedraw = true; return;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
//  TOP-LEVEL DISPATCHER
// ─────────────────────────────────────────────────────────────────────────────
void zombieSeqDraw() {
  int  cs = zombieSeq->getCurrentStep();
  bool ip = zombieSeq->getIsPlaying();

  bool stepChange = (cs != seqLastStep) || (ip != seqLastPlaying);
  bool doFull     = seqNeedsRedraw;

  if (!doFull && !stepChange && seqView == SEQ_MAIN) return;
  if (!doFull && seqView != SEQ_MAIN) return;

  switch (seqView) {
    case SEQ_MAIN:      drawSeqMain(doFull);      break;
    case SEQ_STEP_EDIT: drawSeqStepEdit(doFull);  break;
    case SEQ_TRACK_CFG: drawSeqTrackCfg(doFull);  break;
    case SEQ_SCALE_PICK:drawSeqScalePick(doFull); break;
  }

  seqNeedsRedraw = false;
  seqLastStep    = cs;
  seqLastPlaying = ip;
}

void zombieSeqHandleTouch() {
  switch (seqView) {
    case SEQ_MAIN:      touchSeqMain();       break;
    case SEQ_STEP_EDIT: touchSeqStepEdit();   break;
    case SEQ_TRACK_CFG: touchSeqTrackCfg();   break;
    case SEQ_SCALE_PICK:touchSeqScalePick();  break;
  }
}

void zombieSeqUpdate() {
  if (zombieSeq) {
    zombieSeq->update(millis());
    // Send MIDI clock when playing
    static unsigned long lastClockTick = 0;
    if (zombieSeq->getIsPlaying()) {
      // 24 PPQN: ms per clock = 60000/(bpm*24)
      unsigned long clkMs = (unsigned long)(60000.0f / (zombieSeq->getBPM() * 24.0f));
      if (millis() - lastClockTick >= clkMs) {
        lastClockTick = millis();
        midiOut.clockTick();
      }
    }
  }
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
  zombieSeq->setGlobalKey(0);
  zombieSeq->setGlobalScale(1);  // Major
  zombieSeq->setScaleLock(false);

  // Assign distinct default sounds to each track
  for (int t = 0; t < MAX_SEQ_TRACKS; t++) {
    SequencerTrack* tr = zombieSeq->getTrack(t);
    if (tr) {
      tr->soundPatchIdx = t;   // T1=PROPHET PAD, T2=BASS STAB, etc.
      tr->midiChannel   = (uint8_t)(t + 1);
      tr->midiOutput    = SEQ_OUT_INTERNAL;
      tr->trackLength   = 16;
    }
  }

  seqEditTrack   = 0;
  seqEditStep    = 0;
  seqView        = SEQ_MAIN;
  seqNeedsRedraw = true;
  seqLastStep    = -1;
  seqLastPlaying = false;

  tft.fillScreen(THEME_BG);
}

ZombieSequencer* getZombieSeq() { return zombieSeq; }

#endif
