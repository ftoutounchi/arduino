#pragma once

#include <Arduino.h>

#include "infra/network/JpegDownloader.h"

class PhotoDownloadService {
 public:
  explicit PhotoDownloadService(JpegDownloader& downloader);
  ~PhotoDownloadService();

  bool begin();
  void resetDownloader();

  bool isDownloadInProgress() const;
  bool shouldStartDownload() const;
  void startBackgroundDownload();

  bool takeReadyImage(uint8_t** outData, size_t* outLen, String* outExifDateTime);
  bool takeFailureMessage(String* outMessage);

 private:
  JpegDownloader& downloader_;
  mutable SemaphoreHandle_t stateMutex_;

  bool downloadInProgress_;
  bool pendingImageReady_;
  uint32_t lastDownloadAttemptMs_;
  uint8_t* pendingJpgData_;
  size_t pendingJpgLen_;
  String pendingExifDateTime_;
  bool failureMessagePending_;
  String failureMessage_;

  static void downloaderTaskEntry(void* context);
  void runDownloaderTask();
};
