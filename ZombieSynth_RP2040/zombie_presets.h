#ifndef ZOMBIE_PRESETS_H
#define ZOMBIE_PRESETS_H

// ═══════════════════════════════════════════════════════════════════════════════
// ZOMBIE SS – Preset Manager (RP2040 Port)
// ═══════════════════════════════════════════════════════════════════════════════
//
// Changes from ESP32:
//   - Uses LittleFS instead of ESP32 Preferences/NVS
//   - User presets stored as binary files in /presets/ directory
// ═══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>
#include <LittleFS.h>
#include "config.h"

#define NUM_PRESETS     20
#define NUM_FACTORY     10
#define PRESET_NAME_LEN 14

// Full synth patch data
struct SynthPatch {
  char  name[PRESET_NAME_LEN + 1];
  int   osc1Wave;
  float osc1Level;
  int   osc2Wave;
  float osc2Level;
  float osc2Detune;
  int   filterType;
  float filterCutoff;
  float filterResonance;
  float filterEnvAmount;
  float ampAttack;
  float ampDecay;
  float ampSustain;
  float ampRelease;
  float filterAttack;
  float filterDecay;
  float filterSustain;
  float filterRelease;
  int   lfoWave;
  float lfoRate;
  float lfoDepth;
  int   lfoTarget;
  float masterVolume;
};

// 10 factory presets (Prophet-8 inspired, Zombie themed)
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
  {"ZOMBI PLUCK",   WAVE_SAW, 0.9f, WAVE_SQUARE, 0.3f, 0.004f,
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
  SynthPatch userPresets[NUM_PRESETS - NUM_FACTORY];
  bool fsReady;

  String slotPath(int slot) {
    return "/presets/slot_" + String(slot - NUM_FACTORY) + ".bin";
  }

  void initEmptySlot(int idx) {
    memset(&userPresets[idx], 0, sizeof(SynthPatch));
    strcpy(userPresets[idx].name, "-- EMPTY --");
    userPresets[idx].masterVolume = 0.7f;
    userPresets[idx].filterCutoff = 0.8f;
    userPresets[idx].osc1Wave     = WAVE_SAW;
    userPresets[idx].osc1Level    = 0.7f;
    userPresets[idx].ampSustain   = 0.7f;
  }

public:
  void init() {
    fsReady = LittleFS.begin();
    if (fsReady) {
      // Ensure presets directory exists
      if (!LittleFS.exists("/presets")) {
        LittleFS.mkdir("/presets");
      }
    }

    // Load user presets from flash
    for (int i = 0; i < (NUM_PRESETS - NUM_FACTORY); i++) {
      int slot = i + NUM_FACTORY;
      if (fsReady) {
        String path = slotPath(slot);
        File f = LittleFS.open(path, "r");
        if (f && f.size() == sizeof(SynthPatch)) {
          f.read((uint8_t*)&userPresets[i], sizeof(SynthPatch));
          f.close();
          continue;
        }
        if (f) f.close();
      }
      initEmptySlot(i);
    }
  }

  const char* getName(int slot) {
    if (slot < NUM_FACTORY) return factoryPresets[slot].name;
    if (slot < NUM_PRESETS)  return userPresets[slot - NUM_FACTORY].name;
    return "???";
  }

  const SynthPatch* getPatch(int slot) {
    if (slot < NUM_FACTORY) return &factoryPresets[slot];
    if (slot < NUM_PRESETS)  return &userPresets[slot - NUM_FACTORY];
    return nullptr;
  }

  bool isFactory(int slot) { return slot < NUM_FACTORY; }

  bool saveUserPreset(int slot, const SynthPatch& patch) {
    if (slot < NUM_FACTORY || slot >= NUM_PRESETS) return false;
    userPresets[slot - NUM_FACTORY] = patch;
    if (fsReady) {
      String path = slotPath(slot);
      File f = LittleFS.open(path, "w");
      if (f) {
        f.write((const uint8_t*)&patch, sizeof(SynthPatch));
        f.close();
        return true;
      }
    }
    return false;
  }

  void buildPatchFromParams(SynthPatch& p, const char* name) {
    strncpy(p.name, name, PRESET_NAME_LEN);
    p.name[PRESET_NAME_LEN] = '\0';
  }
};

#endif
