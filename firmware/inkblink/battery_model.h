#pragma once
// ============================================================
//  Battery estimate WITHOUT a sensor (CrowPanel V1.2 cannot measure the battery voltage).
//
//  The firmware counts how long it spent in each state (awake, hotspot, sleep) and how many screen
//  refreshes there were, multiplies that by the measured current and subtracts it from the capacity. The
//  starting point is the battery voltage measured with a multimeter (Li-Po curve) or "battery full".
//  It is an ESTIMATE - errors add up, so every setting is written to a log (battlog.csv)
//  together with the counters, so that the constants can be fitted later.
//
//  Constants measured on the board at 3.7 V; the current hardly depends on the voltage (3.4-4.2 V), so
//  we count in mAh.
// ============================================================
#include <stdint.h>

#define BM_AWAKE_MA_OFF 34.5f     // awake, static screen, processor 240 MHz (measured)
#define BM_AWAKE_MA_CPU80 24.0f   // awake, processor 80 MHz (measured, 0.9.7)
#define BM_AWAKE_MA_LIGHT 1.0f    // light sleep: the power supply showed 0 mA (<1 mA), value with headroom
#define BM_HOTSPOT_MA 108.0f      // hotspot on (measured)
#define BM_FLIP_FAST_MAH 0.0040f  // extra charge of one fast screen refresh (measured)
#define BM_FLIP_FULL_MAH 0.0128f  // ... of a full refresh (measured)
#define BM_SLEEP_MA 0.33f         // deep sleep: measured from a 4-day voltage drop (about 0.30-0.35 mA)

static const uint16_t BM_CAPS[6] = { 800, 1000, 1200, 1500, 2000, 3000 };   // available capacities (mAh)

// Resting voltage of a Li-Po cell (no load) -> percent. Approximate, the middle of the curve is flat.
static const float BM_V[21] = { 3.27f, 3.61f, 3.69f, 3.71f, 3.73f, 3.75f, 3.77f, 3.79f, 3.80f, 3.82f, 3.84f,
                                3.85f, 3.87f, 3.91f, 3.95f, 3.98f, 4.02f, 4.08f, 4.11f, 4.15f, 4.20f };
static const float BM_PCT[21] = { 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100 };

inline float bmPctFromVolt(float v) {
  if (v <= BM_V[0]) return 0;
  if (v >= BM_V[20]) return 100;
  for (int i = 1; i < 21; i++) {
    if (v <= BM_V[i]) return BM_PCT[i - 1] + (v - BM_V[i - 1]) * (BM_PCT[i] - BM_PCT[i - 1]) / (BM_V[i] - BM_V[i - 1]);
  }
  return 100;
}
inline float bmVoltFromPct(float p) {
  if (p <= 0) return BM_V[0];
  if (p >= 100) return BM_V[20];
  for (int i = 1; i < 21; i++) {
    if (p <= BM_PCT[i]) return BM_V[i - 1] + (p - BM_PCT[i - 1]) * (BM_V[i] - BM_V[i - 1]) / (BM_PCT[i] - BM_PCT[i - 1]);
  }
  return BM_V[20];
}

struct BatModel {
  bool valid;            // is the starting point known (entered voltage or "full")
  float capMah;          // cell capacity
  float remMah;          // estimated remaining mAh
  // counters since the last setting (for the log)
  float awakeS, hotspotS, sleepS;
  uint32_t flipsFast, flipsFull;
};

inline void bmReset(BatModel& m, float capMah) {
  m.valid = false;
  m.capMah = capMah;
  m.remMah = 0;
  m.awakeS = m.hotspotS = m.sleepS = 0;
  m.flipsFast = m.flipsFull = 0;
}
inline float bmPercent(const BatModel& m) {
  if (!m.valid || m.capMah <= 0) return 0;
  float p = 100.0f * m.remMah / m.capMah;
  return p < 0 ? 0 : p > 100 ? 100 : p;
}
inline void bmSpendMah(BatModel& m, float mah) {
  m.remMah -= mah;
  if (m.remMah < 0) m.remMah = 0;
}
inline float bmAwakeMa(int powerSave) {
  return powerSave == 1 ? BM_AWAKE_MA_CPU80 : powerSave == 2 ? BM_AWAKE_MA_LIGHT : BM_AWAKE_MA_OFF;
}
// Awake time: hotspot (if on) or awake with the current power-saving mode
inline void bmAddRun(BatModel& m, bool hotspot, int powerSave, float seconds) {
  if (hotspot) {
    m.hotspotS += seconds;
    bmSpendMah(m, BM_HOTSPOT_MA * seconds / 3600.0f);
  } else {
    m.awakeS += seconds;
    bmSpendMah(m, bmAwakeMa(powerSave) * seconds / 3600.0f);
  }
}
inline void bmAddSleep(BatModel& m, float seconds) {
  m.sleepS += seconds;
  bmSpendMah(m, BM_SLEEP_MA * seconds / 3600.0f);
}
inline void bmAddFlips(BatModel& m, uint32_t full, uint32_t fast) {
  m.flipsFull += full;
  m.flipsFast += fast;
  bmSpendMah(m, full * BM_FLIP_FULL_MAH + fast * BM_FLIP_FAST_MAH);
}
inline void bmSetPercent(BatModel& m, float pct) {
  m.valid = true;
  m.remMah = m.capMah * pct / 100.0f;
  m.awakeS = m.hotspotS = m.sleepS = 0;
  m.flipsFast = m.flipsFull = 0;
}
