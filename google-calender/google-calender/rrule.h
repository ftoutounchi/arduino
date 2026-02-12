#pragma once

#include <Arduino.h>
#include <time.h>

struct RRule {
  String freq;
  int interval = 1;
  bool hasByday = false;
  bool byday[7] = {false,false,false,false,false,false,false}; // 0=Sun..6=Sat
  bool hasBymonthday = false;
  int bymonthday = 0;
  time_t until = 0;
  int count = 0;
};
