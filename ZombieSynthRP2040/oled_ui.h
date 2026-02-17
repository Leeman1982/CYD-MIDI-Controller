#ifndef OLED_UI_H
#define OLED_UI_H

#include <U8g2lib.h>
#include <Wire.h>
#include "config.h"

// ── OLED Display Manager ──────────────────────────────────────────────────────
// U8g2 full-buffer mode for 1.3" SH1106 128x64 I2C OLED.
// Provides drawing helpers for consistent ZOMBI SS UI on small screen.

// Display object — SH1106 128x64 I2C, full buffer
// Use U8G2_SSD1306_128X64 if your module has SSD1306 instead
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// ── Layout Constants ──────────────────────────────────────────────────────────
#define HEADER_H     10   // Title bar height
#define LINE_H       10   // Text line height (with 6x10 font)
#define MAX_VISIBLE   5   // Visible parameter lines (rows 1-5)
#define FONT_SMALL    u8g2_font_5x7_tr
#define FONT_MEDIUM   u8g2_font_6x10_tr
#define FONT_LARGE    u8g2_font_8x13B_tr
#define FONT_ICON     u8g2_font_open_iconic_play_1x_t

namespace OledUI {

  void init() {
    Wire.setSDA(OLED_SDA_PIN);
    Wire.setSCL(OLED_SCL_PIN);
    Wire.begin();
    display.begin();
    display.setContrast(200);
    display.clearBuffer();
    display.sendBuffer();
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
  void drawCentered(const char* text, int y, const u8g2_font_t* font = FONT_MEDIUM) {
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
  // Each step: 7px wide, 1px gap = 8px per step = 128px total
  void drawStepGrid(int y, int h, bool steps[16], int currentStep, bool playing) {
    for (int i = 0; i < 16; i++) {
      int x = i * 8;
      if (steps[i]) {
        display.drawBox(x, y, 7, h);
      } else {
        display.drawFrame(x, y, 7, h);
      }
      // Playhead indicator
      if (playing && i == currentStep) {
        display.drawHLine(x, y + h + 1, 7);
      }
    }
  }

  // Splash screen
  void drawSplash() {
    clear();
    display.setFont(FONT_LARGE);
    drawCentered("ZOMBI SS", 28, FONT_LARGE);
    display.setFont(FONT_MEDIUM);
    drawCentered("PROPHET SYNTH v3", 42, FONT_MEDIUM);
    display.setFont(FONT_SMALL);
    drawCentered("RP2040 Dual-Core", 56, FONT_SMALL);
    send();
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
