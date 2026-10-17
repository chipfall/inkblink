#pragma once
// ============================================================
//  Plytka: CrowPanel ESP32 2.13" (DIE01021S)
//  Wszystko, co zalezy od sprzetu, jest TUTAJ. main.ino go nie zna.
//  Pod LilyGo zrobisz board_lilygo.h z tym samym zestawem funkcji.
//
//  WSPOLRZEDNE: wszystkie funkcje gfx* przyjmuja x liczone od LEWEJ
//  krawedzi ekranu i y od gory (sterownik EPD liczy od prawej - ta
//  warstwa to ukrywa).
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

// Ktory przycisk budzi urzadzenie z gleboki uspienia (ext0 = tylko JEDEN pin,
// musi byc pinem RTC: GPIO0-21 na ESP32-S3).
#define PIN_WAKE PIN_KEY_OK

// Pomiar napiecia baterii: GPIO podlaczone do dzielnika napiecia baterii.
// -1 = nieznany / nieskonfigurowany (wskaznik baterii jest wtedy ukryty).
// Pin znajdziesz w menu Info -> PINS (test pinow) albo na schemacie plytki.
#ifndef PIN_BAT_ADC
#define PIN_BAT_ADC -1
#endif
#ifndef BAT_DIV_RATIO
#define BAT_DIV_RATIO 2.0f     // napiecie baterii = napiecie na pinie * BAT_DIV_RATIO
#endif

// 1 = usypiaj ekran po kazdym odswiezeniu (zostaw 0, dopoki nie masz pewnosci)
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

// Surowy stan przycisku (LOW = wcisniety). Uzywa tego input.h.
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

// Zwraca wcisniety przycisk (z prostym debounce) albo KEY_NONE.
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

// Czeka (max 1,5 s), az wszystkie przyciski beda puszczone. Uzywane po
// wybudzeniu, zeby wciskany przycisk nie zostal odczytany jako komenda.
inline void boardWaitRelease() {
  unsigned long t = millis();
  while (millis() - t < 1500) {
    if (digitalRead(PIN_KEY_UP) == HIGH && digitalRead(PIN_KEY_DOWN) == HIGH &&
        digitalRead(PIN_KEY_OK) == HIGH && digitalRead(PIN_KEY_BACK) == HIGH &&
        digitalRead(PIN_KEY_HOME) == HIGH) return;
    delay(10);
  }
}

// Napiecie baterii w mV (usrednione) albo -1, gdy pin nie jest skonfigurowany.
// Uwaga: piny ADC2 (GPIO11-20) nie dzialaja przy wlaczonym WiFi - mierz poza trybem upload.
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

// Surowy odczyt dowolnego pinu w mV (do szukania pinu baterii)
inline int boardPinMv(int pin) {
  analogSetPinAttenuation(pin, ADC_11db);
  long sum = 0;
  for (int i = 0; i < 8; i++) {
    sum += analogReadMilliVolts(pin);
    delayMicroseconds(300);
  }
  return (int)(sum / 8);
}

// Gleboki sen: ekran e-papierowy zachowuje ostatni obraz bez zasilania.
// Budzik: przycisk PIN_WAKE (stan niski). Po wybudzeniu program startuje od nowa.
// Lekkie uspienie na czas bezczynnosci (ms): procesor stoi, pamiec zostaje, wybudzenie dowolnym
// przyciskiem (stan niski) albo po uplywie czasu. Nie uzywac przy wlaczonym WiFi (hotspot).
inline void boardLightSleep(uint32_t ms) {
  static const int pins[5] = { PIN_KEY_HOME, PIN_KEY_BACK, PIN_KEY_UP, PIN_KEY_DOWN, PIN_KEY_OK };
  for (int i = 0; i < 5; i++) gpio_wakeup_enable((gpio_num_t)pins[i], GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
  esp_light_sleep_start();
  // Zrodla wybudzenia zostaja zapamietane w ESP-IDF. Bez wylaczenia budzik 250 ms
  // obudzilby tez gleboki sen (blad z 0.9.7: ekran uspienia i od razu restart).
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  for (int i = 0; i < 5; i++) gpio_wakeup_disable((gpio_num_t)pins[i]);
}

inline void boardDeepSleep() {
  EPD_Sleep();                              // kontroler ekranu w tryb uspienia
  digitalWrite(PIN_EPD_POWER, LOW);         // odetnij zasilanie ekranu
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);   // tylko przycisk budzi (zadnych resztek po lekkim uspieniu)
  rtc_gpio_pullup_en((gpio_num_t)PIN_WAKE);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_WAKE);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_WAKE, 0);
  esp_deep_sleep_start();
}

// ---------- Rysowanie ----------
inline void gfxClear() {
  memset(ImageBW, 0xFF, sizeof(ImageBW));
}

// ------------------------------------------------------------
//  Tekst UTF-8. Font ma tylko ASCII, wiec polskie litery sa skladane:
//  litera bazowa z fontu + ogonek / kreska / kropka dorysowane pikselami.
//  Geometria jest dobrana do fontow 12, 16, 24 i 32 px (font 48 px
//  zawiera tylko cyfry - nie nadaje sie do czytania).
//  Wszystkie znaki zajmuja jedna komorke (font stalej szerokosci).
// ------------------------------------------------------------
enum { D_NONE, D_ACUTE, D_OGONEK, D_DOT, D_STROKE };

struct DiaGeom {
  uint8_t w, xTop, capTop, base;   // szerokosc komorki, gora malych liter, gora duzych, linia bazowa
  uint8_t ah, t, d, th;            // wysokosc akcentu, grubosc kreski, wielkosc kropki, ogonek (wiersze pod linia)
  uint8_t lc, Lc, mid, k;          // kolumna trzonu l i L, wiersz srodka, polowa dlugosci przekreslenia
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

// Dekoduje jeden znak UTF-8. Zwraca liczbe bajtow; cp = 0 oznacza "pomin".
inline int gfxU8Decode(const char* s, int avail, uint32_t& cp) {
  uint8_t c = (uint8_t)s[0];
  if (c < 0x80) { cp = c; return 1; }
  int need;
  if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; need = 1; }
  else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; need = 2; }
  else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; need = 3; }
  else { cp = 0; return 1; }                       // zagubiony bajt kontynuacji
  if (avail - 1 < need) { cp = 0; return avail; }  // ucieta sekwencja
  for (int k = 1; k <= need; k++) cp = (cp << 6) | ((uint8_t)s[k] & 0x3F);
  return need + 1;
}

// Kod znaku -> litera bazowa ASCII + rodzaj dorysowywanego znaczka.
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
  if (cp >= 0xC0 && cp <= 0xFF) {                  // reszta Latin-1: bez akcentow
    static const char t[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
    base = t[cp - 0xC0];
    return;
  }
  base = '?';
}

// Jeden piksel tuszu; px liczone od LEWEJ krawedzi.
inline void gfxInk(int px, int py) {
  if (px < 0 || px >= SCREEN_W || py < 0 || py >= SCREEN_H) return;
#if USE_HORIZONTIAL == 2 || USE_HORIZONTIAL == 3
  EPD_DrawPoint(SCREEN_W - 1 - px, py, WHITE);     // WHITE = "zapal tusz" w tym sterowniku
#else
  EPD_DrawPoint(px, py, BLACK);
#endif
}

// ------------------------------------------------------------
//  Legenda klawiszy: czarne "etykiety" przy prawej krawedzi ekranu, na
//  wysokosci fizycznych klawiszy (MENU u gory, pokretlo w srodku, EXIT na dole).
// ------------------------------------------------------------
#define TAG_H 14            // wysokosc etykiety (px)
#define TAG_COL_W 46        // szerokosc kolumny legendy przy prawej krawedzi (px)

// Etykieta przy prawej krawedzi, y = gorna krawedz. Biale litery (font 12 px) na czarnym.
inline void gfxTag(int y, const char* label) {
  int n = (int)strlen(label);
  int w = n * 6 + 6;                       // 6 px na znak + po 3 px marginesu
  int x0 = SCREEN_W - w;
  for (int py = y; py < y + TAG_H; py++)
    for (int px = x0; px < SCREEN_W; px++) gfxInk(px, py);
  for (int i = 0; i < n; i++) {
    int px = x0 + 3 + i * 6;
    // EPD_ShowChar z kolorem WHITE rysuje odwrocone: biala litera na czarnym tle
#if USE_HORIZONTIAL == 2 || USE_HORIZONTIAL == 3
    EPD_ShowChar(SCREEN_W - 1 - px, y + 1, label[i], WHITE, 12);
#else
    EPD_ShowChar(px, y + 1, label[i], WHITE, 12);
#endif
  }
}

// Maly trojkat (6 px wysokosci) wskazujacy kierunek obrotu pokretla
inline void gfxArrow(int cx, int ytop, bool up) {
  const int h = 6;
  for (int r = 0; r < h; r++) {
    int half = up ? r : (h - 1 - r);
    for (int px = cx - half; px <= cx + half; px++) gfxInk(px, ytop + r);
  }
}

// Piksel bialy (kasowanie tuszu); px od LEWEJ krawedzi.
inline void gfxWhite(int px, int py) {
  if (px < 0 || px >= SCREEN_W || py < 0 || py >= SCREEN_H) return;
#if USE_HORIZONTIAL == 2 || USE_HORIZONTIAL == 3
  EPD_DrawPoint(SCREEN_W - 1 - px, py, BLACK);
#else
  EPD_DrawPoint(px, py, WHITE);
#endif
}

// Czy piksel (px od LEWEJ krawedzi, py od gory) ma tusz? Odwrotnosc gfxInk.
// (Zaimplementowane dla USE_HORIZONTIAL == 2, jak w Twoim sterowniku;
// dla innych kierunkow zwraca false i pogrubianie nic nie robi.)
inline bool gfxIsInk(int px, int py) {
#if USE_HORIZONTIAL == 2
  if (px < 0 || px >= SCREEN_W || py < 0 || py >= SCREEN_H) return false;
  int ex = SCREEN_W - 1 - px;                    // tak jak w gfxInk
  int xpoint = py;                               // tak jak w EPD_DrawPoint (kierunek 2)
  int ypoint = EPD_W - ex - 1;
  int addr = xpoint / 8 + ypoint * ((EPD_H % 8 == 0) ? (EPD_H / 8) : (EPD_H / 8 + 1));
  return (ImageBW[addr] & (0x80 >> (xpoint % 8))) == 0;   // tusz = wyzerowany bit
#else
  (void)px; (void)py;
  return false;
#endif
}

// Pogrubia tusz w prostokacie: kazdy zapalony piksel dostaje sasiada z prawej.
// Idzie od prawej do lewej, zeby nie rozmazywac w nieskonczonosc.
inline void gfxEmbolden(int x0, int y0, int x1, int y1, int times) {
  for (int t = 0; t < times; t++) {
    for (int py = y0; py < y1; py++) {
      for (int px = x1; px >= x0; px--) {
        if (gfxIsInk(px, py)) gfxInk(px + 1, py);
      }
    }
  }
}

// Jedna komorka znaku; px = lewa krawedz komorki, y = gorna.
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
    case D_ACUTE: {                                  // ukosna kreska nad litera
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
    case D_DOT: {                                    // kropka nad litera
      int bottom = cap ? g.capTop - 2 : g.xTop - 2;
      int top = bottom - g.d + 1;
      if (top < 0) top = 0;
      int col = (g.w - g.d) / 2;
      for (int r = 0; r < g.d; r++)
        for (int c = 0; c < g.d; c++) gfxInk(px + col + c, y + top + r);
      break;
    }
    case D_OGONEK: {                                 // haczyk pod prawym koncem litery
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
    case D_STROKE: {                                 // przekreslenie l / L
      int c = cap ? g.Lc : g.lc;
      for (int i = -g.k; i <= g.k; i++)
        for (int q = 0; q < g.t; q++) gfxInk(px + c + i + q, y + g.mid - i);
      break;
    }
  }
}

// Rysuje 'len' bajtow UTF-8; x od LEWEJ krawedzi. Zwraca szerokosc w pikselach.
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

// Szerokosc napisu UTF-8 w pikselach (czcionka stalej szerokosci)
inline int gfxTextWidth(const char* s, int size) {
  int n = 0, len = (int)strlen(s), i = 0;
  while (i < len) {
    uint32_t cp;
    i += gfxU8Decode(s + i, len - i, cp);
    if (cp != 0) n++;
  }
  return n * (size / 2);
}

// Pozioma linia od x0 do x1 (x od LEWEJ krawedzi); kolor jak w pasku postepu
inline void gfxHLine(int x0, int x1, int y) {
#if USE_HORIZONTIAL == 2 || USE_HORIZONTIAL == 3
  EPD_DrawLine(SCREEN_W - 1 - x1, y, SCREEN_W - 1 - x0, y, WHITE);
#else
  EPD_DrawLine(x0, y, x1, y, WHITE);
#endif
}

// ---------- Wyslanie bufora na ekran ----------
// Pelne odswiezenie (z "miganiem") czysci obraz. Szybkie (czesciowe) odswiezenie
// zmienia tylko to, co sie zmienilo, i migania nie ma, ale po kilku takich odswiezeniach
// zostaja slady - wtedy trzeba zrobic pelne.
static uint8_t imagePrev[ALLSCREEN_BYTES];     // obraz aktualnie widoczny na ekranie
static bool imagePrevValid = false;

inline bool gfxFastReady() {
  return imagePrevValid;
}

// Liczniki odswiezen ekranu (do szacowania zuzycia baterii)
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

// Szybkie odswiezenie: sterownik dostaje stary i nowy obraz oraz przebieg DU (patrz EPD_Init.cpp).
// EKSPERYMENTALNE - jesli ekran pokazuje smieci, wylacz "Page flip: Fast" w Ustawieniach.
inline void gfxPresentFast() {
  EPD_DisplayImageFast(ImageBW, imagePrev);
  EPD_Update();
  memcpy(imagePrev, ImageBW, sizeof(imagePrev));
  imagePrevValid = true;
  g_presFast++;
}
