#ifndef OLED_UI_H
#define OLED_UI_H

#include <U8g2lib.h>
#include <Wire.h>
#include "config.h"

// ── OLED Display Manager ──────────────────────────────────────────────────────
// U8g2 full-buffer mode for 1.3" 128x64 I2C OLED.
//
// IMPORTANT: If your screen is blank, try changing these defines:
//   1. OLED_DRIVER: 0 = SH1106 (most 1.3" OLEDs), 1 = SSD1306 (some 1.3" / most 0.96")
//   2. OLED_I2C_ADDR: 0x3C (most common) or 0x3D (some modules)
//
// You can find your display's address by watching the Serial Monitor at
// startup — the I2C scanner will print all detected devices.

#ifndef OLED_DRIVER
  #define OLED_DRIVER 0      // 0 = SH1106, 1 = SSD1306
#endif

#ifndef OLED_I2C_ADDR
  #define OLED_I2C_ADDR 0x3C // Try 0x3D if screen stays blank
#endif

// ── Create display object based on driver selection ───────────────────────────
#if OLED_DRIVER == 1
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
#else
  U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
#endif

// ── Layout Constants ──────────────────────────────────────────────────────────
#define HEADER_H     12   // Title bar height
#define LINE_H       10   // Text line height (with 6x10 font)
#define MAX_VISIBLE   5   // Visible parameter lines
#define FONT_SMALL    u8g2_font_5x7_tr
#define FONT_MEDIUM   u8g2_font_6x10_tr
#define FONT_LARGE    u8g2_font_8x13B_tr

namespace OledUI {

  // ── I2C bus scanner — prints all detected addresses to Serial ────────────
  void scanI2C() {
    Serial.println(F("[OLED] Scanning I2C bus..."));
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        Serial.print(F("[OLED]   Found device at 0x"));
        if (addr < 16) Serial.print('0');
        Serial.println(addr, HEX);
        found++;
      }
    }
    if (found == 0) {
      Serial.println(F("[OLED]   NO I2C devices found! Check wiring."));
      Serial.print(F("[OLED]   SDA=GPIO "));
      Serial.print(OLED_SDA_PIN);
      Serial.print(F("  SCL=GPIO "));
      Serial.println(OLED_SCL_PIN);
    } else {
      Serial.print(F("[OLED]   "));
      Serial.print(found);
      Serial.println(F(" device(s) found."));
    }
  }

  void init() {
    Serial.println(F("[OLED] Initializing display..."));
    Serial.print(F("[OLED]   Driver: "));
    #if OLED_DRIVER == 1
      Serial.println(F("SSD1306"));
    #else
      Serial.println(F("SH1106"));
    #endif
    Serial.print(F("[OLED]   I2C addr: 0x"));
    Serial.println(OLED_I2C_ADDR, HEX);
    Serial.print(F("[OLED]   SDA=GPIO "));
    Serial.print(OLED_SDA_PIN);
    Serial.print(F("  SCL=GPIO "));
    Serial.println(OLED_SCL_PIN);

    // Step 1: Configure Wire pins BEFORE begin (RP2040 requirement)
    Wire.setSDA(OLED_SDA_PIN);
    Wire.setSCL(OLED_SCL_PIN);
    Wire.begin();
    Wire.setClock(400000);  // 400 kHz I2C Fast Mode

    // Step 2: Give OLED time to power up
    delay(150);

    // Step 3: Scan I2C bus for diagnostics
    scanI2C();

    // Step 4: Set I2C address before begin()
    display.setI2CAddress(OLED_I2C_ADDR * 2);  // U8g2 uses 8-bit address (7-bit << 1)

    // Step 5: Initialize display
    display.begin();

    // Step 6: Explicitly turn on display and set max contrast
    display.setPowerSave(0);
    display.setContrast(255);

    // Step 7: Clear and send to verify communication
    display.clearBuffer();
    display.sendBuffer();

    Serial.println(F("[OLED] Init complete."));
  }

  // ── Drawing Primitives ──────────────────────────────────────────────────

  void clear() {
    display.clearBuffer();
  }

  void send() {
    display.sendBuffer();
  }

  // Draw title bar with mode name and optional right-side status
  void drawHeader(const char* title, const char* rightText = nullptr) {
    display.setFont(FONT_MEDIUM);
    display.setDrawColor(1);
    display.drawBox(0, 0, 128, HEADER_H);
    display.setDrawColor(0);
    display.drawStr(2, HEADER_H - 2, title);
    if (rightText) {
      int w = display.getStrWidth(rightText);
      display.drawStr(126 - w, HEADER_H - 2, rightText);
    }
    display.setDrawColor(1);
  }

  // Draw a parameter row: "NAME          VALUE"
  // If selected, draw inverted. If editing, draw value inverted only.
  void drawParamRow(int row, const char* name, const char* value,
                    bool selected, bool editing) {
    int y = HEADER_H + 2 + row * LINE_H;
    display.setFont(FONT_MEDIUM);

    if (selected) {
      if (editing) {
        // Name normal, value inverted
        display.drawStr(2, y + LINE_H - 2, name);
        int vw = display.getStrWidth(value);
        int vx = 126 - vw;
        display.drawBox(vx - 2, y, vw + 4, LINE_H);
        display.setDrawColor(0);
        display.drawStr(vx, y + LINE_H - 2, value);
        display.setDrawColor(1);
      } else {
        // Entire row inverted
        display.drawBox(0, y, 128, LINE_H);
        display.setDrawColor(0);
        display.drawStr(2, y + LINE_H - 2, name);
        int vw = display.getStrWidth(value);
        display.drawStr(126 - vw, y + LINE_H - 2, value);
        display.setDrawColor(1);
      }
    } else {
      // Normal row
      display.drawStr(2, y + LINE_H - 2, name);
      int vw = display.getStrWidth(value);
      display.drawStr(126 - vw, y + LINE_H - 2, value);
    }
  }

  // Draw page tab indicator: dots showing which page of N we're on
  void drawPageDots(int current, int total, int y) {
    int dotW = 4;
    int gap = 3;
    int totalW = total * dotW + (total - 1) * gap;
    int startX = (128 - totalW) / 2;

    for (int i = 0; i < total; i++) {
      int x = startX + i * (dotW + gap);
      if (i == current) {
        display.drawBox(x, y, dotW, 3);
      } else {
        display.drawFrame(x, y, dotW, 3);
      }
    }
  }

  // Draw a horizontal bar (for parameter visualization)
  void drawBar(int x, int y, int w, int h, float value) {
    display.drawFrame(x, y, w, h);
    int fillW = (int)(value * (w - 2));
    if (fillW > 0) display.drawBox(x + 1, y + 1, fillW, h - 2);
  }

  // Draw centered text at y position
  void drawCentered(const char* text, int y, const uint8_t* font = FONT_MEDIUM) {
    display.setFont(font);
    int w = display.getStrWidth(text);
    display.drawStr((128 - w) / 2, y, text);
  }

  // Draw a menu list with scrolling
  void drawMenuList(const char* items[], int count, int selected, int scrollOffset) {
    display.setFont(FONT_MEDIUM);
    for (int i = 0; i < MAX_VISIBLE && (i + scrollOffset) < count; i++) {
      int idx = i + scrollOffset;
      int y = HEADER_H + 2 + i * LINE_H;

      if (idx == selected) {
        display.drawBox(0, y, 128, LINE_H);
        display.setDrawColor(0);
        display.drawStr(4, y + LINE_H - 2, items[idx]);
        display.setDrawColor(1);
      } else {
        display.drawStr(4, y + LINE_H - 2, items[idx]);
      }
    }
  }

  // Draw status bar at bottom
  void drawStatusBar(const char* text) {
    display.setFont(FONT_SMALL);
    int y = 64 - 8;
    display.drawHLine(0, y, 128);
    display.drawStr(2, 63, text);
  }

  // Draw sequencer step grid (16 steps, fits in 128px)
  void drawStepGrid(int y, int h, bool steps[16], int currentStep, bool playing) {
    for (int i = 0; i < 16; i++) {
      int x = i * 8;
      if (steps[i]) {
        display.drawBox(x, y, 7, h);
      } else {
        display.drawFrame(x, y, 7, h);
      }
      if (playing && i == currentStep) {
        display.drawHLine(x, y + h + 1, 7);
      }
    }
  }

  // Splash screen with test pattern
  void drawSplash() {
    Serial.println(F("[OLED] Drawing splash screen..."));

    clear();

    // Draw a border to verify display is working
    display.drawFrame(0, 0, 128, 64);

    // Title text
    drawCentered("ZOMBI SS", 30, FONT_LARGE);
    drawCentered("PROPHET SYNTH v3", 46, FONT_MEDIUM);
    drawCentered("RP2040 Dual-Core", 60, FONT_SMALL);

    send();

    Serial.println(F("[OLED] Splash sent to display."));
  }

  // Note name helper
  const char* noteName(int note) {
    static char buf[6];
    if (note < 0) return "--";
    const char* nn[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    snprintf(buf, sizeof(buf), "%s%d", nn[note % 12], note / 12 - 1);
    return buf;
  }

} // namespace OledUI

#endif
