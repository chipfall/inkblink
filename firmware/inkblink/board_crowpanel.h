#pragma once
// ============================================================
//  Board: CrowPanel ESP32 2.13" (DIE01021S)
//  Everything that depends on the hardware is HERE. The main .ino does not know it.
//  For LilyGo, write board_lilygo.h with the same set of functions.
//
//  COORDINATES: all gfx* functions take x counted from the LEFT
//  edge of the screen and y from the top (the EPD driver counts from the right - this
//  layer hides that).
// ============================================================
#include <Arduino.h>
#include "EPD.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"

extern uint8_t ImageBW[ALLSCREEN_BYTES];

#define BOARD_NAME "CrowPanel 2.13"
#define SCREEN_W 250
#define SCREEN_H 122

#define PIN_EPD_POWER 7
#define PIN_KEY_HOME 2
#define PIN_KEY_BACK 1   // EXIT
#define PIN_KEY_UP 6
#define PIN_KEY_DOWN 4
#define PIN_KEY_OK 5

// Which button wakes the device from deep sleep (ext0 = only ONE pin,
// it must be an RTC pin: GPIO0-21 on the ESP32-S3).
#define PIN_WAKE PIN_KEY_OK

// Battery voltage measurement: GPIO connected to a battery voltage divider.
// -1 = unknown / not configured (the battery indicator is then hidden). Board V1.2 has no
// battery measurement; a modified board can be checked with Developer -> Battery pin test.
#ifndef PIN_BAT_ADC
#define PIN_BAT_ADC -1
#endif
#ifndef BAT_DIV_RATIO
#define BAT_DIV_RATIO 2.0f     // battery voltage = pin voltage * BAT_DIV_RATIO
#endif

// 1 = put the screen to sleep after every refresh (leave 0 unless you are sure)
#define USE_EPD_SLEEP 0

enum Key { KEY_NONE, KEY_UP, KEY_DOWN, KEY_OK, KEY_BACK, KEY_HOME };

inline void boardInit() {
  pinMode(PIN_EPD_POWER, OUTPUT);
  digitalWrite(PIN_EPD_POWER, HIGH);

  pinMode(PIN_KEY_HOME, INPUT);
  pinMode(PIN_KEY_BACK, INPUT);
  pinMode(PIN_KEY_UP, INPUT);
  pinMode(PIN_KEY_DOWN, INPUT);
  pinMode(PIN_KEY_OK, INPUT);

  EPD_Init();
}

// Raw button state (LOW = pressed). Used by input.h.
inline bool boardKeyDown(Key k) {
  switch (k) {
    case KEY_UP: return digitalRead(PIN_KEY_UP) == LOW;
    case KEY_DOWN: return digitalRead(PIN_KEY_DOWN) == LOW;
    case KEY_OK: return digitalRead(PIN_KEY_OK) == LOW;
    case KEY_BACK: return digitalRead(PIN_KEY_BACK) == LOW;
    case KEY_HOME: return digitalRead(PIN_KEY_HOME) == LOW;
    default: return false;
  }
}

// Returns the pressed button (with simple debounce) or KEY_NONE.
inline Key boardReadKey() {
  static const struct { uint8_t pin; Key key; } map[] = {
    { PIN_KEY_UP, KEY_UP },
    { PIN_KEY_DOWN, KEY_DOWN },
    { PIN_KEY_OK, KEY_OK },
    { PIN_KEY_BACK, KEY_BACK },
    { PIN_KEY_HOME, KEY_HOME },
  };
  for (const auto& m : map) {
    if (digitalRead(m.pin) == LOW) {
      delay(25);
      if (digitalRead(m.pin) == LOW) {
        unsigned long t = millis();
        while (digitalRead(m.pin) == LOW && millis() - t < 400) delay(5);
        return m.key;
      }
    }
  }
  return KEY_NONE;
}

// Waits (max 1.5 s) until all buttons are released. Used after
// waking up, so that the button being pressed is not read as a command.
inline void boardWaitRelease() {
  unsigned long t = millis();
  while (millis() - t < 1500) {
    if (digitalRead(PIN_KEY_UP) == HIGH && digitalRead(PIN_KEY_DOWN) == HIGH &&
        digitalRead(PIN_KEY_OK) == HIGH && digitalRead(PIN_KEY_BACK) == HIGH &&
        digitalRead(PIN_KEY_HOME) == HIGH) return;
    delay(10);
  }
}

// Battery voltage in mV (averaged) or -1 when the pin is not configured.
// Note: ADC2 pins (GPIO11-20) do not work with WiFi on - measure outside upload mode.
inline int boardBatteryMv() {
#if PIN_BAT_ADC >= 0
  analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);
  long sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogReadMilliVolts(PIN_BAT_ADC);
    delayMicroseconds(300);
  }
  return (int)((sum / 16) * BAT_DIV_RATIO);
#else
  return -1;
#endif
}

// Raw reading of any pin in mV (for finding the battery pin)
inline int boardPinMv(int pin) {
  analogSetPinAttenuation(pin, ADC_11db);
  long sum = 0;
  for (int i = 0; i < 8; i++) {
    sum += analogReadMilliVolts(pin);
    delayMicroseconds(300);
  }
  return (int)(sum / 8);
}

// Deep sleep: the e-paper screen keeps the last image without power.
// Wake-up: button PIN_WAKE (low level). After waking the program starts from scratch.
// Light sleep while idle (ms): the processor stops, memory is kept, woken by any
// button (low level) or when the time is up. Do not use with WiFi on (hotspot).
inline void boardLightSleep(uint32_t ms) {
  static const int pins[5] = { PIN_KEY_HOME, PIN_KEY_BACK, PIN_KEY_UP, PIN_KEY_DOWN, PIN_KEY_OK };
  for (int i = 0; i < 5; i++) gpio_wakeup_enable((gpio_num_t)pins[i], GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
  esp_light_sleep_start();
  // ESP-IDF remembers the wake-up sources. Without disabling them the 250 ms timer
  // would also wake deep sleep (bug in 0.9.7: sleep screen and an immediate restart).
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  for (int i = 0; i < 5; i++) gpio_wakeup_disable((gpio_num_t)pins[i]);
}

inline void boardDeepSleep() {
  EPD_Sleep();                              // screen controller to sleep mode
  digitalWrite(PIN_EPD_POWER, LOW);         // cut the screen power
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);   // only the button wakes (nothing left over from light sleep)
  rtc_gpio_pullup_en((gpio_num_t)PIN_WAKE);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_WAKE);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_WAKE, 0);
  esp_deep_sleep_start();
}

// ---------- Drawing ----------
inline void gfxClear() {
  memset(ImageBW, 0xFF, sizeof(ImageBW));
}

// ------------------------------------------------------------
//  UTF-8 text. The font has only ASCII, so Polish letters are composed:
//  base letter from the font + ogonek / acute / dot drawn with pixels.
//  The geometry is tuned for the 12, 16, 24 and 32 px fonts (the 48 px font
//  has only digits - it is not usable for reading).
//  All characters take one cell (fixed-width font).
// ------------------------------------------------------------
enum { D_NONE, D_ACUTE, D_OGONEK, D_DOT, D_STROKE };

struct DiaGeom {
  uint8_t w, xTop, capTop, base;   // cell width, top of lower-case letters, top of capitals, baseline
  uint8_t ah, t, d, th;            // accent height, stroke thickness, dot size, ogonek (rows below the baseline)
  uint8_t lc, Lc, mid, k;          // stem column of l and L, middle row, half the length of the stroke
};
static const DiaGeom diaGeom[4] = {
  //  w  xTop capTop base  ah t d th  lc Lc mid k
  {  6,   5,    2,    9,   2, 1, 1, 2,  2, 1,  5, 1 },   // 12 px
  {  8,   7,    3,   13,   2, 1, 1, 2,  3, 1,  8, 2 },   // 16 px
  { 12,  10,    5,   20,   3, 2, 2, 3,  6, 1, 12, 3 },   // 24 px
  { 16,  13,    6,   27,   4, 2, 2, 4,  8, 3, 16, 4 },   // 32 px
};

inline int gfxSizeIdx(int size) {
  return size == 12 ? 0 : size == 16 ? 1 : size == 24 ? 2 : size == 32 ? 3 : -1;
}

// Decodes one UTF-8 character. Returns the number of bytes; cp = 0 means "skip".
inline int gfxU8Decode(const char* s, int avail, uint32_t& cp) {
  uint8_t c = (uint8_t)s[0];
  if (c < 0x80) { cp = c; return 1; }
  int need;
  if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; need = 1; }
  else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; need = 2; }
  else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; need = 3; }
  else { cp = 0; return 1; }                       // stray continuation byte
  if (avail - 1 < need) { cp = 0; return avail; }  // cut-off sequence
  for (int k = 1; k <= need; k++) cp = (cp << 6) | ((uint8_t)s[k] & 0x3F);
  return need + 1;
}

// Character code -> ASCII base letter + kind of mark to draw.
inline void gfxMapCp(uint32_t cp, char& base, uint8_t& dia) {
  dia = D_NONE;
  if (cp >= 32 && cp < 127) { base = (char)cp; return; }
  if (cp < 32) { base = ' '; return; }
  switch (cp) {
    case 0x0105: base = 'a'; dia = D_OGONEK; return;  case 0x0104: base = 'A'; dia = D_OGONEK; return;
    case 0x0119: base = 'e'; dia = D_OGONEK; return;  case 0x0118: base = 'E'; dia = D_OGONEK; return;
    case 0x0107: base = 'c'; dia = D_ACUTE;  return;  case 0x0106: base = 'C'; dia = D_ACUTE;  return;
    case 0x0144: base = 'n'; dia = D_ACUTE;  return;  case 0x0143: base = 'N'; dia = D_ACUTE;  return;
    case 0x00F3: base = 'o'; dia = D_ACUTE;  return;  case 0x00D3: base = 'O'; dia = D_ACUTE;  return;
    case 0x015B: base = 's'; dia = D_ACUTE;  return;  case 0x015A: base = 'S'; dia = D_ACUTE;  return;
    case 0x017A: base = 'z'; dia = D_ACUTE;  return;  case 0x0179: base = 'Z'; dia = D_ACUTE;  return;
    case 0x017C: base = 'z'; dia = D_DOT;    return;  case 0x017B: base = 'Z'; dia = D_DOT;    return;
    case 0x0142: base = 'l'; dia = D_STROKE; return;  case 0x0141: base = 'L'; dia = D_STROKE; return;
    case 0x00A0: base = ' '; return;
    case 0x00AD: base = '-'; return;
    case 0x2018: case 0x2019: case 0x201A: base = '\''; return;
    case 0x201C: case 0x201D: case 0x201E: case 0x00AB: case 0x00BB: base = '"'; return;
    case 0x2010: case 0x2011: case 0x2012: case 0x2013: case 0x2014: case 0x2015: base = '-'; return;
    case 0x2026: base = '.'; return;
    case 0x2022: base = '*'; return;
    default: break;
  }
  if (cp >= 0xC0 && cp <= 0xFF) {                  // rest of Latin-1: without accents
    static const char t[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
    base = t[cp - 0xC0];
    return;
  }
  base = '?';
}

// One ink pixel; px counted from the LEFT edge.
inline void gfxInk(int px, int py) {
  if (px < 0 || px >= SCREEN_W || py < 0 || py >= SCREEN_H) return;
#if USE_HORIZONTIAL == 2 || USE_HORIZONTIAL == 3
  EPD_DrawPoint(SCREEN_W - 1 - px, py, WHITE);     // WHITE = "ink on" in this driver
#else
  EPD_DrawPoint(px, py, BLACK);
#endif
}

// ------------------------------------------------------------
//  Key legend: black "labels" at the right edge of the screen, level with
//  the physical keys (MENU at the top, the wheel in the middle, EXIT at the bottom).
// ------------------------------------------------------------
#define TAG_H 14            // label height (px)
#define TAG_COL_W 46        // width of the key legend column at the right edge (px)

// Label at the right edge, y = top edge. White letters (12 px font) on black.
inline void gfxTag(int y, const char* label) {
  int n = (int)strlen(label);
  int w = n * 6 + 6;                       // 6 px per character + 3 px margin on each side
  int x0 = SCREEN_W - w;
  for (int py = y; py < y + TAG_H; py++)
    for (int px = x0; px < SCREEN_W; px++) gfxInk(px, py);
  for (int i = 0; i < n; i++) {
    int px = x0 + 3 + i * 6;
    // EPD_ShowChar with WHITE draws inverted: a white letter on a black background
#if USE_HORIZONTIAL == 2 || USE_HORIZONTIAL == 3
    EPD_ShowChar(SCREEN_W - 1 - px, y + 1, label[i], WHITE, 12);
#else
    EPD_ShowChar(px, y + 1, label[i], WHITE, 12);
#endif
  }
}

// Small triangle (6 px high) showing the wheel's direction of rotation
inline void gfxArrow(int cx, int ytop, bool up) {
  const int h = 6;
  for (int r = 0; r < h; r++) {
    int half = up ? r : (h - 1 - r);
    for (int px = cx - half; px <= cx + half; px++) gfxInk(px, ytop + r);
  }
}

// White pixel (erases ink); px from the LEFT edge.
inline void gfxWhite(int px, int py) {
  if (px < 0 || px >= SCREEN_W || py < 0 || py >= SCREEN_H) return;
#if USE_HORIZONTIAL == 2 || USE_HORIZONTIAL == 3
  EPD_DrawPoint(SCREEN_W - 1 - px, py, BLACK);
#else
  EPD_DrawPoint(px, py, WHITE);
#endif
}

// Does the pixel (px from the LEFT edge, py from the top) have ink? The inverse of gfxInk.
// (Implemented for USE_HORIZONTIAL == 2, as in this driver;
// for other directions it returns false and emboldening does nothing.)
inline bool gfxIsInk(int px, int py) {
#if USE_HORIZONTIAL == 2
  if (px < 0 || px >= SCREEN_W || py < 0 || py >= SCREEN_H) return false;
  int ex = SCREEN_W - 1 - px;                    // same as in gfxInk
  int xpoint = py;                               // same as in EPD_DrawPoint (direction 2)
  int ypoint = EPD_W - ex - 1;
  int addr = xpoint / 8 + ypoint * ((EPD_H % 8 == 0) ? (EPD_H / 8) : (EPD_H / 8 + 1));
  return (ImageBW[addr] & (0x80 >> (xpoint % 8))) == 0;   // ink = cleared bit
#else
  (void)px; (void)py;
  return false;
#endif
}

// Emboldens the ink in a rectangle: every inked pixel gets a neighbour on the right.
// Goes from right to left so the smear does not spread forever.
inline void gfxEmbolden(int x0, int y0, int x1, int y1, int times) {
  for (int t = 0; t < times; t++) {
    for (int py = y0; py < y1; py++) {
      for (int px = x1; px >= x0; px--) {
        if (gfxIsInk(px, py)) gfxInk(px + 1, py);
      }
    }
  }
}

// One character cell; px = left edge of the cell, y = top edge.
inline void gfxDrawCell(int px, int y, uint32_t cp, int size) {
  if (px < 0 || px + size / 2 > SCREEN_W) return;
  char base;
  uint8_t dia;
  gfxMapCp(cp, base, dia);
#if USE_HORIZONTIAL == 2 || USE_HORIZONTIAL == 3
  EPD_ShowChar(SCREEN_W - 1 - px, y, base, BLACK, size);
#else
  EPD_ShowChar(px, y, base, BLACK, size);
#endif
  if (dia == D_NONE) return;
  int gi = gfxSizeIdx(size);
  if (gi < 0) return;
  const DiaGeom& g = diaGeom[gi];
  bool cap = (base >= 'A' && base <= 'Z');

  switch (dia) {
    case D_ACUTE: {                                  // acute stroke above the letter
      int bottom;
      if (cap) {
        bottom = g.capTop - 2;
        if (bottom < g.ah - 1) bottom = g.capTop - 1;
      } else {
        bottom = g.xTop - 2;
      }
      int cx0 = (g.w - (g.ah + g.t - 1)) / 2;
      for (int i = 0; i < g.ah; i++) {
        int row = bottom - (g.ah - 1) + i;
        int col = cx0 + (g.ah - 1 - i);
        for (int q = 0; q < g.t; q++) gfxInk(px + col + q, y + row);
      }
      break;
    }
    case D_DOT: {                                    // dot above the letter
      int bottom = cap ? g.capTop - 2 : g.xTop - 2;
      int top = bottom - g.d + 1;
      if (top < 0) top = 0;
      int col = (g.w - g.d) / 2;
      for (int r = 0; r < g.d; r++)
        for (int c = 0; c < g.d; c++) gfxInk(px + col + c, y + top + r);
      break;
    }
    case D_OGONEK: {                                 // hook under the right end of the letter
      int R = g.w - 1 - g.t;
      for (int r = 1; r <= g.th; r++) {
        int row = g.base + r;
        if (r < g.th) {
          for (int q = 0; q < g.t; q++) gfxInk(px + R - g.t + 1 + q, y + row);
        } else {
          for (int c = R - g.t + 1; c <= R + g.t; c++) gfxInk(px + c, y + row);
        }
      }
      break;
    }
    case D_STROKE: {                                 // stroke through l / L
      int c = cap ? g.Lc : g.lc;
      for (int i = -g.k; i <= g.k; i++)
        for (int q = 0; q < g.t; q++) gfxInk(px + c + i + q, y + g.mid - i);
      break;
    }
  }
}

// Draws 'len' UTF-8 bytes; x from the LEFT edge. Returns the width in pixels.
inline int gfxTextN(int x, int y, const char* s, int len, int size) {
  int w = size / 2;
  int px = x;
  int i = 0;
  while (i < len) {
    uint32_t cp;
    i += gfxU8Decode(s + i, len - i, cp);
    if (cp == 0) continue;
    gfxDrawCell(px, y, cp, size);
    px += w;
  }
  return px - x;
}

inline void gfxText(int x, int y, const char* s, int size) {
  gfxTextN(x, y, s, (int)strlen(s), size);
}

// Width of a UTF-8 string in pixels (fixed-width font)
inline int gfxTextWidth(const char* s, int size) {
  int n = 0, len = (int)strlen(s), i = 0;
  while (i < len) {
    uint32_t cp;
    i += gfxU8Decode(s + i, len - i, cp);
    if (cp != 0) n++;
  }
  return n * (size / 2);
}

// Horizontal line from x0 to x1 (x from the LEFT edge); colour as in the progress bar
inline void gfxHLine(int x0, int x1, int y) {
#if USE_HORIZONTIAL == 2 || USE_HORIZONTIAL == 3
  EPD_DrawLine(SCREEN_W - 1 - x1, y, SCREEN_W - 1 - x0, y, WHITE);
#else
  EPD_DrawLine(x0, y, x1, y, WHITE);
#endif
}

// ---------- Sending the buffer to the screen ----------
// A full refresh (with "flashing") cleans the image. A fast (partial) refresh
// changes only what has changed and does not flash, but after a few of them
// traces remain - then a full one is needed.
static uint8_t imagePrev[ALLSCREEN_BYTES];     // image currently shown on the screen
static bool imagePrevValid = false;

inline bool gfxFastReady() {
  return imagePrevValid;
}

// Screen refresh counters (for the battery estimate)
uint32_t g_presFull = 0, g_presFast = 0;

inline void gfxPresent() {
#if USE_EPD_SLEEP
  EPD_Init();
#endif
  EPD_DisplayImage(ImageBW);
  EPD_Update();
#if USE_EPD_SLEEP
  EPD_Sleep();
#endif
  memcpy(imagePrev, ImageBW, sizeof(imagePrev));
  imagePrevValid = true;
  g_presFull++;
}

// Fast refresh: the driver gets the old and the new image and the DU waveform (see EPD_Init.cpp).
// If the screen shows garbage, turn off "Page flip: Fast" in Settings.
inline void gfxPresentFast() {
  EPD_DisplayImageFast(ImageBW, imagePrev);
  EPD_Update();
  memcpy(imagePrev, ImageBW, sizeof(imagePrev));
  imagePrevValid = true;
  g_presFast++;
}
