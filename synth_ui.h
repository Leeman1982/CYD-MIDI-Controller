#ifndef SYNTH_UI_H
#define SYNTH_UI_H

#include <TFT_eSPI.h>
#include "synth_config.h"

// ============================================================================
// Synth UI Components
// ============================================================================
// Touchscreen UI elements for the synthesizer
// ============================================================================

extern TFT_eSPI tft;

// Touch state structure
struct TouchState {
  bool isPressed;
  bool justPressed;
  bool justReleased;
  int x;
  int y;
};

extern TouchState touch;

// UI Helper Functions
namespace SynthUI {

  // Draw a rotary knob
  void drawKnob(int x, int y, int radius, String label, float value, String valueStr, bool selected = false) {
    // Outer circle
    uint16_t color = selected ? COLOR_PRIMARY : COLOR_KNOB;
    tft.drawCircle(x, y, radius, color);
    tft.drawCircle(x, y, radius - 1, color);

    // Value arc (270 degrees range)
    float angle = -135.0f + (value * 270.0f);
    float angleRad = angle * PI / 180.0f;
    int endX = x + (radius - 4) * cos(angleRad);
    int endY = y + (radius - 4) * sin(angleRad);

    // Draw indicator line
    tft.drawLine(x, y, endX, endY, COLOR_INDICATOR);

    // Label
    tft.setTextColor(COLOR_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(label, x, y - radius - 12, 2);

    // Value
    tft.setTextColor(COLOR_TEXT_DIM);
    tft.drawString(valueStr, x, y + radius + 12, 2);
  }

  // Draw a horizontal slider
  void drawSlider(int x, int y, int width, int height, String label, float value, String valueStr, bool selected = false) {
    uint16_t color = selected ? COLOR_PRIMARY : COLOR_SURFACE;

    // Background
    tft.fillRoundRect(x, y, width, height, 4, color);
    tft.drawRoundRect(x, y, width, height, 4, COLOR_TEXT_DIM);

    // Fill
    int fillWidth = (width - 4) * value;
    if (fillWidth > 0) {
      tft.fillRoundRect(x + 2, y + 2, fillWidth, height - 4, 2, COLOR_ACCENT);
    }

    // Label
    tft.setTextColor(COLOR_TEXT);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(label, x, y - 10, 2);

    // Value
    tft.setTextColor(COLOR_TEXT_DIM);
    tft.setTextDatum(MR_DATUM);
    tft.drawString(valueStr, x + width, y - 10, 2);
  }

  // Draw a button
  void drawButton(int x, int y, int width, int height, String text, bool pressed, uint16_t color = COLOR_PRIMARY) {
    uint16_t bgColor = pressed ? color : COLOR_SURFACE;
    uint16_t borderColor = pressed ? COLOR_TEXT : color;

    tft.fillRoundRect(x, y, width, height, 6, bgColor);
    tft.drawRoundRect(x, y, width, height, 6, borderColor);
    tft.drawRoundRect(x + 1, y + 1, width - 2, height - 2, 5, borderColor);

    tft.setTextColor(pressed ? COLOR_BG : COLOR_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(text, x + width / 2, y + height / 2, 2);
  }

  // Check if a point is within a rectangle
  bool isInRect(int px, int py, int x, int y, int w, int h) {
    return (px >= x && px <= x + w && py >= y && py <= y + h);
  }

  // Check if a point is within a circle
  bool isInCircle(int px, int py, int cx, int cy, int radius) {
    int dx = px - cx;
    int dy = py - cy;
    return (dx * dx + dy * dy) <= (radius * radius);
  }

  // Draw page header with title and page indicators
  void drawHeader(String title, int currentPage, int totalPages) {
    tft.fillRect(0, 0, SCREEN_WIDTH, 30, COLOR_SURFACE);

    // Title
    tft.setTextColor(COLOR_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(title, SCREEN_WIDTH / 2, 15, 4);

    // Page indicators
    int dotSpacing = 12;
    int startX = (SCREEN_WIDTH - (totalPages * dotSpacing)) / 2;
    for (int i = 0; i < totalPages; i++) {
      int dotX = startX + i * dotSpacing;
      if (i == currentPage) {
        tft.fillCircle(dotX, 25, 3, COLOR_ACCENT);
      } else {
        tft.drawCircle(dotX, 25, 3, COLOR_TEXT_DIM);
      }
    }
  }

  // Draw nav buttons (prev/next page)
  void drawNavButtons() {
    // Left arrow
    tft.fillTriangle(15, 15, 25, 10, 25, 20, COLOR_TEXT);
    // Right arrow
    tft.fillTriangle(SCREEN_WIDTH - 15, 15, SCREEN_WIDTH - 25, 10, SCREEN_WIDTH - 25, 20, COLOR_TEXT);
  }

  // Format value for display
  String formatValue(float value, int decimals = 1) {
    char buffer[16];
    dtostrf(value, 1, decimals, buffer);
    return String(buffer);
  }

  // Format frequency for display
  String formatFreq(float freq) {
    if (freq >= 1000.0f) {
      return formatValue(freq / 1000.0f, 2) + "kHz";
    } else {
      return formatValue(freq, 0) + "Hz";
    }
  }

  // Format time for display
  String formatTime(float seconds) {
    if (seconds >= 1.0f) {
      return formatValue(seconds, 2) + "s";
    } else {
      return formatValue(seconds * 1000.0f, 0) + "ms";
    }
  }

  // Waveform selector
  void drawWaveformSelector(int x, int y, int width, int height, OscillatorType type, bool selected = false) {
    uint16_t color = selected ? COLOR_PRIMARY : COLOR_SURFACE;
    tft.fillRoundRect(x, y, width, height, 4, color);
    tft.drawRoundRect(x, y, width, height, 4, selected ? COLOR_ACCENT : COLOR_TEXT_DIM);

    // Draw waveform icon
    int centerX = x + width / 2;
    int centerY = y + height / 2;
    int waveWidth = width - 16;
    int waveHeight = height - 16;

    tft.setTextColor(COLOR_TEXT);
    tft.setTextDatum(MC_DATUM);

    switch (type) {
      case OSC_SINE:
        // Draw sine wave
        for (int i = 0; i < waveWidth - 1; i++) {
          float t1 = (float)i / waveWidth * 2.0f * PI;
          float t2 = (float)(i + 1) / waveWidth * 2.0f * PI;
          int y1 = centerY + sin(t1) * waveHeight / 2;
          int y2 = centerY + sin(t2) * waveHeight / 2;
          tft.drawLine(x + 8 + i, y1, x + 8 + i + 1, y2, COLOR_TEXT);
        }
        break;

      case OSC_SAW:
        // Draw sawtooth
        tft.drawLine(x + 8, centerY + waveHeight / 2, x + width - 8, centerY - waveHeight / 2, COLOR_TEXT);
        tft.drawLine(x + width - 8, centerY - waveHeight / 2, x + width - 8, centerY + waveHeight / 2, COLOR_TEXT);
        break;

      case OSC_SQUARE:
        // Draw square wave
        tft.drawLine(x + 8, centerY + waveHeight / 2, x + 8, centerY - waveHeight / 2, COLOR_TEXT);
        tft.drawLine(x + 8, centerY - waveHeight / 2, centerX, centerY - waveHeight / 2, COLOR_TEXT);
        tft.drawLine(centerX, centerY - waveHeight / 2, centerX, centerY + waveHeight / 2, COLOR_TEXT);
        tft.drawLine(centerX, centerY + waveHeight / 2, x + width - 8, centerY + waveHeight / 2, COLOR_TEXT);
        break;

      case OSC_TRIANGLE:
        // Draw triangle wave
        tft.drawLine(x + 8, centerY, centerX, centerY - waveHeight / 2, COLOR_TEXT);
        tft.drawLine(centerX, centerY - waveHeight / 2, x + width - 8, centerY + waveHeight / 2, COLOR_TEXT);
        break;

      case OSC_PULSE:
        tft.drawString("PWM", centerX, centerY, 2);
        break;

      case OSC_NOISE:
        tft.drawString("NOI", centerX, centerY, 2);
        break;
    }
  }

  // Clear screen
  void clearScreen() {
    tft.fillScreen(COLOR_BG);
  }

  // Draw envelope visualization
  void drawEnvelope(int x, int y, int width, int height, float attack, float decay, float sustain, float release) {
    tft.drawRoundRect(x, y, width, height, 4, COLOR_TEXT_DIM);

    // Scale parameters for drawing
    int totalTime = attack + decay + release + 0.5f; // Add sustain time
    float scaleX = width / totalTime;
    float scaleY = height;

    int x1 = x;
    int y1 = y + height;

    // Attack
    int x2 = x + attack * scaleX;
    int y2 = y;
    tft.drawLine(x1, y1, x2, y2, COLOR_ACCENT);

    // Decay
    x1 = x2;
    y1 = y2;
    x2 = x + (attack + decay) * scaleX;
    y2 = y + (1.0f - sustain) * scaleY;
    tft.drawLine(x1, y1, x2, y2, COLOR_ACCENT);

    // Sustain
    x1 = x2;
    y1 = y2;
    x2 = x + (attack + decay + 0.5f) * scaleX;
    y2 = y1;
    tft.drawLine(x1, y1, x2, y2, COLOR_ACCENT);

    // Release
    x1 = x2;
    y1 = y2;
    x2 = x + width - 2;
    y2 = y + height;
    tft.drawLine(x1, y1, x2, y2, COLOR_ACCENT);
  }
}

#endif // SYNTH_UI_H
