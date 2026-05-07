#include "ui/assets/WeatherIcons.h"

#include "ui/assets/WeatherPngAssets.h"

namespace {
enum class IconKind : uint8_t {
  kClearDay,
  kClearNight,
  kPartlyCloudyDay,
  kPartlyCloudyNight,
  kOvercast,
  kFog,
  kDrizzle,
  kRain,
  kSnow,
  kThunderstorms,
  kUnknown,
};

IconKind codeToKind(int weatherCode, bool night) {
  if (weatherCode == 0 || weatherCode == 1) {
    return night ? IconKind::kClearNight : IconKind::kClearDay;
  }
  if (weatherCode == 2) {
    return night ? IconKind::kPartlyCloudyNight : IconKind::kPartlyCloudyDay;
  }
  if (weatherCode == 3) {
    return IconKind::kOvercast;
  }
  if (weatherCode == 45 || weatherCode == 48) {
    return IconKind::kFog;
  }
  if (weatherCode >= 51 && weatherCode <= 57) {
    return IconKind::kDrizzle;
  }
  if ((weatherCode >= 61 && weatherCode <= 67) || (weatherCode >= 80 && weatherCode <= 82)) {
    return IconKind::kRain;
  }
  if ((weatherCode >= 71 && weatherCode <= 77) || weatherCode == 85 || weatherCode == 86) {
    return IconKind::kSnow;
  }
  if (weatherCode >= 95) {
    return IconKind::kThunderstorms;
  }
  return IconKind::kUnknown;
}

const lv_img_dsc_t* iconForSize(IconKind kind, bool large) {
  if (large) {
    switch (kind) {
      case IconKind::kClearDay:
        return &gWeatherIcon88ClearDay;
      case IconKind::kClearNight:
        return &gWeatherIcon88ClearNight;
      case IconKind::kPartlyCloudyDay:
        return &gWeatherIcon88PartlyCloudyDay;
      case IconKind::kPartlyCloudyNight:
        return &gWeatherIcon88PartlyCloudyNight;
      case IconKind::kOvercast:
        return &gWeatherIcon88Overcast;
      case IconKind::kFog:
        return &gWeatherIcon88Fog;
      case IconKind::kDrizzle:
        return &gWeatherIcon88Drizzle;
      case IconKind::kRain:
        return &gWeatherIcon88Rain;
      case IconKind::kSnow:
        return &gWeatherIcon88Snow;
      case IconKind::kThunderstorms:
        return &gWeatherIcon88Thunderstorms;
      case IconKind::kUnknown:
      default:
        return &gWeatherIcon88Unknown;
    }
  }

  switch (kind) {
    case IconKind::kClearDay:
      return &gWeatherIcon48ClearDay;
    case IconKind::kClearNight:
      return &gWeatherIcon48ClearNight;
    case IconKind::kPartlyCloudyDay:
      return &gWeatherIcon48PartlyCloudyDay;
    case IconKind::kPartlyCloudyNight:
      return &gWeatherIcon48PartlyCloudyNight;
    case IconKind::kOvercast:
      return &gWeatherIcon48Overcast;
    case IconKind::kFog:
      return &gWeatherIcon48Fog;
    case IconKind::kDrizzle:
      return &gWeatherIcon48Drizzle;
    case IconKind::kRain:
      return &gWeatherIcon48Rain;
    case IconKind::kSnow:
      return &gWeatherIcon48Snow;
    case IconKind::kThunderstorms:
      return &gWeatherIcon48Thunderstorms;
    case IconKind::kUnknown:
    default:
      return &gWeatherIcon48Unknown;
  }
}
}  // namespace

const lv_img_dsc_t* weatherIconForCode(int weatherCode, bool night, bool large) {
  return iconForSize(codeToKind(weatherCode, night), large);
}
