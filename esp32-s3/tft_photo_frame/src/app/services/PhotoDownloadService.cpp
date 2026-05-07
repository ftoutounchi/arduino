#include "app/services/PhotoDownloadService.h"

#include "config/AppConfig.h"

PhotoDownloadService::PhotoDownloadService(JpegDownloader& downloader)
    : downloader_(downloader),
      stateMutex_(nullptr),
      downloadInProgress_(false),
      pendingImageReady_(false),
      lastDownloadAttemptMs_(0),
      pendingJpgData_(nullptr),
      pendingJpgLen_(0),
      pendingExifDateTime_(""),
      failureMessagePending_(false),
      failureMessage_("") {}

PhotoDownloadService::~PhotoDownloadService() {
  if (stateMutex_ != nullptr) {
    if (xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
      if (pendingJpgData_ != nullptr) {
        free(pendingJpgData_);
        pendingJpgData_ = nullptr;
      }
      pendingJpgLen_ = 0;
      pendingExifDateTime_ = "";
      pendingImageReady_ = false;
      xSemaphoreGive(stateMutex_);
    }
    vSemaphoreDelete(stateMutex_);
    stateMutex_ = nullptr;
  }
}

bool PhotoDownloadService::begin() {
  if (stateMutex_ != nullptr) {
    return true;
  }

  stateMutex_ = xSemaphoreCreateMutex();
  if (stateMutex_ == nullptr) {
    Serial.println("PhotoDownloadService: failed to create mutex");
    return false;
  }

  return true;
}

void PhotoDownloadService::resetDownloader() {
  downloader_.begin();
}

bool PhotoDownloadService::isDownloadInProgress() const {
  if (stateMutex_ == nullptr) {
    return false;
  }
  if (xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(20)) != pdTRUE) {
    return true;
  }
  const bool inProgress = downloadInProgress_;
  xSemaphoreGive(stateMutex_);
  return inProgress;
}

bool PhotoDownloadService::shouldStartDownload() const {
  if (stateMutex_ == nullptr) {
    return false;
  }

  if (xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(20)) != pdTRUE) {
    return false;
  }

  const uint32_t now = millis();
  const bool due = (lastDownloadAttemptMs_ == 0) ||
                   (now - lastDownloadAttemptMs_ >= Config::gAutoViewSettings.photoRefreshIntervalMs);
  const bool shouldStart = !downloadInProgress_ && due;

  xSemaphoreGive(stateMutex_);
  return shouldStart;
}

void PhotoDownloadService::startBackgroundDownload() {
  if (stateMutex_ == nullptr) {
    return;
  }

  if (xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(50)) != pdTRUE) {
    return;
  }

  if (downloadInProgress_) {
    xSemaphoreGive(stateMutex_);
    return;
  }

  downloadInProgress_ = true;
  xSemaphoreGive(stateMutex_);

  const BaseType_t created = xTaskCreatePinnedToCore(
      &PhotoDownloadService::downloaderTaskEntry,
      "img_download",
      Config::kDownloadTaskStack,
      this,
      1,
      nullptr,
      1);

  if (created != pdPASS) {
    if (xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
      downloadInProgress_ = false;
      xSemaphoreGive(stateMutex_);
    }
    Serial.println("PhotoDownloadService: failed to start download task");
  }
}

bool PhotoDownloadService::takeReadyImage(uint8_t** outData, size_t* outLen, String* outExifDateTime) {
  if (outData == nullptr || outLen == nullptr || stateMutex_ == nullptr) {
    return false;
  }

  *outData = nullptr;
  *outLen = 0;
  if (outExifDateTime != nullptr) {
    *outExifDateTime = "";
  }

  if (xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(20)) != pdTRUE) {
    return false;
  }

  if (!pendingImageReady_ || pendingJpgData_ == nullptr || pendingJpgLen_ == 0) {
    xSemaphoreGive(stateMutex_);
    return false;
  }

  *outData = pendingJpgData_;
  *outLen = pendingJpgLen_;
  if (outExifDateTime != nullptr) {
    *outExifDateTime = pendingExifDateTime_;
  }

  pendingJpgData_ = nullptr;
  pendingJpgLen_ = 0;
  pendingExifDateTime_ = "";
  pendingImageReady_ = false;

  xSemaphoreGive(stateMutex_);
  return true;
}

bool PhotoDownloadService::takeFailureMessage(String* outMessage) {
  if (outMessage == nullptr || stateMutex_ == nullptr) {
    return false;
  }

  if (xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(20)) != pdTRUE) {
    return false;
  }

  if (!failureMessagePending_) {
    xSemaphoreGive(stateMutex_);
    return false;
  }

  *outMessage = failureMessage_;
  failureMessagePending_ = false;
  xSemaphoreGive(stateMutex_);
  return true;
}

void PhotoDownloadService::downloaderTaskEntry(void* context) {
  static_cast<PhotoDownloadService*>(context)->runDownloaderTask();
}

void PhotoDownloadService::runDownloaderTask() {
  JpegDownloader::DownloadResult result = {nullptr, 0, ""};
  const bool ok = downloader_.downloadAnyImage(&result);

  if (stateMutex_ != nullptr && xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(1000)) == pdTRUE) {
    if (ok) {
      if (pendingJpgData_ != nullptr) {
        free(pendingJpgData_);
      }
      pendingJpgData_ = result.data;
      pendingJpgLen_ = result.len;
      pendingExifDateTime_ = result.exifDateTime;
      pendingImageReady_ = true;
      Serial.println("PhotoDownloadService: new image ready");
    } else {
      if (result.data != nullptr) {
        free(result.data);
      }
      failureMessage_ = downloader_.lastError();
      if (failureMessage_.isEmpty()) {
        failureMessage_ = "No decodable image";
      }
      failureMessagePending_ = true;
      Serial.printf("PhotoDownloadService: download failed (%s)\n", failureMessage_.c_str());
    }

    downloadInProgress_ = false;
    lastDownloadAttemptMs_ = millis();
    xSemaphoreGive(stateMutex_);
  } else if (result.data != nullptr) {
    free(result.data);
  }

  vTaskDelete(nullptr);
}
