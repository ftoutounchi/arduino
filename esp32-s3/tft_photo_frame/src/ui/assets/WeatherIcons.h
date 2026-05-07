#pragma once

#include <lvgl.h>

// Returns a weather icon descriptor for the given Open-Meteo weather code.
// `large=true` uses 88x88 (main icon), otherwise 48x48 (forecast icon).
const lv_img_dsc_t* weatherIconForCode(int weatherCode, bool night, bool large);
