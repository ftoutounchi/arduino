#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "config/AppConfig.h"
#include "infra/wifi/WifiManager.h"

class JpegDownloader {
 public:
  struct DownloadResult {
    uint8_t* data;
    size_t len;
    String exifDateTime;
  };

  explicit JpegDownloader(WifiManager& wifi);

  void begin();
  bool downloadAnyImage(DownloadResult* outResult);
  const String& lastError() const;

 private:
  enum class SourceMode : uint8_t { kDirectUrl, kNumberedRandom };

  struct NumberedRandomSource {
    String repoApiBase;
    String prefix;
    String ext;
    String ref;
    uint32_t startId;
    uint32_t endId;
    uint8_t digits;
  };

  struct PhotoSource {
    SourceMode mode;
    String directUrl;
    NumberedRandomSource numbered;
  };

  WifiManager& wifi_;
  size_t nextSourceIndex_;
  PhotoSource sources_[Config::kMaxPhotoSources];
  size_t sourceCount_;
  String githubToken_;
  String githubUserAgent_;
  String lastError_;

  void clearSources();
  bool addDirectUrlSource(const String& url);
  bool addNumberedRandomSource(JsonObjectConst obj);
  bool buildNumberedRandomUrl(const NumberedRandomSource& src, String* outUrl) const;
  void loadUrlsFromJson();
  void loadGithubAuthFromJson();
  bool isGithubApiUrl(const char* url) const;
  bool downloadCompatibleJpeg(const char* url, DownloadResult* outResult);
  bool downloadJpeg(const char* url, uint8_t** outData, size_t* outLen);
  static bool isLikelySupportedJpeg(const uint8_t* data, size_t len);
  bool extractExifDateTime(const uint8_t* data, size_t len, String* outDateTime) const;
  static bool parseExifDateTimeFromApp1(const uint8_t* app1Data, size_t app1Len, String* outDateTime);
  static bool formatExifDateTime(const char* rawExif, String* outDateTime);
};
