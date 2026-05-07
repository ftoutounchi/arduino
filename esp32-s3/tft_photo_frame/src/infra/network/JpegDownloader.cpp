#include "infra/network/JpegDownloader.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>
#include <cctype>
#include <cstring>

#include "config/AppConfig.h"

namespace {
uint16_t readU16(const uint8_t* p, bool littleEndian) {
  if (littleEndian) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
  }
  return (static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]);
}

uint32_t readU32(const uint8_t* p, bool littleEndian) {
  if (littleEndian) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
  }
  return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

bool readIfdAsciiTag(const uint8_t* tiff,
                     size_t tiffLen,
                     uint32_t ifdOffset,
                     bool littleEndian,
                     uint16_t wantedTag,
                     String* outRaw) {
  if (outRaw == nullptr || ifdOffset + 2 > tiffLen) {
    return false;
  }

  const uint16_t entryCount = readU16(tiff + ifdOffset, littleEndian);
  size_t entryPos = ifdOffset + 2;

  for (uint16_t i = 0; i < entryCount; ++i) {
    if (entryPos + 12 > tiffLen) {
      return false;
    }

    const uint16_t tag = readU16(tiff + entryPos, littleEndian);
    const uint16_t type = readU16(tiff + entryPos + 2, littleEndian);
    const uint32_t count = readU32(tiff + entryPos + 4, littleEndian);
    const uint8_t* valueOrOffset = tiff + entryPos + 8;

    if (tag == wantedTag && type == 2 && count > 1) {
      const char* text = nullptr;
      if (count <= 4) {
        text = reinterpret_cast<const char*>(valueOrOffset);
      } else {
        const uint32_t valueOffset = readU32(valueOrOffset, littleEndian);
        if (valueOffset >= tiffLen || valueOffset + count > tiffLen) {
          return false;
        }
        text = reinterpret_cast<const char*>(tiff + valueOffset);
      }

      size_t textLen = count;
      while (textLen > 0 && (text[textLen - 1] == '\0' || isspace(static_cast<unsigned char>(text[textLen - 1])))) {
        --textLen;
      }
      if (textLen == 0) {
        return false;
      }

      outRaw->reserve(textLen);
      *outRaw = "";
      for (size_t k = 0; k < textLen; ++k) {
        const char c = text[k];
        if (c == '\0') {
          break;
        }
        outRaw->concat(c);
      }
      return outRaw->length() > 0;
    }

    entryPos += 12;
  }

  return false;
}

bool readIfdOffsetTag(const uint8_t* tiff,
                      size_t tiffLen,
                      uint32_t ifdOffset,
                      bool littleEndian,
                      uint16_t wantedTag,
                      uint32_t* outOffset) {
  if (outOffset == nullptr || ifdOffset + 2 > tiffLen) {
    return false;
  }

  const uint16_t entryCount = readU16(tiff + ifdOffset, littleEndian);
  size_t entryPos = ifdOffset + 2;

  for (uint16_t i = 0; i < entryCount; ++i) {
    if (entryPos + 12 > tiffLen) {
      return false;
    }

    const uint16_t tag = readU16(tiff + entryPos, littleEndian);
    const uint16_t type = readU16(tiff + entryPos + 2, littleEndian);
    const uint32_t count = readU32(tiff + entryPos + 4, littleEndian);
    const uint8_t* valueOrOffset = tiff + entryPos + 8;

    if (tag == wantedTag && count == 1) {
      if (type == 4) {
        *outOffset = readU32(valueOrOffset, littleEndian);
        return true;
      }
      if (type == 3) {
        *outOffset = readU16(valueOrOffset, littleEndian);
        return true;
      }
      return false;
    }

    entryPos += 12;
  }

  return false;
}
}  // namespace

JpegDownloader::JpegDownloader(WifiManager& wifi)
    : wifi_(wifi),
      nextSourceIndex_(0),
      sourceCount_(0),
      githubToken_(""),
      githubUserAgent_("esp32-photo-frame"),
      lastError_("") {}

void JpegDownloader::begin() {
  clearSources();
  loadUrlsFromJson();
  loadGithubAuthFromJson();
}

bool JpegDownloader::downloadAnyImage(DownloadResult* outResult) {
  if (outResult == nullptr) {
    lastError_ = "Internal output error";
    return false;
  }
  outResult->data = nullptr;
  outResult->len = 0;
  outResult->exifDateTime = "";
  lastError_ = "";

  if (!wifi_.ensureConnected()) {
    lastError_ = "WiFi disconnected";
    return false;
  }

  if (sourceCount_ == 0) {
    Serial.println("No JSON sources, trying random fallback URL");
  } else {
    const size_t startIndex = nextSourceIndex_;
    nextSourceIndex_ = (nextSourceIndex_ + 1) % sourceCount_;
    constexpr uint8_t kNumberedRetries = 3;

    for (size_t offset = 0; offset < sourceCount_; ++offset) {
      const size_t idx = (startIndex + offset) % sourceCount_;
      const PhotoSource& src = sources_[idx];
      Serial.printf("Trying source %u/%u\n", static_cast<unsigned>(idx + 1),
                    static_cast<unsigned>(sourceCount_));

      if (src.mode == SourceMode::kDirectUrl) {
        if (downloadCompatibleJpeg(src.directUrl.c_str(), outResult)) {
          return true;
        }
        continue;
      }

      for (uint8_t attempt = 0; attempt < kNumberedRetries; ++attempt) {
        String url;
        if (!buildNumberedRandomUrl(src.numbered, &url)) {
          lastError_ = "Invalid numbered_random config";
          break;
        }
        Serial.printf("  numbered_random pick %u/%u\n", static_cast<unsigned>(attempt + 1),
                      static_cast<unsigned>(kNumberedRetries));
        if (downloadCompatibleJpeg(url.c_str(), outResult)) {
          return true;
        }
      }
    }
  }

  char randomUrl[128];
  const uint32_t seed = static_cast<uint32_t>(esp_random());
  snprintf(randomUrl, sizeof(randomUrl), "https://picsum.photos/seed/%lu/320/320.jpg", static_cast<unsigned long>(seed));
  Serial.println("Trying random fallback image URL");
  if (downloadCompatibleJpeg(randomUrl, outResult)) {
    return true;
  }

  if (sourceCount_ == 0) {
    lastError_ = "No photo sources in JSON";
  }
  return false;
}

void JpegDownloader::clearSources() {
  sourceCount_ = 0;
  nextSourceIndex_ = 0;
}

bool JpegDownloader::addDirectUrlSource(const String& url) {
  if (sourceCount_ >= Config::kMaxPhotoSources) {
    return false;
  }
  String trimmed = url;
  trimmed.trim();
  if (trimmed.isEmpty()) {
    return false;
  }

  PhotoSource& src = sources_[sourceCount_++];
  src.mode = SourceMode::kDirectUrl;
  src.directUrl = trimmed;
  return true;
}

bool JpegDownloader::addNumberedRandomSource(JsonObjectConst obj) {
  if (sourceCount_ >= Config::kMaxPhotoSources) {
    return false;
  }

  const char* base = obj["repo_api_base"] | "";
  if (base == nullptr || base[0] == '\0') {
    return false;
  }

  uint32_t startId = obj["start_id"] | 1;
  uint32_t endId = obj["end_id"] | 0;
  if (endId < startId) {
    return false;
  }

  uint8_t digits = obj["digits"] | 0;
  if (digits > 9) {
    digits = 9;
  }

  PhotoSource& src = sources_[sourceCount_++];
  src.mode = SourceMode::kNumberedRandom;
  src.numbered.repoApiBase = String(base);
  src.numbered.prefix = String(static_cast<const char*>(obj["prefix"] | "photo_"));
  src.numbered.ext = String(static_cast<const char*>(obj["ext"] | ".jpg"));
  src.numbered.ref = String(static_cast<const char*>(obj["ref"] | "main"));
  src.numbered.startId = startId;
  src.numbered.endId = endId;
  src.numbered.digits = digits;
  return true;
}

bool JpegDownloader::buildNumberedRandomUrl(const NumberedRandomSource& src, String* outUrl) const {
  if (outUrl == nullptr || src.endId < src.startId) {
    return false;
  }

  const uint32_t span = (src.endId - src.startId) + 1;
  const uint32_t id = src.startId + (static_cast<uint32_t>(esp_random()) % span);

  char idBuf[16];
  if (src.digits > 0) {
    snprintf(idBuf, sizeof(idBuf), "%0*u", static_cast<int>(src.digits), static_cast<unsigned>(id));
  } else {
    snprintf(idBuf, sizeof(idBuf), "%u", static_cast<unsigned>(id));
  }

  String base = src.repoApiBase;
  base.trim();
  while (base.endsWith("/")) {
    base.remove(base.length() - 1);
  }

  String url = base + "/" + src.prefix + idBuf + src.ext;
  if (!src.ref.isEmpty()) {
    url += "?ref=";
    url += src.ref;
  }
  *outUrl = url;
  return true;
}

void JpegDownloader::loadUrlsFromJson() {
  if (!LittleFS.begin(false)) {
    Serial.println("LittleFS mount failed; no fallback URL list available");
    lastError_ = "LittleFS mount failed";
    return;
  }

  File f = LittleFS.open(Config::kPhotoUrlsJsonPath, "r");
  if (!f) {
    Serial.println("photo_urls.json not found; no fallback URL list available");
    lastError_ = "photo_urls.json missing";
    return;
  }

  DynamicJsonDocument doc(12288);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("photo_urls.json parse error: %s\n", err.c_str());
    lastError_ = "photo_urls.json parse error";
    return;
  }

  JsonArray list;
  if (doc.is<JsonArray>()) {
    list = doc.as<JsonArray>();
  } else if (doc["sources"].is<JsonArray>()) {
    list = doc["sources"].as<JsonArray>();
  } else if (doc["photo_urls"].is<JsonArray>()) {
    // Backward-compatible mode: plain URL list.
    list = doc["photo_urls"].as<JsonArray>();
  } else {
    Serial.println("photo_urls.json missing sources array");
    lastError_ = "photo_urls missing array";
    return;
  }

  clearSources();
  for (JsonVariant v : list) {
    if (sourceCount_ >= Config::kMaxPhotoSources) {
      break;
    }
    if (v.is<const char*>()) {
      addDirectUrlSource(String(v.as<const char*>()));
      continue;
    }
    if (!v.is<JsonObjectConst>()) {
      continue;
    }

    JsonObjectConst obj = v.as<JsonObjectConst>();
    const char* mode = obj["mode"] | "";
    if (strcmp(mode, "numbered_random") == 0) {
      addNumberedRandomSource(obj);
      continue;
    }
    if (strcmp(mode, "direct_url") == 0) {
      addDirectUrlSource(String(static_cast<const char*>(obj["url"] | "")));
      continue;
    }
  }

  if (sourceCount_ == 0) {
    Serial.println("photo_urls.json has no usable sources");
    lastError_ = "photo_urls has no usable sources";
    return;
  }

  nextSourceIndex_ = 0;
  Serial.printf("Loaded %u photo source(s) from %s\n", static_cast<unsigned>(sourceCount_),
                Config::kPhotoUrlsJsonPath);
}

const String& JpegDownloader::lastError() const {
  return lastError_;
}

void JpegDownloader::loadGithubAuthFromJson() {
  File f = LittleFS.open(Config::kGithubAuthJsonPath, "r");
  if (!f) {
    Serial.println("github_auth.json not found; GitHub private access disabled");
    return;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("github_auth.json parse error: %s\n", err.c_str());
    return;
  }

  if (doc["github_token"].is<const char*>()) {
    githubToken_ = doc["github_token"].as<const char*>();
  }
  if (doc["user_agent"].is<const char*>()) {
    githubUserAgent_ = doc["user_agent"].as<const char*>();
  }

  githubToken_.trim();
  githubUserAgent_.trim();
  if (githubUserAgent_.isEmpty()) {
    githubUserAgent_ = "esp32-photo-frame";
  }

  if (githubToken_.isEmpty()) {
    Serial.println("github_auth.json loaded but token is empty");
    return;
  }

  if (githubToken_.length() > Config::kMaxGithubTokenLength) {
    githubToken_ = githubToken_.substring(0, Config::kMaxGithubTokenLength);
  }
  if (githubUserAgent_.length() > Config::kMaxUserAgentLength) {
    githubUserAgent_ = githubUserAgent_.substring(0, Config::kMaxUserAgentLength);
  }

  Serial.println("GitHub auth loaded");
}

bool JpegDownloader::isGithubApiUrl(const char* url) const {
  if (url == nullptr) {
    return false;
  }
  return strncmp(url, "https://api.github.com/", 23) == 0;
}

bool JpegDownloader::downloadCompatibleJpeg(const char* url, DownloadResult* outResult) {
  uint8_t* data = nullptr;
  size_t len = 0;
  if (!downloadJpeg(url, &data, &len)) {
    return false;
  }

  if (!isLikelySupportedJpeg(data, len)) {
    Serial.println("Downloaded JPEG not baseline-compatible for decoder");
    lastError_ = "Unsupported JPEG (must be baseline)";
    free(data);
    return false;
  }

  outResult->data = data;
  outResult->len = len;
  outResult->exifDateTime = "";
  if (extractExifDateTime(data, len, &outResult->exifDateTime)) {
    Serial.printf("EXIF date/time: %s\n", outResult->exifDateTime.c_str());
  } else {
    Serial.println("EXIF date/time missing");
  }

  return true;
}

bool JpegDownloader::downloadJpeg(const char* url, uint8_t** outData, size_t* outLen) {
  *outData = nullptr;
  *outLen = 0;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  https.setTimeout(20000);

  if (!https.begin(client, url)) {
    Serial.println("HTTP begin failed");
    lastError_ = "HTTP begin failed";
    return false;
  }

  if (isGithubApiUrl(url)) {
    https.addHeader("User-Agent", githubUserAgent_);
    https.addHeader("Accept", "application/vnd.github.raw");
    if (!githubToken_.isEmpty()) {
      https.addHeader("Authorization", "Bearer " + githubToken_);
    } else {
      Serial.println("Warning: GitHub API URL without token");
    }
  }

  Serial.print("Downloading image: ");
  Serial.println(url);

  const int httpCode = https.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("HTTP GET failed, code=%d\n", httpCode);
    lastError_ = "HTTP " + String(httpCode);
    https.end();
    return false;
  }

  int announcedLen = https.getSize();
  if (announcedLen > 0 && static_cast<size_t>(announcedLen) > Config::kMaxJpegBytes) {
    Serial.printf("Image too large: %d bytes\n", announcedLen);
    lastError_ = "Image too large";
    https.end();
    return false;
  }

  size_t capacity = (announcedLen > 0) ? static_cast<size_t>(announcedLen) : 64 * 1024;
  if (capacity > Config::kMaxJpegBytes) {
    capacity = Config::kMaxJpegBytes;
  }

  uint8_t* data = static_cast<uint8_t*>(malloc(capacity));
  if (data == nullptr) {
    Serial.println("Not enough memory for JPEG buffer");
    lastError_ = "Out of memory";
    https.end();
    return false;
  }

  WiFiClient* stream = https.getStreamPtr();
  uint8_t chunk[1024];
  size_t total = 0;

  while (https.connected() && (announcedLen < 0 || total < static_cast<size_t>(announcedLen))) {
    size_t avail = stream->available();
    if (avail == 0) {
      delay(2);
      continue;
    }

    size_t toRead = avail;
    if (toRead > sizeof(chunk)) {
      toRead = sizeof(chunk);
    }

    const int readCount = stream->readBytes(chunk, toRead);
    if (readCount <= 0) {
      continue;
    }

    if (total + static_cast<size_t>(readCount) > Config::kMaxJpegBytes) {
      Serial.println("JPEG exceeded max allowed size");
      lastError_ = "JPEG exceeded max size";
      free(data);
      https.end();
      return false;
    }

    if (total + static_cast<size_t>(readCount) > capacity) {
      size_t newCapacity = capacity * 2;
      if (newCapacity < total + static_cast<size_t>(readCount)) {
        newCapacity = total + static_cast<size_t>(readCount);
      }
      if (newCapacity > Config::kMaxJpegBytes) {
        newCapacity = Config::kMaxJpegBytes;
      }

      uint8_t* grown = static_cast<uint8_t*>(realloc(data, newCapacity));
      if (grown == nullptr) {
        Serial.println("Failed to grow JPEG buffer");
        lastError_ = "Out of memory (realloc)";
        free(data);
        https.end();
        return false;
      }

      data = grown;
      capacity = newCapacity;
    }

    memcpy(data + total, chunk, static_cast<size_t>(readCount));
    total += static_cast<size_t>(readCount);
  }

  https.end();

  if (total == 0) {
    Serial.println("No image data received");
    lastError_ = "No image data";
    free(data);
    return false;
  }

  if (total < 4 || data[0] != 0xFF || data[1] != 0xD8) {
    Serial.println("Downloaded data is not a JPEG stream");
    lastError_ = "Response is not JPEG";
    free(data);
    return false;
  }

  *outData = data;
  *outLen = total;
  Serial.printf("Downloaded JPEG bytes: %u\n", static_cast<unsigned>(total));
  return true;
}

bool JpegDownloader::isLikelySupportedJpeg(const uint8_t* data, size_t len) {
  if (len < 4 || data[0] != 0xFF || data[1] != 0xD8) {
    return false;
  }

  size_t i = 2;
  while (i + 1 < len) {
    if (data[i] != 0xFF) {
      ++i;
      continue;
    }

    while (i < len && data[i] == 0xFF) {
      ++i;
    }
    if (i >= len) {
      return false;
    }

    const uint8_t marker = data[i++];
    if (marker == 0xD9) {
      break;
    }
    if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
      continue;
    }

    if (i + 1 >= len) {
      return false;
    }
    const uint16_t segLen = (static_cast<uint16_t>(data[i]) << 8) | data[i + 1];
    i += 2;
    if (segLen < 2 || i + segLen - 2 > len) {
      return false;
    }

    if (marker == 0xC0) {
      if (segLen < 8) {
        return false;
      }
      const uint8_t components = data[i + 5];
      return components == 1 || components == 3;
    }

    if ((marker >= 0xC1 && marker <= 0xCF) && marker != 0xC4 && marker != 0xC8 && marker != 0xCC) {
      return false;
    }

    i += segLen - 2;
  }

  return false;
}

bool JpegDownloader::extractExifDateTime(const uint8_t* data, size_t len, String* outDateTime) const {
  if (outDateTime == nullptr) {
    return false;
  }
  *outDateTime = "";

  if (data == nullptr || len < 4 || data[0] != 0xFF || data[1] != 0xD8) {
    return false;
  }

  size_t i = 2;
  while (i + 1 < len) {
    if (data[i] != 0xFF) {
      ++i;
      continue;
    }

    while (i < len && data[i] == 0xFF) {
      ++i;
    }
    if (i >= len) {
      break;
    }

    const uint8_t marker = data[i++];
    if (marker == 0xD9 || marker == 0xDA) {
      break;
    }
    if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
      continue;
    }

    if (i + 1 >= len) {
      break;
    }
    const uint16_t segLen = (static_cast<uint16_t>(data[i]) << 8) | data[i + 1];
    i += 2;
    if (segLen < 2 || i + segLen - 2 > len) {
      break;
    }

    if (marker == 0xE1 && parseExifDateTimeFromApp1(data + i, segLen - 2, outDateTime)) {
      return true;
    }

    i += segLen - 2;
  }

  return false;
}

bool JpegDownloader::parseExifDateTimeFromApp1(const uint8_t* app1Data, size_t app1Len, String* outDateTime) {
  if (outDateTime == nullptr || app1Data == nullptr || app1Len < 14) {
    return false;
  }
  if (memcmp(app1Data, "Exif\0\0", 6) != 0) {
    return false;
  }

  const uint8_t* tiff = app1Data + 6;
  const size_t tiffLen = app1Len - 6;
  if (tiffLen < 8) {
    return false;
  }

  const bool littleEndian = (tiff[0] == 'I' && tiff[1] == 'I');
  const bool bigEndian = (tiff[0] == 'M' && tiff[1] == 'M');
  if (!littleEndian && !bigEndian) {
    return false;
  }

  if (readU16(tiff + 2, littleEndian) != 42) {
    return false;
  }

  const uint32_t ifd0Offset = readU32(tiff + 4, littleEndian);
  if (ifd0Offset >= tiffLen) {
    return false;
  }

  String rawDateTime;
  uint32_t exifIfdOffset = 0;
  if (readIfdOffsetTag(tiff, tiffLen, ifd0Offset, littleEndian, 0x8769, &exifIfdOffset) &&
      exifIfdOffset < tiffLen) {
    if (readIfdAsciiTag(tiff, tiffLen, exifIfdOffset, littleEndian, 0x9003, &rawDateTime) &&
        formatExifDateTime(rawDateTime.c_str(), outDateTime)) {
      return true;
    }
    if (readIfdAsciiTag(tiff, tiffLen, exifIfdOffset, littleEndian, 0x9004, &rawDateTime) &&
        formatExifDateTime(rawDateTime.c_str(), outDateTime)) {
      return true;
    }
  }

  if (readIfdAsciiTag(tiff, tiffLen, ifd0Offset, littleEndian, 0x0132, &rawDateTime) &&
      formatExifDateTime(rawDateTime.c_str(), outDateTime)) {
    return true;
  }

  return false;
}

bool JpegDownloader::formatExifDateTime(const char* rawExif, String* outDateTime) {
  if (rawExif == nullptr || outDateTime == nullptr) {
    return false;
  }

  // Expected EXIF format starts with: YYYY:MM...
  if (strlen(rawExif) < 7) {
    return false;
  }
  for (size_t i = 0; i < 4; ++i) {
    if (isdigit(static_cast<unsigned char>(rawExif[i])) == 0) {
      return false;
    }
  }
  if (rawExif[4] != ':' || isdigit(static_cast<unsigned char>(rawExif[5])) == 0 ||
      isdigit(static_cast<unsigned char>(rawExif[6])) == 0) {
    return false;
  }

  char out[8];
  out[0] = rawExif[0];
  out[1] = rawExif[1];
  out[2] = rawExif[2];
  out[3] = rawExif[3];
  out[4] = '.';
  out[5] = rawExif[5];
  out[6] = rawExif[6];
  out[7] = '\0';
  *outDateTime = String(out);
  return true;
}
