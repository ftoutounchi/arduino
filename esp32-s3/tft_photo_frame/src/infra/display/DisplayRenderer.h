#pragma once

#include <Arduino.h>
#include <Adafruit_ST7789.h>

class DisplayRenderer {
 public:
  DisplayRenderer();

  void begin();
  void applyDisplayTransform();
  void setDisplayTransform(uint8_t transform);
  void showStatus(const char* line1, const char* line2 = nullptr);
  bool renderJpeg(const uint8_t* jpgData, size_t jpgLen, const char* exifDateTime = nullptr);
  void fillScreen(uint16_t color);
  void drawRgbBitmap(int16_t x, int16_t y, const uint16_t* bitmap, uint16_t w, uint16_t h);

 private:
  static DisplayRenderer* activeInstance_;

  Adafruit_ST7789 tft_;
  int16_t imageOffsetX_;
  int16_t imageOffsetY_;
  uint8_t displayTransform_;

  static bool tftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
  bool drawBlock(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
  void writeMadctl(uint8_t transform);
  void drawExifBadge(const char* exifDateTime);
  void setBacklight(bool on);
};
