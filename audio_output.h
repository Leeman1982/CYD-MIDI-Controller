#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H

#include <Arduino.h>
#include <driver/i2s.h>
#include "synth_config.h"
#include "synth_engine.h"

// ============================================================================
// I2S Audio Output for PCM5102A DAC
// ============================================================================
// Manages I2S output and audio buffer generation
// Runs on Core 1 for optimal performance
// ============================================================================

class AudioOutput {
private:
  SynthEngine* synthEngine;
  bool initialized;
  TaskHandle_t audioTaskHandle;

  static void audioTaskWrapper(void* parameter) {
    AudioOutput* audioOut = static_cast<AudioOutput*>(parameter);
    audioOut->audioTask();
  }

  void audioTask() {
    int16_t buffer[BUFFER_SIZE * 2]; // Stereo interleaved
    size_t bytesWritten;

    while (true) {
      // Generate audio samples
      for (int i = 0; i < BUFFER_SIZE; i++) {
        float sample = synthEngine->process();

        // Convert float (-1.0 to 1.0) to 16-bit integer
        int16_t sampleInt = (int16_t)(sample * 32767.0f);

        // Stereo: same signal on both channels for now
        buffer[i * 2] = sampleInt;     // Left
        buffer[i * 2 + 1] = sampleInt; // Right
      }

      // Write to I2S
      i2s_write(I2S_PORT, buffer, sizeof(buffer), &bytesWritten, portMAX_DELAY);
    }
  }

public:
  AudioOutput(SynthEngine* synth) :
    synthEngine(synth),
    initialized(false),
    audioTaskHandle(NULL) {}

  bool init() {
    // Configure I2S
    i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = BUFFER_SIZE,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0
    };

    // Configure I2S pins
    i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_BCK_PIN,
      .ws_io_num = I2S_LRCK_PIN,
      .data_out_num = I2S_DATA_PIN,
      .data_in_num = I2S_PIN_NO_CHANGE
    };

    // Install and start I2S driver
    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
      Serial.printf("Failed to install I2S driver: %d\n", err);
      return false;
    }

    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) {
      Serial.printf("Failed to set I2S pins: %d\n", err);
      return false;
    }

    // Clear I2S buffer
    i2s_zero_dma_buffer(I2S_PORT);

    initialized = true;
    Serial.println("I2S audio output initialized");
    return true;
  }

  void start() {
    if (!initialized) {
      Serial.println("Cannot start audio: not initialized");
      return;
    }

    // Create audio task on Core 1 with high priority
    xTaskCreatePinnedToCore(
      audioTaskWrapper,     // Function
      "AudioTask",          // Name
      4096,                 // Stack size
      this,                 // Parameter
      10,                   // Priority (high)
      &audioTaskHandle,     // Handle
      1                     // Core 1
    );

    Serial.println("Audio task started on Core 1");
  }

  void stop() {
    if (audioTaskHandle != NULL) {
      vTaskDelete(audioTaskHandle);
      audioTaskHandle = NULL;
    }
  }

  bool isInitialized() const {
    return initialized;
  }
};

#endif // AUDIO_OUTPUT_H
