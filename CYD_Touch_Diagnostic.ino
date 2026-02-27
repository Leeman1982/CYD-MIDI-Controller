/*******************************************************************
 * 2.4" CYD Touch Screen Diagnostic Tool
 *
 * This sketch helps identify the correct touch configuration for your
 * specific 2.4" ESP32 display board.
 *
 * Instructions:
 * 1. Upload this sketch
 * 2. Open Serial Monitor (115200 baud)
 * 3. Touch the screen
 * 4. Report which configuration works
 *******************************************************************/

#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

// Configuration 1: Standard ESP32-2432S024C (Most Common)
#define CONFIG1_IRQ 36
#define CONFIG1_MOSI 32
#define CONFIG1_MISO 39
#define CONFIG1_CLK 25
#define CONFIG1_CS 33

// Configuration 2: Older 2.4" variant
#define CONFIG2_IRQ 36
#define CONFIG2_MOSI 32
#define CONFIG2_MISO 39
#define CONFIG2_CLK 25
#define CONFIG2_CS 33

// Configuration 3: Some boards with shared SPI
#define CONFIG3_IRQ 36
#define CONFIG3_MOSI 13  // Shared with display
#define CONFIG3_MISO 12  // Shared with display
#define CONFIG3_CLK 14   // Shared with display
#define CONFIG3_CS 33

// Try Configuration 1 first (most common)
SPIClass touchSPI = SPIClass(VSPI);
XPT2046_Touchscreen ts(CONFIG1_CS, CONFIG1_IRQ);

bool touchDetected = false;
int configTested = 1;

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n===========================================");
  Serial.println("2.4\" CYD Touch Screen Diagnostic");
  Serial.println("===========================================\n");

  // Initialize display
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  tft.drawString("CYD Touch Test", 20, 20);
  tft.drawString("Touch anywhere", 20, 50);
  tft.setTextSize(1);
  tft.drawString("Check Serial Monitor", 20, 80);

  Serial.println("Display initialized: 240x320");
  Serial.println("Driver: ILI9341_2_DRIVER");
  Serial.println();

  // Print board info
  Serial.println("ESP32 Chip Info:");
  Serial.printf("  Model: %s\n", ESP.getChipModel());
  Serial.printf("  Revision: %d\n", ESP.getChipRevision());
  Serial.printf("  Cores: %d\n", ESP.getChipCores());
  Serial.printf("  Flash: %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
  Serial.println();

  // Test Configuration 1 (VSPI)
  Serial.println("Testing Configuration 1: VSPI with standard pins");
  Serial.println("  Touch IRQ:  GPIO 36");
  Serial.println("  Touch MOSI: GPIO 32");
  Serial.println("  Touch MISO: GPIO 39");
  Serial.println("  Touch CLK:  GPIO 25");
  Serial.println("  Touch CS:   GPIO 33");
  Serial.println();

  touchSPI.begin(CONFIG1_CLK, CONFIG1_MISO, CONFIG1_MOSI, CONFIG1_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  Serial.println("Touch controller initialized");
  Serial.println("Touch the screen to test...");
  Serial.println();
}

void loop() {
  // Check for touch
  if (ts.touched()) {
    TS_Point p = ts.getPoint();

    if (!touchDetected) {
      touchDetected = true;
      Serial.println("========================================");
      Serial.println("✓ TOUCH DETECTED!");
      Serial.println("========================================");
      Serial.printf("Configuration %d WORKS!\n", configTested);
      Serial.println();

      tft.fillRect(0, 100, 320, 100, TFT_GREEN);
      tft.setTextColor(TFT_BLACK, TFT_GREEN);
      tft.setTextSize(3);
      tft.drawString("TOUCH OK!", 40, 120);
      tft.setTextSize(1);
    }

    // Print raw touch coordinates
    Serial.printf("Raw Touch: X=%d, Y=%d, Z=%d\n", p.x, p.y, p.z);

    // Map to screen coordinates (approximate)
    int screenX = map(p.x, 200, 3700, 0, 320);
    int screenY = map(p.y, 200, 3900, 0, 240);

    Serial.printf("Mapped: X=%d, Y=%d\n", screenX, screenY);
    Serial.println();

    // Draw touch point
    tft.fillCircle(screenX, screenY, 5, TFT_RED);

    delay(100);
  } else {
    if (touchDetected) {
      Serial.println("Touch released\n");
      touchDetected = false;
    }
  }

  delay(50);
}
