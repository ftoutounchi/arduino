#include "infra/display/DisplayRenderer.h"

#include <SPI.h>
#include <TJpg_Decoder.h>

#include "config/AppConfig.h"

DisplayRenderer* DisplayRenderer::activeInstance_ = nullptr;

DisplayRenderer::DisplayRenderer()
    : tft_(Config::kPinCs, Config::kPinDc, Config::kPinMosi, Config::kPinSclk, Config::kPinRst),
      imageOffsetX_(0),
      imageOffsetY_(0),
      displayTransform_(Config::kDefaultDisplayTransform) {}

void DisplayRenderer::begin() {
  setBacklight(true);
  SPI.begin(Config::kPinSclk, -1, Config::kPinMosi, Config::kPinCs);
  tft_.init(Config::kDisplayWidth, Config::kDisplayHeight, SPI_MODE0);
  tft_.setSPISpeed(10000000);
  tft_.setRotation(0);
  applyDisplayTransform();
}

void DisplayRenderer::applyDisplayTransform() {
  setDisplayTransform(Config::gDisplayTransform);
}

void DisplayRenderer::setDisplayTransform(uint8_t transform) {
  if (transform > Config::kDisplayTransformMax) {
    transform = Config::kDefaultDisplayTransform;
  }
  displayTransform_ = transform;
  writeMadctl(displayTransform_);
}

void DisplayRenderer::writeMadctl(uint8_t transform) {
  uint8_t madctl = ST77XX_MADCTL_MX | ST77XX_MADCTL_MY | ST77XX_MADCTL_RGB;  // Default panel orientation.
  switch (transform) {
    case 1:  // Mirror X
      madctl = ST77XX_MADCTL_MY | ST77XX_MADCTL_RGB;
      break;
    case 2:  // Mirror Y
      madctl = ST77XX_MADCTL_MX | ST77XX_MADCTL_RGB;
      break;
    case 3:  // Rotate 180
      madctl = ST77XX_MADCTL_RGB;
      break;
    default:  // Normal
      break;
  }
  tft_.sendCommand(ST77XX_MADCTL, &madctl, 1);
  Serial.printf("Display transform=%u applied\n", static_cast<unsigned>(transform));
}

void DisplayRenderer::showStatus(const char* line1, const char* line2) {
  fillScreen(ST77XX_BLACK);
  tft_.setTextWrap(false);
  tft_.setTextColor(ST77XX_WHITE);
  tft_.setTextSize(2);
  tft_.setCursor(14, 96);
  tft_.print(line1);
  if (line2 != nullptr) {
    tft_.setTextSize(1);
    tft_.setCursor(14, 128);
    tft_.print(line2);
  }
}

bool DisplayRenderer::renderJpeg(const uint8_t* jpgData, size_t jpgLen, const char* exifDateTime) {
  uint16_t w = 0;
  uint16_t h = 0;
  if (TJpgDec.getJpgSize(&w, &h, jpgData, static_cast<uint32_t>(jpgLen)) != JDR_OK || w == 0 || h == 0) {
    Serial.println("Failed to parse JPEG dimensions");
    return false;
  }

  const bool fillMode = Config::gAutoViewSettings.photoFillMode;
  uint8_t scale = 0;
  if (fillMode) {
    // Choose the largest JPEG downscale that still covers the full 6:7 display,
    // so we get a center-cropped "cover" result.
    while (scale < 3 && ((w >> (scale + 1)) >= Config::kDisplayWidth) &&
           ((h >> (scale + 1)) >= Config::kDisplayHeight)) {
      ++scale;
    }
  } else {
    // Fit entire image inside display (letterboxing may appear).
    while (scale < 3 && ((w >> scale) > Config::kDisplayWidth || (h >> scale) > Config::kDisplayHeight)) {
      ++scale;
    }
  }

  const uint8_t scaleDiv = static_cast<uint8_t>(1U << scale);
  const int16_t drawW = static_cast<int16_t>(w >> scale);
  const int16_t drawH = static_cast<int16_t>(h >> scale);
  imageOffsetX_ = (Config::kDisplayWidth - drawW) / 2;
  imageOffsetY_ = (Config::kDisplayHeight - drawH) / 2;

  Serial.printf("Decoded image %ux%u, mode=%s, scale=1/%u, draw=%dx%d, offset=(%d,%d)\n", w, h,
                fillMode ? "fill_6x7" : "fit", static_cast<unsigned>(scaleDiv), drawW, drawH, imageOffsetX_,
                imageOffsetY_);

  fillScreen(ST77XX_BLACK);
  TJpgDec.setJpgScale(scaleDiv);
  TJpgDec.setSwapBytes(false);
  activeInstance_ = this;
  TJpgDec.setCallback(&DisplayRenderer::tftOutput);

  const JRESULT res = TJpgDec.drawJpg(0, 0, jpgData, static_cast<uint32_t>(jpgLen));
  if (res != JDR_OK) {
    Serial.printf("JPEG draw failed, code=%d\n", res);
    return false;
  }

  drawExifBadge(exifDateTime);
  return true;
}

bool DisplayRenderer::tftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (activeInstance_ == nullptr) {
    return false;
  }
  return activeInstance_->drawBlock(x, y, w, h, bitmap);
}

bool DisplayRenderer::drawBlock(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  int16_t drawX = x + imageOffsetX_;
  int16_t drawY = y + imageOffsetY_;
  int16_t srcX = 0;
  int16_t srcY = 0;
  int16_t drawW = static_cast<int16_t>(w);
  int16_t drawH = static_cast<int16_t>(h);

  if (drawX < 0) {
    srcX = -drawX;
    drawW -= srcX;
    drawX = 0;
  }
  if (drawY < 0) {
    srcY = -drawY;
    drawH -= srcY;
    drawY = 0;
  }
  if (drawX + drawW > Config::kDisplayWidth) {
    drawW = Config::kDisplayWidth - drawX;
  }
  if (drawY + drawH > Config::kDisplayHeight) {
    drawH = Config::kDisplayHeight - drawY;
  }

  if (drawW <= 0 || drawH <= 0) {
    // Block is fully outside screen; keep decoding remaining blocks.
    return true;
  }
  // drawRGBBitmap expects contiguous rows. After clipping, source rows still
  // have the original stride (`w`), so draw row-by-row.
  uint16_t* src = bitmap + (srcY * static_cast<int16_t>(w)) + srcX;
  for (int16_t row = 0; row < drawH; ++row) {
    tft_.drawRGBBitmap(drawX, drawY + row, src + (row * static_cast<int16_t>(w)),
                       static_cast<uint16_t>(drawW), 1);
  }
  return true;
}

void DisplayRenderer::fillScreen(uint16_t color) {
  tft_.fillScreen(color);
}

void DisplayRenderer::drawRgbBitmap(int16_t x, int16_t y, const uint16_t* bitmap, uint16_t w, uint16_t h) {
  if (bitmap == nullptr || w == 0 || h == 0) {
    return;
  }

  if (x >= Config::kDisplayWidth || y >= Config::kDisplayHeight) {
    return;
  }

  tft_.drawRGBBitmap(x, y, const_cast<uint16_t*>(bitmap), w, h);
}

void DisplayRenderer::drawExifBadge(const char* exifDateTime) {
  if (exifDateTime == nullptr || exifDateTime[0] == '\0') {
    return;
  }

  tft_.setTextWrap(false);
  constexpr uint8_t kTextSize = 3;
  tft_.setTextSize(kTextSize);

  int16_t textX = 0;
  int16_t textY = 0;
  uint16_t textW = 0;
  uint16_t textH = 0;
  tft_.getTextBounds(exifDateTime, 0, 0, &textX, &textY, &textW, &textH);
  if (textW == 0 || textH == 0) {
    return;
  }

  constexpr int16_t kPadX = 10;
  constexpr int16_t kPadY = 8;
  constexpr int16_t kBottomMargin = 8;
  constexpr int16_t kRadius = 10;

  const int16_t boxW = static_cast<int16_t>(textW) + (kPadX * 2);
  const int16_t boxH = static_cast<int16_t>(textH) + (kPadY * 2);
  const int16_t boxX = (Config::kDisplayWidth - boxW) / 2;
  const int16_t boxY = Config::kDisplayHeight - boxH - kBottomMargin;

  tft_.fillRoundRect(boxX, boxY, boxW, boxH, kRadius, ST77XX_BLACK);
  tft_.drawRoundRect(boxX, boxY, boxW, boxH, kRadius, ST77XX_WHITE);
  tft_.setTextColor(ST77XX_WHITE);
  tft_.setCursor(boxX + kPadX, boxY + kPadY);
  tft_.print(exifDateTime);
}

void DisplayRenderer::setBacklight(bool on) {
  pinMode(Config::kPinBl, OUTPUT);
  if (Config::kBacklightActiveHigh) {
    digitalWrite(Config::kPinBl, on ? HIGH : LOW);
  } else {
    digitalWrite(Config::kPinBl, on ? LOW : HIGH);
  }
}
