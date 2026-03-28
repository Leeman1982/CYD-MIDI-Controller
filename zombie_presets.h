#ifndef ZOMBIE_PRESETS_H
#define ZOMBIE_PRESETS_H

#include <Preferences.h>
#include "synth_engine.h"

#define NUM_PRESETS     20
#define NUM_FACTORY     10
#define PRESET_NAME_LEN 14

// Full synth patch data
struct SynthPatch {
  char name[PRESET_NAME_LEN + 1];
  // Oscillators
  int   osc1Wave;
  float osc1Level;
  int   osc2Wave;
  float osc2Level;
  float osc2Detune;
  // Filter
  int   filterType;
  float filterCutoff;
  float filterResonance;
  float filterEnvAmount;
  // Amp Envelope
  float ampAttack;
  float ampDecay;
  float ampSustain;
  float ampRelease;
  // Filter Envelope
  float filterAttack;
  float filterDecay;
  float filterSustain;
  float filterRelease;
  // LFO
  int   lfoWave;
  float lfoRate;
  float lfoDepth;
  int   lfoTarget;   // 0=filter, 1=pitch, 2=amp
  // Master
  float masterVolume;
};

// ---- 10 factory presets ----
const SynthPatch factoryPresets[NUM_FACTORY] = {
  // 0: PROPHET PAD
  {"PROPHET PAD",   WAVE_SAW, 0.6f, WAVE_SAW, 0.5f, 0.006f,
   FILTER_LOWPASS, 0.55f, 0.35f, 0.6f,
   0.4f, 0.5f, 0.75f, 0.8f,
   0.3f, 0.4f, 0.5f, 0.4f,
   0, 0.5f, 0.15f, 0, 0.7f},

  // 1: BASS STAB
  {"BASS STAB",     WAVE_SAW, 0.8f, WAVE_SQUARE, 0.4f, 0.003f,
   FILTER_LOWPASS, 0.7f, 0.5f, 0.8f,
   0.005f, 0.2f, 0.0f, 0.1f,
   0.005f, 0.15f, 0.0f, 0.1f,
   0, 0.0f, 0.0f, 0, 0.8f},

  // 2: LEAD SAW
  {"LEAD SAW",      WAVE_SAW, 0.7f, WAVE_SAW, 0.6f, 0.008f,
   FILTER_LOWPASS, 0.85f, 0.25f, 0.3f,
   0.008f, 0.1f, 0.85f, 0.2f,
   0.008f, 0.1f, 0.6f, 0.15f,
   0, 6.0f, 0.05f, 1, 0.75f},

  // 3: ZOMBI PLUCK
  {"ZOMBI PLUCK",  WAVE_SAW, 0.9f, WAVE_SQUARE, 0.3f, 0.004f,
   FILTER_LOWPASS, 0.9f, 0.45f, 0.95f,
   0.003f, 0.35f, 0.0f, 0.15f,
   0.003f, 0.2f, 0.0f, 0.1f,
   0, 0.0f, 0.0f, 0, 0.75f},

  // 4: DEAD STRINGS
  {"DEAD STRINGS",  WAVE_SAW, 0.55f, WAVE_SAW, 0.55f, 0.010f,
   FILTER_LOWPASS, 0.6f, 0.2f, 0.4f,
   0.35f, 0.4f, 0.8f, 0.9f,
   0.2f, 0.35f, 0.55f, 0.6f,
   0, 4.5f, 0.12f, 0, 0.65f},

  // 5: DOOM BRASS
  {"DOOM BRASS",    WAVE_SAW, 0.8f, WAVE_PULSE, 0.6f, 0.005f,
   FILTER_LOWPASS, 0.72f, 0.4f, 0.75f,
   0.07f, 0.2f, 0.7f, 0.3f,
   0.05f, 0.2f, 0.5f, 0.2f,
   0, 5.5f, 0.04f, 0, 0.8f},

  // 6: DEATH BELL
  {"DEATH BELL",    WAVE_SINE, 0.7f, WAVE_TRIANGLE, 0.5f, 0.012f,
   FILTER_BANDPASS, 0.7f, 0.5f, 0.2f,
   0.003f, 0.8f, 0.1f, 2.0f,
   0.003f, 0.5f, 0.1f, 1.5f,
   0, 0.0f, 0.0f, 0, 0.6f},

  // 7: BLOOD ORGAN
  {"BLOOD ORGAN",   WAVE_SQUARE, 0.6f, WAVE_TRIANGLE, 0.5f, 0.002f,
   FILTER_LOWPASS, 0.75f, 0.15f, 0.2f,
   0.005f, 0.3f, 0.8f, 0.4f,
   0.005f, 0.2f, 0.4f, 0.2f,
   0, 6.0f, 0.08f, 2, 0.7f},

  // 8: GHOST LEAD
  {"GHOST LEAD",    WAVE_PULSE, 0.65f, WAVE_SAW, 0.45f, 0.007f,
   FILTER_HIGHPASS, 0.4f, 0.35f, 0.3f,
   0.01f, 0.15f, 0.9f, 0.25f,
   0.01f, 0.1f, 0.5f, 0.15f,
   0, 5.0f, 0.07f, 1, 0.7f},

  // 9: APOCALYPSE
  {"APOCALYPSE",    WAVE_SAW, 0.75f, WAVE_SAW, 0.75f, 0.015f,
   FILTER_LOWPASS, 0.45f, 0.6f, 0.7f,
   0.5f, 0.6f, 0.6f, 1.5f,
   0.4f, 0.5f, 0.4f, 1.0f,
   0, 0.8f, 0.2f, 0, 0.8f},
};

class PresetManager {
private:
  Preferences prefs;
  SynthPatch userPresets[NUM_PRESETS - NUM_FACTORY];

  String slotKey(int slot) {
    return "slot_" + String(slot - NUM_FACTORY);
  }

public:
  void init() {
    prefs.begin("zombie_ss", false);
    // Load user presets from NVS
    for (int i = NUM_FACTORY; i < NUM_PRESETS; i++) {
      String key = slotKey(i);
      if (prefs.isKey(key.c_str())) {
        prefs.getBytes(key.c_str(), &userPresets[i - NUM_FACTORY], sizeof(SynthPatch));
      } else {
        // Empty slot defaults
        memset(&userPresets[i - NUM_FACTORY], 0, sizeof(SynthPatch));
        strcpy(userPresets[i - NUM_FACTORY].name, "-- EMPTY --");
        userPresets[i - NUM_FACTORY].masterVolume = 0.7f;
        userPresets[i - NUM_FACTORY].filterCutoff = 0.8f;
        userPresets[i - NUM_FACTORY].osc1Wave = WAVE_SAW;
        userPresets[i - NUM_FACTORY].osc1Level = 0.7f;
        userPresets[i - NUM_FACTORY].ampSustain = 0.7f;
      }
    }
  }

  const char* getName(int slot) {
    if (slot < NUM_FACTORY) return factoryPresets[slot].name;
    return userPresets[slot - NUM_FACTORY].name;
  }

  const SynthPatch* getPatch(int slot) {
    if (slot < NUM_FACTORY) return &factoryPresets[slot];
    return &userPresets[slot - NUM_FACTORY];
  }

  bool isFactory(int slot) { return slot < NUM_FACTORY; }

  bool saveUserPreset(int slot, const SynthPatch& patch) {
    if (slot < NUM_FACTORY || slot >= NUM_PRESETS) return false;
    userPresets[slot - NUM_FACTORY] = patch;
    String key = slotKey(slot);
    prefs.putBytes(key.c_str(), &patch, sizeof(SynthPatch));
    return true;
  }

  void buildPatchFromParams(SynthPatch& p, const char* name) {
    strncpy(p.name, name, PRESET_NAME_LEN);
    p.name[PRESET_NAME_LEN] = '\0';
    // Caller fills remaining fields
  }
};

#endif
