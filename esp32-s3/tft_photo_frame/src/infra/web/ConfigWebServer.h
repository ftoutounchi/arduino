#pragma once

#include <WebServer.h>

class ConfigWebServer {
 public:
  ConfigWebServer();

  void begin();
  void loop();

  void setConfigChangedCallback(void (*cb)(void*), void* ctx);

 private:
  WebServer server_;
  bool started_;
  void (*configChangedCb_)(void*);
  void* configChangedCtx_;

  static bool ensureFsMounted();
  static bool readFile(const char* path, String* out);
  static bool writeFileAtomic(const char* path, const String& content);

  void notifyConfigChanged() const;
  void setupRoutes();

  void handleRoot();
  void handleGetConfig();
  void handleGetPhotoUrls();
  void handlePostSettings();
  void handlePostPhotoUrls();
  void handlePostGithubAuth();
};
