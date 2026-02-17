#ifndef ZOMBIE_PRESETS_H
#define ZOMBIE_PRESETS_H

#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"

// ── Preset Storage — RP2040 EEPROM (flash-backed) ────────────────────────────
// 10 factory + 10 user presets. User presets stored in EEPROM.

#define NUM_PRESETS     20
#define NUM_FACTORY     10
#define PRESET_NAME_LEN 14

#define EEPROM_MAGIC    0xZB55
#define EEPROM_SIZE     2048   // Reserve 2KB

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
  float ampAttack, ampDecay, ampSustain, ampRelease;
  float filterAttack, filterDecay, filterSustain, filterRelease;
  int   lfoWave;
  float lfoRate;
  float lfoDepth;
  int   lfoTarget;
  float masterVolume;
};

// ── 10 Factory Presets ────────────────────────────────────────────────────────
static const SynthPatch factoryPresets[NUM_FACTORY] = {
  {"PROPHET PAD",  0, 0.6f, 0, 0.5f, 0.006f,  0, 0.55f, 0.35f, 0.6f,
   0.4f, 0.5f, 0.75f, 0.8f,  0.3f, 0.4f, 0.5f, 0.4f,  0, 0.5f, 0.15f, 0, 0.7f},
  {"BASS STAB",    0, 0.8f, 1, 0.4f, 0.003f,  0, 0.7f, 0.5f, 0.8f,
   0.005f, 0.2f, 0.0f, 0.1f,  0.005f, 0.15f, 0.0f, 0.1f,  0, 0.0f, 0.0f, 0, 0.8f},
  {"LEAD SAW",     0, 0.7f, 0, 0.6f, 0.008f,  0, 0.85f, 0.25f, 0.3f,
   0.008f, 0.1f, 0.85f, 0.2f,  0.008f, 0.1f, 0.6f, 0.15f,  0, 6.0f, 0.05f, 1, 0.75f},
  {"ZOMBI PLUCK",  0, 0.9f, 1, 0.3f, 0.004f,  0, 0.9f, 0.45f, 0.95f,
   0.003f, 0.35f, 0.0f, 0.15f,  0.003f, 0.2f, 0.0f, 0.1f,  0, 0.0f, 0.0f, 0, 0.75f},
  {"DEAD STRINGS", 0, 0.55f, 0, 0.55f, 0.010f,  0, 0.6f, 0.2f, 0.4f,
   0.35f, 0.4f, 0.8f, 0.9f,  0.2f, 0.35f, 0.55f, 0.6f,  0, 4.5f, 0.12f, 0, 0.65f},
  {"DOOM BRASS",   0, 0.8f, 4, 0.6f, 0.005f,  0, 0.72f, 0.4f, 0.75f,
   0.07f, 0.2f, 0.7f, 0.3f,  0.05f, 0.2f, 0.5f, 0.2f,  0, 5.5f, 0.04f, 0, 0.8f},
  {"DEATH BELL",   3, 0.7f, 2, 0.5f, 0.012f,  2, 0.7f, 0.5f, 0.2f,
   0.003f, 0.8f, 0.1f, 2.0f,  0.003f, 0.5f, 0.1f, 1.5f,  0, 0.0f, 0.0f, 0, 0.6f},
  {"BLOOD ORGAN",  1, 0.6f, 2, 0.5f, 0.002f,  0, 0.75f, 0.15f, 0.2f,
   0.005f, 0.3f, 0.8f, 0.4f,  0.005f, 0.2f, 0.4f, 0.2f,  0, 6.0f, 0.08f, 2, 0.7f},
  {"GHOST LEAD",   4, 0.65f, 0, 0.45f, 0.007f,  1, 0.4f, 0.35f, 0.3f,
   0.01f, 0.15f, 0.9f, 0.25f,  0.01f, 0.1f, 0.5f, 0.15f,  0, 5.0f, 0.07f, 1, 0.7f},
  {"APOCALYPSE",   0, 0.75f, 0, 0.75f, 0.015f,  0, 0.45f, 0.6f, 0.7f,
   0.5f, 0.6f, 0.6f, 1.5f,  0.4f, 0.5f, 0.4f, 1.0f,  0, 0.8f, 0.2f, 0, 0.8f},
};

class PresetManager {
private:
  SynthPatch userPresets[NUM_PRESETS - NUM_FACTORY];
  bool inited = false;

  // Each user preset stored at offset: 4 + (slot * sizeof(SynthPatch))
  // Byte 0-1: magic number (0x5A42 = "ZB")
  int slotOffset(int slot) {
    return 4 + (slot - NUM_FACTORY) * sizeof(SynthPatch);
  }

public:
  void init() {
    if (inited) return;
    EEPROM.begin(EEPROM_SIZE);

    // Check magic
    uint8_t m0 = EEPROM.read(0);
    uint8_t m1 = EEPROM.read(1);
    bool valid = (m0 == 0x5A && m1 == 0x42);

    for (int i = NUM_FACTORY; i < NUM_PRESETS; i++) {
      if (valid) {
        int offset = slotOffset(i);
        uint8_t* ptr = (uint8_t*)&userPresets[i - NUM_FACTORY];
        for (size_t b = 0; b < sizeof(SynthPatch); b++)
          ptr[b] = EEPROM.read(offset + b);
      } else {
        memset(&userPresets[i - NUM_FACTORY], 0, sizeof(SynthPatch));
        strcpy(userPresets[i - NUM_FACTORY].name, "-- EMPTY --");
        userPresets[i - NUM_FACTORY].masterVolume = 0.7f;
        userPresets[i - NUM_FACTORY].filterCutoff = 0.8f;
        userPresets[i - NUM_FACTORY].osc1Wave = 0;
        userPresets[i - NUM_FACTORY].osc1Level = 0.7f;
        userPresets[i - NUM_FACTORY].ampSustain = 0.7f;
      }
    }

    if (!valid) {
      EEPROM.write(0, 0x5A);
      EEPROM.write(1, 0x42);
      EEPROM.commit();
    }

    inited = true;
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
    int offset = slotOffset(slot);
    const uint8_t* ptr = (const uint8_t*)&patch;
    for (size_t b = 0; b < sizeof(SynthPatch); b++)
      EEPROM.write(offset + b, ptr[b]);
    EEPROM.commit();
    return true;
  }
};

#endif
