#pragma once
// ============================================================
//  Zegar: czas ustawiany z telefonu (strona WWW w trybie Upload), strefy czasowe z
//  automatycznym czasem letnim (format POSIX TZ). Czas przezywa uspienie, ale nie
//  calkowite odlaczenie zasilania - wtedy wystarczy znow otworzyc strone WWW.
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

static time_t (*clockNowHook)() = nullptr;   // do testow na komputerze

inline time_t clockNow() {
  return clockNowHook ? clockNowHook() : time(nullptr);
}
inline bool clockValid() {
  return clockNow() > 1700000000L;           // po listopadzie 2023 = czas ustawiony
}
inline void clockApplyTz(int idx) {
  if (idx < 0 || idx >= TZ_COUNT) idx = 0;
  setenv("TZ", TZS[idx].posix, 1);
  tzset();
}
inline bool clockSet(time_t t) {
  struct timeval tv;
  tv.tv_sec = t;
  tv.tv_usec = 0;
  return settimeofday(&tv, nullptr) == 0;
}
// mode: 1 = 24h ("14:35"), 2 = 12h ("2:35pm"). Zwraca false, gdy czas nie jest ustawiony.
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
