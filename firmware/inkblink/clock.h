#pragma once
// ============================================================
//  Clock: time set from the phone (web page in Upload mode), time zones with
//  automatic daylight saving time (POSIX TZ format). The time survives sleep, but not
//  a complete loss of power - then simply open the web page again.
// ============================================================
#include <Arduino.h>
#include <time.h>
#include <sys/time.h>

struct TzInfo {
  const char* name;
  const char* posix;
};
static const TzInfo TZS[] = {
  { "Central Europe", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "UK / Portugal", "GMT0BST,M3.5.0/1,M10.5.0" },
  { "Eastern Europe", "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { "Moscow / Turkey", "MSK-3" },
  { "UTC", "UTC0" },
  { "US Eastern", "EST5EDT,M3.2.0,M11.1.0" },
  { "US Central", "CST6CDT,M3.2.0,M11.1.0" },
  { "US Mountain", "MST7MDT,M3.2.0,M11.1.0" },
  { "US Pacific", "PST8PDT,M3.2.0,M11.1.0" },
  { "Dubai", "GST-4" },
  { "India", "IST-5:30" },
  { "China / Singapore", "CST-8" },
  { "Japan / Korea", "JST-9" },
  { "Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3" },
  { "New Zealand", "NZST-12NZDT,M9.5.0,M4.1.0/3" }
};
#define TZ_COUNT 15

static time_t (*clockNowHook)() = nullptr;   // for tests on a PC
static void (*clockSetHook)(time_t) = nullptr;

inline time_t clockNow() {
  return clockNowHook ? clockNowHook() : time(nullptr);
}
inline bool clockValid() {
  return clockNow() > 1700000000L;           // after November 2023 = time is set
}
inline void clockApplyTz(int idx) {
  if (idx < 0 || idx >= TZ_COUNT) idx = 0;
  setenv("TZ", TZS[idx].posix, 1);
  tzset();
}
inline bool clockSet(time_t t) {
  if (clockSetHook) {                        // tests on a PC
    clockSetHook(t);
    return true;
  }
  struct timeval tv;
  tv.tv_sec = t;
  tv.tv_usec = 0;
  return settimeofday(&tv, nullptr) == 0;
}
inline int clockDaysInMonth(int y, int m) {
  static const uint8_t dm[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
  return dm[(m - 1) % 12];
}
// Sets the time from fields in the LOCAL TIME of the chosen zone (the zone must already be set by clockApplyTz)
inline bool clockSetLocal(int y, int mo, int d, int h, int mi) {
  struct tm tmv;
  memset(&tmv, 0, sizeof(tmv));
  tmv.tm_year = y - 1900;
  tmv.tm_mon = mo - 1;
  tmv.tm_mday = d;
  tmv.tm_hour = h;
  tmv.tm_min = mi;
  tmv.tm_isdst = -1;                         // work out daylight saving time by itself
  time_t t = mktime(&tmv);
  if (t < 1700000000L) return false;
  return clockSet(t);
}
// mode: 1 = 24h ("14:35"), 2 = 12h ("2:35pm"). Returns false when the time is not set.
inline bool clockFormat(char* out, size_t n, int mode) {
  if (!clockValid()) {
    if (n) out[0] = 0;
    return false;
  }
  time_t t = clockNow();
  struct tm tmv;
  localtime_r(&t, &tmv);
  if (mode == 2) {
    int h = tmv.tm_hour % 12;
    if (h == 0) h = 12;
    snprintf(out, n, "%d:%02d%s", h, tmv.tm_min, tmv.tm_hour < 12 ? "am" : "pm");
  } else {
    snprintf(out, n, "%02d:%02d", tmv.tm_hour, tmv.tm_min);
  }
  return true;
}
// Local date "YYYY-MM-DD"; false when the time is not set.
inline bool clockFormatDate(char* out, size_t n) {
  if (!clockValid()) {
    if (n) out[0] = 0;
    return false;
  }
  time_t t = clockNow();
  struct tm tmv;
  localtime_r(&t, &tmv);
  snprintf(out, n, "%04d-%02d-%02d", tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
  return true;
}
