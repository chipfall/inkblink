#pragma once
// ============================================================
//  Czcionki czytnika: rodziny, rozmiary, pogrubienie, szerokosci znakow.
//
//  Rodziny:
//    Classic   - wbudowany font Elecrow, stala szerokosc, rozmiary 12/16 px,
//                polskie litery skladane z litery + znaczka (patrz board_crowpanel.h)
//    Helvetica - U8g2 helvR/helvB (_te), 10/12/14/18 px, proporcjonalny, prawdziwe polskie litery
//    Century   - U8g2 ncenR/ncenB (_te), New Century Schoolbook (szeryfowa), 10/12/14/18 px
//    Unifont   - U8g2 unifont_t_polish, 16 px
//
//  Wymaga (w tej kolejnosci przed tym plikiem): Adafruit_GFX.h,
//  U8g2_for_Adafruit_GFX.h, board_crowpanel.h.
//  USE_U8G2 = 0 wylacza Helvetice i Unifont (wtedy biblioteki nie sa potrzebne).
// ============================================================
#include <Arduino.h>

#ifndef USE_U8G2
#define USE_U8G2 1
#endif

// Dostrajanie rodzin U8g2 (piksele): odstep od gory (na kreski nad Ś, Ź, Ó)
// i dodatkowy odstep miedzy wierszami.
#define U8_TOP_PAD 2
#define U8_LINE_GAP 0

enum { FAM_CLASSIC = 0, FAM_HELV = 1, FAM_UNIFONT = 2, FAM_CENTURY = 3 };   // numery zapisane w NVS - nie zmieniac

static const uint8_t SZ_CLASSIC[2] = { 12, 16 };   // 24 i 32 px usuniete: za malo liter w wierszu
static const uint8_t SZ_HELV[4] = { 10, 12, 14, 18 };
static const uint8_t SZ_CENTURY[4] = { 10, 12, 14, 18 };
static const uint8_t SZ_UNIFONT[1] = { 16 };

inline int fontFamilyCount() {
  return USE_U8G2 ? 4 : 1;
}
inline const char* fontFamilyName(int f) {
  return f == FAM_HELV ? "Helvetica" : f == FAM_UNIFONT ? "Unifont" : f == FAM_CENTURY ? "Century" : "Classic";
}
inline int fontSizeCount(int f) {
  return (f == FAM_HELV || f == FAM_CENTURY) ? 4 : f == FAM_UNIFONT ? 1 : 2;
}
inline int fontSizeValue(int f, int si) {
  if (f == FAM_HELV) return SZ_HELV[si & 3];
  if (f == FAM_CENTURY) return SZ_CENTURY[si & 3];
  if (f == FAM_UNIFONT) return SZ_UNIFONT[0];
  return SZ_CLASSIC[si & 1];
}

#if USE_U8G2
// ---------- "Plotno" Adafruit_GFX rysujace do bufora e-papieru ----------
class EpdCanvas : public Adafruit_GFX {
 public:
  EpdCanvas() : Adafruit_GFX(SCREEN_W, SCREEN_H) {}
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (color) gfxInk(x, y);          // kolor 1 = tusz; x od lewej krawedzi
  }
};
static EpdCanvas fontCanvas;
static U8G2_FOR_ADAFRUIT_GFX u8g2;

// Polskie litery, ktore zostawiamy bez zmian w rodzinach U8g2
static const uint16_t POLISH[18] = {
  0x0104, 0x0105, 0x0106, 0x0107, 0x0118, 0x0119, 0x0141, 0x0142, 0x0143,
  0x0144, 0x00D3, 0x00F3, 0x015A, 0x015B, 0x0179, 0x017A, 0x017B, 0x017C
};
#endif

// ---------- Biezaca czcionka ----------
static int fFam = FAM_CLASSIC;
static int fSize = 1;
static bool fBold = false;
static int fLineH = 16;      // wysokosc wiersza
static int fFixedW = 8;      // szerokosc znaku (0 = proporcjonalny)
static int fTopPad = 0;      // odstep od gory (rodziny U8g2)
static int fAsc = 0;         // ascent (rodziny U8g2): linia bazowa = gora wiersza + fAsc
static int fLineGap = 0;          // dodatkowy odstep miedzy wierszami (interlinia), px
static bool fSynthBold = false;   // pogrubienie U8g2 przez podwojne rysowanie (+1 px)

#if USE_U8G2
static const uint8_t* fFont = nullptr;
static uint8_t wAscii[95];     // szerokosci znakow 32..126
static uint8_t wPol[18];       // szerokosci polskich liter
static uint8_t wFallback = 6;

inline int utf8Encode(uint32_t cp, char* out) {
  if (cp < 0x80) { out[0] = (char)cp; return 1; }
  out[0] = (char)(0xC0 | (cp >> 6));
  out[1] = (char)(0x80 | (cp & 0x3F));
  return 2;
}

// Kod znaku -> kod, ktory font U8g2 na pewno ma (ASCII + polskie litery);
// reszta (cudzyslowy, myslniki, litery z innych jezykow) sprowadzana do ASCII.
inline uint32_t fontNorm(uint32_t cp) {
  if (cp >= 32 && cp < 127) return cp;
  if (cp < 32) return ' ';
  for (int i = 0; i < 18; i++) {
    if (POLISH[i] == cp) return cp;
  }
  char b;
  uint8_t d;
  gfxMapCp(cp, b, d);
  return (uint8_t)b;
}

// Przesuniecie do nastepnego znaku. getUTF8Width() zwraca dla OSTATNIEGO znaku napisu jego
// szerokosc widoczna (bez odstepow; spacja ma 0), a nie przesuniecie - suma takich wartosci dla
// pojedynczych znakow jest za mala i wiersze wychodza za prawa krawedz. Roznica
// szerokosci "cc" i "c" daje czyste przesuniecie niezaleznie od tego, jak liczony jest ostatni znak.
static int u8Advance(const char* ch, int n) {
  char s1[4], s2[8];
  memcpy(s1, ch, n);
  s1[n] = '\0';
  memcpy(s2, ch, n);
  memcpy(s2 + n, ch, n);
  s2[2 * n] = '\0';
  int adv = u8g2.getUTF8Width(s2) - u8g2.getUTF8Width(s1);
  if (adv <= 0) adv = u8g2.getUTF8Width(s1) + 1;     // zabezpieczenie (brak glifu itp.)
  return adv < 1 ? 1 : adv;
}

static void fontBuildWidths() {
  int extra = fSynthBold ? 1 : 0;
  char s[4];
  for (int c = 32; c < 127; c++) {
    s[0] = (char)c;
    int w = u8Advance(s, 1) + extra;
    wAscii[c - 32] = (uint8_t)(w > 255 ? 255 : w);
  }
  for (int i = 0; i < 18; i++) {
    int n = utf8Encode(POLISH[i], s);
    int w = u8Advance(s, n) + extra;
    wPol[i] = (uint8_t)(w > 255 ? 255 : w);
  }
  wFallback = wAscii['?' - 32];

  // Samokalibracja: porownaj sume szerokosci z tabeli z prawdziwa szerokoscia tekstu probnego.
  // Gdyby tabela dawala mniej, dolicz roznice do kazdego znaku - wiersze nigdy nie wyjda za krawedz.
  static const char* probe = "The quick brown fox jumps over the lazy dog. Zażółć gęślą jaźń, WWW iii mmm.";
  int trueW = u8g2.getUTF8Width(probe) + extra;
  int sum = 0, cnt = 0, len = (int)strlen(probe), i = 0;
  while (i < len) {
    uint32_t cp;
    i += gfxU8Decode(probe + i, len - i, cp);
    if (cp == 0) continue;
    uint32_t nc = fontNorm(cp);
    int w = wFallback;
    if (nc >= 32 && nc < 127) w = wAscii[nc - 32];
    else for (int k = 0; k < 18; k++) if (POLISH[k] == nc) w = wPol[k];
    sum += w;
    cnt++;
  }
  int corr = 0;
  if (cnt > 0 && trueW > sum) {
    corr = (trueW - sum + cnt - 1) / cnt;
    for (int k = 0; k < 95; k++) wAscii[k] = (uint8_t)min(255, wAscii[k] + corr);
    for (int k = 0; k < 18; k++) wPol[k] = (uint8_t)min(255, wPol[k] + corr);
    wFallback = wAscii['?' - 32];
  }
  Serial.printf("czcionka: a=%d i=%d spacja=%d W=%d | tekst probny: biblioteka %d px, tabela %d px, korekta +%d\n",
                wAscii['a' - 32], wAscii['i' - 32], wAscii[0], wAscii['W' - 32], trueW, sum, corr);
}
#endif

inline void fontInit() {
#if USE_U8G2
  u8g2.begin(fontCanvas);
#endif
}

#if USE_U8G2
// Mierzy rzeczywista wysokosc rysowanych glifow (akcenty nad literami, ogonki pod nimi): rysuje
// probke do bufora ekranu i szuka najwyzszego/najnizszego zapalonego wiersza wzgledem linii bazowej.
// getFontAscent()/getFontDescent() podaja tylko wysokosc "A" i "g", wiec bez tego wiersze
// z Ś, Ż, ą, ę nachodzily na siebie. Bufor jest czyszczony - wolac przed rysowaniem ekranu.
static void fontMeasureBox(const char* probe, int& top, int& bottom) {
  gfxClear();
  const int by = 70;
  u8g2.setFont(fFont);
  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(1);
  u8g2.drawUTF8(2, by, probe);
  int minR = SCREEN_H, maxR = -1;
  for (int y = 0; y < SCREEN_H; y++) {
    for (int x = 0; x < SCREEN_W; x++) {
      if (gfxIsInk(x, y)) {
        if (y < minR) minR = y;
        if (y > maxR) maxR = y;
      }
    }
  }
  gfxClear();
  top = (maxR < 0) ? 0 : by - minR;
  bottom = (maxR < 0) ? 0 : maxR - by;
}
#endif

// Wybiera czcionke ksiazki. Wolac przy starcie i po zmianie ustawien.
inline void fontSetLineGap(int px) { fLineGap = px < 0 ? 0 : px > 8 ? 8 : px; }

inline void fontSelect(int fam, int si, bool bold) {
  if (fam < 0 || fam >= fontFamilyCount()) fam = FAM_CLASSIC;
  if (si < 0 || si >= fontSizeCount(fam)) si = 0;
  fFam = fam;
  fSize = si;
  fBold = bold;

  if (fam == FAM_CLASSIC) {
    int px = SZ_CLASSIC[si];
    fLineH = px + fLineGap;
    fFixedW = px / 2;
    fTopPad = 0;
    fAsc = 0;
    fSynthBold = bold;
    return;
  }

#if USE_U8G2
  fFixedW = 0;
  fTopPad = U8_TOP_PAD;
  fSynthBold = false;
  if (fam == FAM_HELV) {
    if (si == 0) fFont = bold ? u8g2_font_helvB10_te : u8g2_font_helvR10_te;
    else if (si == 1) fFont = bold ? u8g2_font_helvB12_te : u8g2_font_helvR12_te;
    else if (si == 2) fFont = bold ? u8g2_font_helvB14_te : u8g2_font_helvR14_te;
    else fFont = bold ? u8g2_font_helvB18_te : u8g2_font_helvR18_te;
  } else if (fam == FAM_CENTURY) {
    if (si == 0) fFont = bold ? u8g2_font_ncenB10_te : u8g2_font_ncenR10_te;
    else if (si == 1) fFont = bold ? u8g2_font_ncenB12_te : u8g2_font_ncenR12_te;
    else if (si == 2) fFont = bold ? u8g2_font_ncenB14_te : u8g2_font_ncenR14_te;
    else fFont = bold ? u8g2_font_ncenB18_te : u8g2_font_ncenR18_te;
  } else {
    fFont = u8g2_font_unifont_t_polish;
    fSynthBold = bold;                   // Unifont nie ma boldu - pogrubiamy sztucznie (podwojne rysowanie)
  }
  u8g2.setFont(fFont);
  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(1);
  fAsc = u8g2.getFontAscent();           // wysokosc litery "A" - polskie znaki sa wyzsze/nizsze
  int desc = u8g2.getFontDescent();      // wartosc ujemna
  int mTop = 0, mBottom = 0;
  fontMeasureBox(fam == FAM_UNIFONT ? "ĄĆĘŁŃÓŚŹŻąćęłńóśźżgjpqy" : "ąćęłńóśźżbdfhklgjpqy", mTop, mBottom);
  if (mTop > fAsc) fAsc = mTop;          // linia bazowa nizej, zeby akcenty nie wypadaly poza wiersz
  if (mBottom > -desc) desc = -mBottom;
  fLineH = fAsc - desc + 1 + U8_LINE_GAP + fLineGap;
  fontBuildWidths();
#endif
}

inline int fontLineH() { return fLineH; }
inline int fontFixedW() { return fFixedW; }
inline int fontTopPad() { return fTopPad; }

// Szerokosc znaku (kod z dekodera UTF-8) w pikselach
inline int fontCharW(uint32_t cp) {
  if (cp == 0) return 0;
  if (fFam == FAM_CLASSIC) return fFixedW;
#if USE_U8G2
  cp = fontNorm(cp);
  if (cp >= 32 && cp < 127) return wAscii[cp - 32];
  for (int i = 0; i < 18; i++) {
    if (POLISH[i] == cp) return wPol[i];
  }
  return wFallback;
#else
  return fFixedW;
#endif
}

// Rysuje 'len' bajtow UTF-8 w jednym wierszu; rowTop = gorna krawedz wiersza
// (juz z odstepem fontTopPad), x od LEWEJ krawedzi.
#if USE_U8G2
// Tekst interfejsu (menu, listy, naglowki) w Helvetice 14 px: wyrazne "l" (w Classic wyglada jak "1")
// i prawdziwy bold. y = gorna krawedz wiersza (jak w gfxText). Zwraca szerokosc napisu w px.
inline void uiSetHelv(bool bold) {
  u8g2.setFont(bold ? u8g2_font_helvB14_te : u8g2_font_helvR14_te);
  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(1);
}
inline int uiWidthHelv(const char* s, bool bold) {
  uiSetHelv(bold);
  return u8g2.getUTF8Width(s);
}
inline int uiTextHelv(int x, int y, const char* s, bool bold) {
  uiSetHelv(bold);
  u8g2.drawUTF8(x, y + u8g2.getFontAscent() + 1, s);
  return u8g2.getUTF8Width(s);
}
#endif

inline void fontDrawLine(int x, int rowTop, const char* s, int len) {
  if (len <= 0) return;

  if (fFam == FAM_CLASSIC) {
    int px = SZ_CLASSIC[fSize];
    int w = gfxTextN(x, rowTop, s, len, px);
    // EPD_ShowChar maluje tez tlo litery, wiec pogrubiamy gotowy tusz (nie rysujemy drugi raz)
    if (fBold) gfxEmbolden(x, rowTop, x + w, rowTop + px, px >= 24 ? 2 : 1);
    return;
  }

#if USE_U8G2
  char nb[256];
  int j = 0;
  int i = 0;
  while (i < len && j < 250) {
    uint32_t cp;
    i += gfxU8Decode(s + i, len - i, cp);
    if (cp == 0) continue;
    j += utf8Encode(fontNorm(cp), nb + j);
  }
  nb[j] = '\0';

  u8g2.setFont(fFont);
  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(1);
  int by = rowTop + fAsc;
  if (!fSynthBold) {
    u8g2.drawUTF8(x, by, nb);
  } else {
    // pogrubienie sztuczne: kazdy znak rysowany dwa razy (x i x+1) z TYM SAMYM skokiem, jaki liczy
    // zawijanie (szerokosc + 1) - inaczej litery stykaja sie, a wiersz jest krotszy niz zakladano
    int px = x, k = 0;
    while (nb[k]) {
      uint32_t cp;
      int n = gfxU8Decode(nb + k, 4, cp);
      if (n < 1) n = 1;
      char one[6];
      memcpy(one, nb + k, n);
      one[n] = '\0';
      u8g2.drawUTF8(px, by, one);
      u8g2.drawUTF8(px + 1, by, one);
      px += fontCharW(cp);
      k += n;
    }
  }
#endif
}
