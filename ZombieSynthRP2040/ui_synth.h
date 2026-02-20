#ifndef UI_SYNTH_H
#define UI_SYNTH_H

#include "config.h"
#include "oled_ui.h"
#include "synth_engine.h"
#include "zombie_lfo.h"

// ── Synth Parameter Editor UI ─────────────────────────────────────────────────
// 6 pages: OSC, FILTER, AMP ENV, FILT ENV, LFO, FX
// Navigation: encoder scroll params, press to edit, back to exit

extern SynthEngine  synthEngine;
extern SynthParams  synthParams;
extern LFOEngine    globalLFO;
extern int          lastPlayedMidiNote;

#define SYNTH_NUM_PAGES 6
static const char* synthPageNames[] = {"OSC","FLTR","AMP","FENV","LFO","FX"};

// Per-page parameter definitions
struct ParamDef {
  const char* name;
  float       minVal, maxVal, step;
  bool        isInt;
};

// ── OSC page params ──────────────────────────────────────────────────────────
static const ParamDef oscParams[] = {
  {"OSC1 Wave",   0, 6, 1, true},
  {"OSC1 Level",  0, 1.0f, 0.05f, false},
  {"OSC2 Wave",   0, 6, 1, true},
  {"OSC2 Level",  0, 1.0f, 0.05f, false},
  {"Detune",      0, 1.0f, 0.02f, false},
  {"Semitones",  -12, 12, 1, true},
};
#define NUM_OSC_PARAMS 6

// ── Filter page params ───────────────────────────────────────────────────────
static const ParamDef filterParams[] = {
  {"Cutoff",      0, 1.0f, 0.02f, false},
  {"Resonance",   0, 1.0f, 0.02f, false},
  {"Env Amount",  0, 1.0f, 0.05f, false},
  {"Type",        0, 3, 1, true},
};
#define NUM_FILTER_PARAMS 4

// ── Amp Envelope params ──────────────────────────────────────────────────────
static const ParamDef ampEnvParams[] = {
  {"Attack",      0.001f, 2.0f, 0.02f, false},
  {"Decay",       0.001f, 2.0f, 0.02f, false},
  {"Sustain",     0, 1.0f, 0.02f, false},
  {"Release",     0.001f, 2.0f, 0.02f, false},
};
#define NUM_AMPENV_PARAMS 4

// ── Filter Envelope params ───────────────────────────────────────────────────
static const ParamDef filtEnvParams[] = {
  {"Attack",      0.001f, 2.0f, 0.02f, false},
  {"Decay",       0.001f, 2.0f, 0.02f, false},
  {"Sustain",     0, 1.0f, 0.02f, false},
  {"Release",     0.001f, 2.0f, 0.02f, false},
};
#define NUM_FILTENV_PARAMS 4

// ── LFO params ───────────────────────────────────────────────────────────────
static const ParamDef lfoParams[] = {
  {"Enabled",     0, 1, 1, true},
  {"Wave",        0, 4, 1, true},
  {"Rate",        0.1f, 20.0f, 0.2f, false},
  {"Depth",       0, 1.0f, 0.02f, false},
  {"Target",      0, 5, 1, true},
};
#define NUM_LFO_PARAMS 5

// ── FX params ────────────────────────────────────────────────────────────────
static const ParamDef fxParams[] = {
  {"Chorus On",   0, 1, 1, true},
  {"Cho Rate",    0.1f, 8.0f, 0.1f, false},
  {"Cho Depth",   0.001f, 0.01f, 0.001f, false},
  {"Cho Mix",     0, 1.0f, 0.05f, false},
  {"Delay On",    0, 1, 1, true},
  {"Dly Time",    50, 250, 10, true},
  {"Dly Fdbk",    0, 0.9f, 0.05f, false},
  {"Dly Mix",     0, 1.0f, 0.05f, false},
  {"Reverb On",   0, 1, 1, true},
  {"Rev Room",    0, 1.0f, 0.05f, false},
  {"Rev Damp",    0, 1.0f, 0.05f, false},
  {"Rev Mix",     0, 1.0f, 0.05f, false},
};
#define NUM_FX_PARAMS 12

namespace UISynth {

  static int  currentPage = 0;
  static int  selectedParam = 0;
  static int  scrollOffset = 0;
  static bool editing = false;

  int getPageParamCount() {
    switch (currentPage) {
      case 0: return NUM_OSC_PARAMS;
      case 1: return NUM_FILTER_PARAMS;
      case 2: return NUM_AMPENV_PARAMS;
      case 3: return NUM_FILTENV_PARAMS;
      case 4: return NUM_LFO_PARAMS;
      case 5: return NUM_FX_PARAMS;
      default: return 0;
    }
  }

  const ParamDef* getPageParams() {
    switch (currentPage) {
      case 0: return oscParams;
      case 1: return filterParams;
      case 2: return ampEnvParams;
      case 3: return filtEnvParams;
      case 4: return lfoParams;
      case 5: return fxParams;
      default: return nullptr;
    }
  }

  // Get/set current param value by page and index
  float getParamValue(int page, int idx) {
    switch (page) {
      case 0: // OSC
        switch (idx) {
          case 0: return (float)synthParams.osc1Wave;
          case 1: return synthParams.osc1Level;
          case 2: return (float)synthParams.osc2Wave;
          case 3: return synthParams.osc2Level;
          case 4: return synthParams.osc2Detune;
          case 5: return synthParams.osc2Semitones;
        } break;
      case 1: // Filter
        switch (idx) {
          case 0: return synthParams.filterCutoff;
          case 1: return synthParams.filterResonance;
          case 2: return synthParams.filterEnvAmount;
          case 3: return (float)synthParams.filterType;
        } break;
      case 2: // Amp Env
        switch (idx) {
          case 0: return synthParams.ampAttack;
          case 1: return synthParams.ampDecay;
          case 2: return synthParams.ampSustain;
          case 3: return synthParams.ampRelease;
        } break;
      case 3: // Filter Env
        switch (idx) {
          case 0: return synthParams.filterAttack;
          case 1: return synthParams.filterDecay;
          case 2: return synthParams.filterSustain;
          case 3: return synthParams.filterRelease;
        } break;
      case 4: // LFO
        switch (idx) {
          case 0: return globalLFO.enabled ? 1.0f : 0.0f;
          case 1: return (float)globalLFO.wave;
          case 2: return globalLFO.rate;
          case 3: return globalLFO.depth;
          case 4: return (float)globalLFO.target;
        } break;
      case 5: // FX
        switch (idx) {
          case 0:  return synthEngine.fx.chorus.enabled ? 1.0f : 0.0f;
          case 1:  return synthEngine.fx.chorus.rate;
          case 2:  return synthEngine.fx.chorus.depth;
          case 3:  return synthEngine.fx.chorus.mix;
          case 4:  return synthEngine.fx.delay.enabled ? 1.0f : 0.0f;
          case 5:  return synthEngine.fx.delay.delayMs;
          case 6:  return synthEngine.fx.delay.feedback;
          case 7:  return synthEngine.fx.delay.mix;
          case 8:  return synthEngine.fx.reverb.enabled ? 1.0f : 0.0f;
          case 9:  return synthEngine.fx.reverb.roomSize;
          case 10: return synthEngine.fx.reverb.damping;
          case 11: return synthEngine.fx.reverb.mix;
        } break;
    }
    return 0.0f;
  }

  void setParamValue(int page, int idx, float val) {
    switch (page) {
      case 0: // OSC
        switch (idx) {
          case 0: synthParams.osc1Wave = (int)val; synthEngine.setOsc1Waveform((WaveformType)(int)val); break;
          case 1: synthParams.osc1Level = val; break;
          case 2: synthParams.osc2Wave = (int)val; synthEngine.setOsc2Waveform((WaveformType)(int)val); break;
          case 3: synthParams.osc2Level = val; break;
          case 4: synthParams.osc2Detune = val; synthEngine.setOsc2Detune(val * 0.02f); break;
          case 5: synthParams.osc2Semitones = val; synthEngine.setOsc2Semitones(val); break;
        } break;
      case 1: // Filter
        switch (idx) {
          case 0: synthParams.filterCutoff = val; synthEngine.setFilterCutoff(val); break;
          case 1: synthParams.filterResonance = val; synthEngine.setFilterResonance(val); break;
          case 2: synthParams.filterEnvAmount = val; synthEngine.setFilterEnvAmount(val); break;
          case 3: synthParams.filterType = (int)val; synthEngine.setFilterType((FilterType)(int)val); break;
        } break;
      case 2: // Amp Env
        switch (idx) {
          case 0: synthParams.ampAttack  = val; break;
          case 1: synthParams.ampDecay   = val; break;
          case 2: synthParams.ampSustain = val; break;
          case 3: synthParams.ampRelease = val; break;
        }
        synthEngine.setAmpEnvelope(synthParams.ampAttack, synthParams.ampDecay,
                                    synthParams.ampSustain, synthParams.ampRelease);
        break;
      case 3: // Filter Env
        switch (idx) {
          case 0: synthParams.filterAttack  = val; break;
          case 1: synthParams.filterDecay   = val; break;
          case 2: synthParams.filterSustain = val; break;
          case 3: synthParams.filterRelease = val; break;
        }
        synthEngine.setFilterEnvelope(synthParams.filterAttack, synthParams.filterDecay,
                                       synthParams.filterSustain, synthParams.filterRelease);
        break;
      case 4: // LFO
        switch (idx) {
          case 0: globalLFO.enabled = (val > 0.5f); break;
          case 1: globalLFO.wave    = (LFOWave)(int)val; break;
          case 2: globalLFO.rate    = val; break;
          case 3: globalLFO.depth   = val; globalLFO.enabled = (val > 0.001f); break;
          case 4: globalLFO.target  = (LFOTarget)(int)val; break;
        } break;
      case 5: // FX
        switch (idx) {
          case 0:  synthEngine.fx.chorus.enabled = (val > 0.5f); break;
          case 1:  synthEngine.fx.chorus.rate    = val; break;
          case 2:  synthEngine.fx.chorus.depth   = val; break;
          case 3:  synthEngine.fx.chorus.mix     = val; break;
          case 4:  synthEngine.fx.delay.enabled  = (val > 0.5f); break;
          case 5:  synthEngine.fx.delay.delayMs  = val; break;
          case 6:  synthEngine.fx.delay.feedback = val; break;
          case 7:  synthEngine.fx.delay.mix      = val; break;
          case 8:  synthEngine.fx.reverb.enabled = (val > 0.5f); break;
          case 9:  synthEngine.fx.reverb.roomSize= val; synthEngine.fx.reverb.updateParams(); break;
          case 10: synthEngine.fx.reverb.damping = val; synthEngine.fx.reverb.updateParams(); break;
          case 11: synthEngine.fx.reverb.mix     = val; break;
        } break;
    }
  }

  // Format parameter value for display
  void formatValue(int page, int idx, float val, char* buf, int bufLen) {
    switch (page) {
      case 0: // OSC
        if (idx == 0 || idx == 2) {
          snprintf(buf, bufLen, "%s", waveformNames[(int)val % NUM_WAVEFORMS]);
        } else if (idx == 4) {
          snprintf(buf, bufLen, "%.3f", val * 0.02f);
        } else if (idx == 5) {
          snprintf(buf, bufLen, "%+.0f", val);
        } else {
          snprintf(buf, bufLen, "%d%%", (int)(val * 100));
        } break;
      case 1: // Filter
        if (idx == 3) {
          snprintf(buf, bufLen, "%s", filterTypeNames[(int)val % NUM_FILTER_TYPES]);
        } else {
          snprintf(buf, bufLen, "%d%%", (int)(val * 100));
        } break;
      case 2: case 3: // Envelopes
        if (idx == 2) snprintf(buf, bufLen, "%d%%", (int)(val * 100)); // Sustain is a level
        else snprintf(buf, bufLen, "%.2fs", val);
        break;
      case 4: // LFO
        if (idx == 0) snprintf(buf, bufLen, "%s", val > 0.5f ? "ON" : "OFF");
        else if (idx == 1) snprintf(buf, bufLen, "%s", lfoWaveNames[(int)val % NUM_LFO_WAVES]);
        else if (idx == 2) snprintf(buf, bufLen, "%.1fHz", val);
        else if (idx == 3) snprintf(buf, bufLen, "%d%%", (int)(val * 100));
        else if (idx == 4) snprintf(buf, bufLen, "%s", lfoTargetNames[(int)val % NUM_LFO_TARGETS]);
        break;
      case 5: // FX
        if (idx == 0 || idx == 4 || idx == 8) {
          snprintf(buf, bufLen, "%s", val > 0.5f ? "ON" : "OFF");
        } else if (idx == 5) {
          snprintf(buf, bufLen, "%dms", (int)val);
        } else if (idx == 2) {
          snprintf(buf, bufLen, "%.1fms", val * 1000);
        } else {
          snprintf(buf, bufLen, "%d%%", (int)(val * 100));
        } break;
      default:
        snprintf(buf, bufLen, "%.2f", val);
    }
  }

  void enter() {
    currentPage = 0; selectedParam = 0; scrollOffset = 0; editing = false;
  }

  void draw() {
    OledUI::clear();

    // Header with page name and voice count
    char headerR[16];
    snprintf(headerR, sizeof(headerR), "%s %dV",
             OledUI::noteName(lastPlayedMidiNote),
             synthEngine.getActiveVoiceCount());
    OledUI::drawHeader(synthPageNames[currentPage], headerR);

    // Page dots
    OledUI::drawPageDots(currentPage, SYNTH_NUM_PAGES, HEADER_H + 1);

    // Parameter list
    int count = getPageParamCount();
    const ParamDef* params = getPageParams();

    // Ensure selected is visible
    if (selectedParam < scrollOffset) scrollOffset = selectedParam;
    if (selectedParam >= scrollOffset + MAX_VISIBLE) scrollOffset = selectedParam - MAX_VISIBLE + 1;

    for (int i = 0; i < MAX_VISIBLE && (i + scrollOffset) < count; i++) {
      int idx = i + scrollOffset;
      float val = getParamValue(currentPage, idx);
      char valBuf[16];
      formatValue(currentPage, idx, val, valBuf, sizeof(valBuf));

      OledUI::drawParamRow(i, params[idx].name, valBuf,
                           idx == selectedParam, editing && idx == selectedParam);
    }

    // Scroll indicator
    if (count > MAX_VISIBLE) {
      display.setFont(FONT_SMALL);
      if (scrollOffset > 0) display.drawStr(122, HEADER_H + 10, "^");
      if (scrollOffset + MAX_VISIBLE < count) display.drawStr(122, 58, "v");
    }

    OledUI::send();
  }

  // Returns true if still in synth mode, false if should exit to menu
  bool handleInput(InputEvent evt) {
    int count = getPageParamCount();
    const ParamDef* params = getPageParams();

    switch (evt) {
      case EVT_ENC_CW:
        if (editing) {
          float val = getParamValue(currentPage, selectedParam);
          const ParamDef& p = params[selectedParam];
          val += p.step;
          if (val > p.maxVal) val = p.maxVal;
          if (p.isInt) val = roundf(val);
          setParamValue(currentPage, selectedParam, val);
        } else {
          selectedParam++;
          if (selectedParam >= count) selectedParam = count - 1;
        }
        break;

      case EVT_ENC_CCW:
        if (editing) {
          float val = getParamValue(currentPage, selectedParam);
          const ParamDef& p = params[selectedParam];
          val -= p.step;
          if (val < p.minVal) val = p.minVal;
          if (p.isInt) val = roundf(val);
          setParamValue(currentPage, selectedParam, val);
        } else {
          selectedParam--;
          if (selectedParam < 0) selectedParam = 0;
        }
        break;

      case EVT_ENC_PRESS:
        editing = !editing;
        break;

      case EVT_CONFIRM_PRESS:
        // Next page
        currentPage = (currentPage + 1) % SYNTH_NUM_PAGES;
        selectedParam = 0; scrollOffset = 0; editing = false;
        break;

      case EVT_BACK_PRESS:
        if (editing) {
          editing = false;
        } else if (currentPage > 0) {
          currentPage--;
          selectedParam = 0; scrollOffset = 0;
        } else {
          return false; // Exit to menu
        }
        break;

      default: break;
    }
    return true;
  }

} // namespace UISynth

#endif
