#ifndef UI_PRESETS_H
#define UI_PRESETS_H

#include "config.h"
#include "oled_ui.h"
#include "zombie_presets.h"
#include "synth_engine.h"
#include "zombie_lfo.h"

// ── Presets UI ────────────────────────────────────────────────────────────────
// Scroll through 20 presets, load/save, rename (limited on OLED)

extern SynthEngine   synthEngine;
extern SynthParams   synthParams;
extern LFOEngine     globalLFO;
extern PresetManager presetMgr;

namespace UIPresets {

  static int  selected = 0;
  static int  scrollOffset = 0;
  static bool confirmSave = false;

  // Character editing for rename (simplified: no QWERTY on 128x64)
  static bool renaming = false;
  static int  renameCursor = 0;
  static char renameBuf[PRESET_NAME_LEN + 1];

  void enter() {
    selected = 0;
    scrollOffset = 0;
    confirmSave = false;
    renaming = false;
  }

  // Apply a patch to the live synth
  void applyPatch(const SynthPatch& p) {
    synthParams.osc1Wave       = p.osc1Wave;
    synthParams.osc1Level      = p.osc1Level;
    synthParams.osc2Wave       = p.osc2Wave;
    synthParams.osc2Level      = p.osc2Level;
    synthParams.osc2Detune     = p.osc2Detune;
    synthParams.filterType     = p.filterType;
    synthParams.filterCutoff   = p.filterCutoff;
    synthParams.filterResonance= p.filterResonance;
    synthParams.filterEnvAmount= p.filterEnvAmount;
    synthParams.ampAttack      = p.ampAttack;
    synthParams.ampDecay       = p.ampDecay;
    synthParams.ampSustain     = p.ampSustain;
    synthParams.ampRelease     = p.ampRelease;
    synthParams.filterAttack   = p.filterAttack;
    synthParams.filterDecay    = p.filterDecay;
    synthParams.filterSustain  = p.filterSustain;
    synthParams.filterRelease  = p.filterRelease;
    synthParams.masterVolume   = p.masterVolume;

    globalLFO.wave    = (LFOWave)p.lfoWave;
    globalLFO.rate    = p.lfoRate;
    globalLFO.depth   = p.lfoDepth;
    globalLFO.target  = (LFOTarget)p.lfoTarget;
    globalLFO.enabled = (p.lfoDepth > 0.001f);

    synthEngine.setOsc1Waveform((WaveformType)p.osc1Wave);
    synthEngine.setOsc2Waveform((WaveformType)p.osc2Wave);
    synthEngine.setFilterType((FilterType)p.filterType);
    synthEngine.setFilterCutoff(p.filterCutoff);
    synthEngine.setFilterResonance(p.filterResonance);
    synthEngine.setAmpEnvelope(p.ampAttack, p.ampDecay, p.ampSustain, p.ampRelease);
    synthEngine.setFilterEnvelope(p.filterAttack, p.filterDecay, p.filterSustain, p.filterRelease);
    synthEngine.setMasterVolume(p.masterVolume);
  }

  // Collect current state into a patch
  void collectPatch(SynthPatch& p, const char* name) {
    strncpy(p.name, name, PRESET_NAME_LEN);
    p.name[PRESET_NAME_LEN] = '\0';
    p.osc1Wave       = synthParams.osc1Wave;
    p.osc1Level      = synthParams.osc1Level;
    p.osc2Wave       = synthParams.osc2Wave;
    p.osc2Level      = synthParams.osc2Level;
    p.osc2Detune     = synthParams.osc2Detune;
    p.filterType     = synthParams.filterType;
    p.filterCutoff   = synthParams.filterCutoff;
    p.filterResonance= synthParams.filterResonance;
    p.filterEnvAmount= synthParams.filterEnvAmount;
    p.ampAttack      = synthParams.ampAttack;
    p.ampDecay       = synthParams.ampDecay;
    p.ampSustain     = synthParams.ampSustain;
    p.ampRelease     = synthParams.ampRelease;
    p.filterAttack   = synthParams.filterAttack;
    p.filterDecay    = synthParams.filterDecay;
    p.filterSustain  = synthParams.filterSustain;
    p.filterRelease  = synthParams.filterRelease;
    p.lfoWave        = (int)globalLFO.wave;
    p.lfoRate        = globalLFO.rate;
    p.lfoDepth       = globalLFO.depth;
    p.lfoTarget      = (int)globalLFO.target;
    p.masterVolume   = synthParams.masterVolume;
  }

  void drawRename() {
    OledUI::clear();
    OledUI::drawHeader("RENAME", "");

    display.setFont(FONT_MEDIUM);

    // Show current name with cursor
    int y = 20;
    for (int i = 0; i < PRESET_NAME_LEN; i++) {
      char ch = renameBuf[i];
      if (ch == '\0') ch = '_';
      char s[2] = {ch, 0};
      int x = 4 + i * 8;

      if (i == renameCursor) {
        display.drawBox(x, y, 8, LINE_H + 2);
        display.setDrawColor(0);
        display.drawStr(x + 1, y + LINE_H, s);
        display.setDrawColor(1);
      } else {
        display.drawStr(x + 1, y + LINE_H, s);
      }
    }

    display.setFont(FONT_SMALL);
    display.drawStr(2, 42, "Turn:char Enc:next");
    display.drawStr(2, 52, "Confirm:done Back:cancel");

    OledUI::send();
  }

  void draw() {
    if (renaming) { drawRename(); return; }

    OledUI::clear();

    char hdr[16];
    snprintf(hdr, sizeof(hdr), "%d/%d", selected + 1, NUM_PRESETS);
    OledUI::drawHeader("PRESETS", hdr);

    // Preset list
    if (selected < scrollOffset) scrollOffset = selected;
    if (selected >= scrollOffset + MAX_VISIBLE) scrollOffset = selected - MAX_VISIBLE + 1;

    display.setFont(FONT_MEDIUM);
    for (int i = 0; i < MAX_VISIBLE && (i + scrollOffset) < NUM_PRESETS; i++) {
      int idx = i + scrollOffset;
      int y = HEADER_H + 2 + i * LINE_H;

      char line[22];
      snprintf(line, sizeof(line), "%c%2d %s",
               idx < NUM_FACTORY ? 'F' : 'U',
               idx + 1,
               presetMgr.getName(idx));

      if (idx == selected) {
        display.drawBox(0, y, 128, LINE_H);
        display.setDrawColor(0);
        display.drawStr(2, y + LINE_H - 2, line);
        display.setDrawColor(1);
      } else {
        display.drawStr(2, y + LINE_H - 2, line);
      }
    }

    // Action hints
    if (confirmSave) {
      OledUI::drawStatusBar("Confirm SAVE? Enc=Y Bk=N");
    } else {
      const char* hint = (selected >= NUM_FACTORY)
        ? "Enc:LOAD Cfm:SAVE Bk:exit"
        : "Enc:LOAD Back:exit";
      OledUI::drawStatusBar(hint);
    }

    OledUI::send();
  }

  bool handleInput(InputEvent evt) {
    // Rename mode
    if (renaming) {
      switch (evt) {
        case EVT_ENC_CW: {
          char& ch = renameBuf[renameCursor];
          if (ch == '\0' || ch == ' ') ch = 'A';
          else if (ch == 'Z') ch = '0';
          else if (ch == '9') ch = ' ';
          else ch++;
          break;
        }
        case EVT_ENC_CCW: {
          char& ch = renameBuf[renameCursor];
          if (ch == '\0' || ch == ' ') ch = '9';
          else if (ch == '0') ch = 'Z';
          else if (ch == 'A') ch = ' ';
          else ch--;
          break;
        }
        case EVT_ENC_PRESS:
          renameCursor++;
          if (renameCursor >= PRESET_NAME_LEN) renameCursor = 0;
          break;
        case EVT_CONFIRM_PRESS: {
          // Save with new name
          SynthPatch p;
          collectPatch(p, renameBuf);
          presetMgr.saveUserPreset(selected, p);
          renaming = false;
          break;
        }
        case EVT_BACK_PRESS:
          renaming = false;
          break;
        default: break;
      }
      return true;
    }

    // Save confirmation
    if (confirmSave) {
      if (evt == EVT_ENC_PRESS) {
        // Proceed to rename then save
        strncpy(renameBuf, presetMgr.getName(selected), PRESET_NAME_LEN);
        renameBuf[PRESET_NAME_LEN] = '\0';
        renameCursor = 0;
        renaming = true;
        confirmSave = false;
      } else if (evt == EVT_BACK_PRESS) {
        confirmSave = false;
      }
      return true;
    }

    // Normal preset list
    switch (evt) {
      case EVT_ENC_CW:
        if (selected < NUM_PRESETS - 1) selected++;
        break;
      case EVT_ENC_CCW:
        if (selected > 0) selected--;
        break;
      case EVT_ENC_PRESS:
        // Load preset
        applyPatch(*presetMgr.getPatch(selected));
        break;
      case EVT_CONFIRM_PRESS:
        if (selected >= NUM_FACTORY) {
          confirmSave = true;
        }
        break;
      case EVT_BACK_PRESS:
        return false; // Exit to menu
      default: break;
    }
    return true;
  }

} // namespace UIPresets

#endif
