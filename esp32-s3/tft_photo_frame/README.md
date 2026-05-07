# ESP32-S3 Zero TFT Photo Frame (240x280)

This project downloads internet JPEG images and displays them on a 1.69 inch 240x280 ST7789 display.

## Photo Formatting Tool (`tools/format_photos.py`)

Use this script to convert local images into ESP32/TJpg_Decoder-friendly JPEG files.

- Converts to baseline JPEG (non-progressive), RGB
- Resizes to fit the display (default max: `240x280`)
- Applies EXIF auto-rotation so photos keep the correct orientation
- Writes results to `<input_dir>/formatted` by default

Install dependency:

```sh
python3 -m pip install Pillow
```

Basic usage:

```sh
python3 tools/format_photos.py /path/to/photos
```

Common examples:

```sh
# Custom output folder
python3 tools/format_photos.py /path/to/photos --output-dir /path/to/photos/formatted

# Overwrite existing output files
python3 tools/format_photos.py /path/to/photos --overwrite

# Custom output size and quality
python3 tools/format_photos.py /path/to/photos --max-width 240 --max-height 280 --quality 88

# Sequential output names: photo_0001.jpg, photo_0002.jpg, ...
python3 tools/format_photos.py /path/to/photos --name-pattern "photo_####.jpg" --start-id 1
```

## Page Architecture (OOP)

The app uses a modular page system where each page owns its lifecycle, event handling, and local behavior:
- `PhotoFramePage`
- `DashboardPage`
- `CalendarPage`
- `AgendaPage`
- `AlarmPage`
- `SettingsPage`

Core control is centralized in `PageManager`:
- register pages
- switch/push/pop pages
- keep active page state
- forward button events to the active page
- call lifecycle and frame methods

All pages implement the same interface:

```cpp
class IPage {
 public:
  virtual ~IPage() = default;
  virtual void onEnter() = 0;
  virtual void onExit() = 0;
  virtual void update(uint32_t nowMs) = 0;
  virtual void render() = 0;
  virtual void handleEvent(const PageEvent& event) = 0;
};
```

`PhotoFrameApp` now acts as an orchestrator only:
- boot and service initialization
- input polling (`BootButtonInput`)
- runtime config reload
- calling `PageManager::update()` and `PageManager::render()`

LVGL responsibilities are split as well:
- `LvglHost`: LVGL init, display flush bridge, and LVGL timer servicing
- `DashboardView`, `CalendarView`, `AgendaView`, `AlarmView`, `SettingsView`: each page owns its own UI tree, state, and page-specific logic

Data fetching is now layered to keep views clean and replaceable:
- `app/ports/*`: interfaces used by app layer (`IAgendaProvider`, `IWeatherProvider`)
- `infra/network/*Provider`: concrete HTTP/JSON adapters (`GoogleScriptAgendaProvider`, `OpenMeteoWeatherProvider`)
- `app/services/*Service`: app-level formatting/state prep (`AgendaService`, `WeatherService`)
- `ui/views/*`: render-only logic consuming services, with no direct HTTP/JSON parsing

Dependency direction:
- `ui -> app/services -> app/ports <- infra`
- pages and views do not talk directly to each other

All views follow one shared contract (`IView`):

```cpp
class IView {
 public:
  virtual ~IView() = default;
  virtual void onEnter() = 0;
  virtual void onExit() = 0;
  virtual void update(uint32_t nowMs) = 0;
  virtual void render() = 0;
  virtual ViewAction handleEvent(const PageEvent& event) = 0;
};
```

## Pins

TFT pins:
- `TFT_SCLK = 7`
- `TFT_MOSI = 8`
- `TFT_RST  = 9`
- `TFT_DC   = 10`
- `TFT_CS   = 11`
- `TFT_BL   = 6`

## App Settings (`src/config/AppConfig.h`)

Main runtime behavior is configured in:
- `src/config/AppConfig.h`

### Time/Weather auto page options

These options control automatic switching between photo frame and time/weather page:

- `kInfoPageAutoTimeoutEnabled`
: Enable/disable auto-return when user opens time/weather page manually.

- `kInfoPageAutoTimeoutMs`
: How long to stay on time/weather page after manual TOUCH switch.

- `kInfoPageAutoCycleEnabled`
: Enable/disable automatic cycle from photo frame to time/weather page.

- `kInfoPageAutoCyclePhotoCount`
: After showing this many photos, switch to time/weather page automatically.

- `kInfoPageAutoCycleDurationMs`
: How long to keep time/weather page open during auto cycle.

Default values:

```cpp
constexpr bool kInfoPageAutoTimeoutEnabled = true;
constexpr uint32_t kInfoPageAutoTimeoutMs = 30 * 1000;
constexpr bool kInfoPageAutoCycleEnabled = true;
constexpr uint16_t kInfoPageAutoCyclePhotoCount = 2;
constexpr uint32_t kInfoPageAutoCycleDurationMs = 30 * 1000;
```

Example presets:

```cpp
// 1) Manual only (no auto switching)
constexpr bool kInfoPageAutoTimeoutEnabled = false;
constexpr bool kInfoPageAutoCycleEnabled = false;

// 2) Auto cycle every 5 photos, show dashboard for 20 seconds
constexpr bool kInfoPageAutoCycleEnabled = true;
constexpr uint16_t kInfoPageAutoCyclePhotoCount = 5;
constexpr uint32_t kInfoPageAutoCycleDurationMs = 20 * 1000;

// 3) Manual TOUCH switch returns to photos after 15 seconds
constexpr bool kInfoPageAutoTimeoutEnabled = true;
constexpr uint32_t kInfoPageAutoTimeoutMs = 15 * 1000;
```

### On-device settings page (LVGL)

You can open Settings by holding the TOUCH button for about 1.2 seconds.

Settings page is organized into tabs:
- `AUTO`
- `PHOTO`
- `WEATHER`
- `SYSTEM`

Controls in Settings page:
- Short TOUCH press: select next row
- Long TOUCH press on normal row: change selected value
- Long TOUCH press on `Tab: ...`: switch to next tab
- In `SYSTEM` tab, select `Exit to Photo Frame` and long-press TOUCH to return

Settings page edits these runtime values:
- Timeout enabled
- Timeout seconds
- Cycle enabled
- Cycle photo count
- Cycle duration seconds
- Weather location label
- Weather latitude / longitude

Settings are now persisted in LittleFS:
- `/settings.json`

They are:
- loaded at boot
- saved automatically when you change a value in Settings page

Default file is at:
- `fsdata/settings.json`

If you edit `fsdata/settings.json` manually, upload filesystem:

```sh
cd /home/farzad/dev/Arduino/esp32-s3/tft_photo_frame
~/.platformio/penv/bin/pio run -t uploadfs
```

## URL JSON Config

Edit URL list in:
- `fsdata/photo_urls.json`

Format:

```json
{
  "photo_urls": [
    "https://example.com/photo1.jpg",
    "https://example.com/photo2.jpg"
  ]
}
```

After changing the JSON, upload filesystem and firmware:

```sh
cd /home/farzad/dev/Arduino/esp32-s3/tft_photo_frame
~/.platformio/penv/bin/pio run -t uploadfs
~/.platformio/penv/bin/pio run -t upload
```

The sketch tries a random `picsum` image first, then uses URLs from JSON.

## Private GitHub Repo Photos

For private repos, use GitHub API URLs (not `github.com/.../blob/...` links):

```json
{
  "photo_urls": [
    "https://api.github.com/repos/ftoutounchi/myAlbum/contents/photos/photo1.jpg?ref=main"
  ]
}
```

Set your token in:
- `fsdata/github_auth.json`

```json
{
  "github_token": "PUT_YOUR_FINE_GRAINED_TOKEN_HERE",
  "user_agent": "esp32-photo-frame"
}
```

Token should be a fine-grained PAT with `Contents: Read` for that repository.
