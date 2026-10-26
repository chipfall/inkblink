#pragma once
// ============================================================
//  Reader fonts: families, sizes, bold, character widths.
//
//  Families:
//    Classic   - built-in Elecrow font, fixed width, sizes 12/16 px,
//                Polish letters composed from a letter + mark (see board_crowpanel.h)
//    Helvetica - U8g2 helvR/helvB (_te), 10/12/14/18 px, proportional, real Polish letters
//    Century   - U8g2 ncenR/ncenB (_te), New Century Schoolbook (serif), 10/12/14/18 px
//    Unifont   - U8g2 unifont_t_polish, 16 px
//
//  Needs (in this order, before this file): Adafruit_GFX.h,
//  U8g2_for_Adafruit_GFX.h, board_crowpanel.h.
//  USE_U8G2 = 0 turns off Helvetica and Unifont (the libraries are not needed then).
// ============================================================
#include <Arduino.h>

#ifndef USE_U8G2
#define USE_U8G2 1
#endif

// Tuning of the U8g2 families (pixels): space at the top (for the accents on Ś, Ź, Ó)
// and extra space between lines.
#define U8_TOP_PAD 2
#define U8_LINE_GAP 0

enum { FAM_CLASSIC = 0, FAM_HELV = 1, FAM_UNIFONT = 2, FAM_CENTURY = 3 };   // numbers stored in NVS - do not change

static const uint8_t SZ_CLASSIC[2] = { 12, 16 };   // 24 and 32 px removed: too few letters per line
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
// ---------- Adafruit_GFX "canvas" drawing into the e-paper buffer ----------
class EpdCanvas : public Adafruit_GFX {
 public:
  EpdCanvas() : Adafruit_GFX(SCREEN_W, SCREEN_H) {}
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (color) gfxInk(x, y);          // colour 1 = ink; x from the left edge
  }
};
static EpdCanvas fontCanvas;
static U8G2_FOR_ADAFRUIT_GFX u8g2;

// Polish letters that are kept unchanged in the U8g2 families
static const uint16_t POLISH[18] = {
  0x0104, 0x0105, 0x0106, 0x0107, 0x0118, 0x0119, 0x0141, 0x0142, 0x0143,
  0x0144, 0x00D3, 0x00F3, 0x015A, 0x015B, 0x0179, 0x017A, 0x017B, 0x017C
};
#endif

// ---------- Current font ----------
static int fFam = FAM_CLASSIC;
static int fSize = 1;
static bool fBold = false;
static int fLineH = 16;      // line height
static int fFixedW = 8;      // character width (0 = proportional)
static int fTopPad = 0;      // space at the top (U8g2 families)
static int fAsc = 0;         // ascent (U8g2 families): baseline = top of the line + fAsc
static int fLineGap = 0;          // extra space between lines (line spacing), px
static bool fSynthBold = false;   // U8g2 bold by drawing twice (+1 px)

#if USE_U8G2
static const uint8_t* fFont = nullptr;
static uint8_t wAscii[95];     // widths of characters 32..126
static uint8_t wPol[18];       // widths of Polish letters
static uint8_t wFallback = 6;

inline int utf8Encode(uint32_t cp, char* out) {
  if (cp < 0x80) { out[0] = (char)cp; return 1; }
  out[0] = (char)(0xC0 | (cp >> 6));
  out[1] = (char)(0x80 | (cp & 0x3F));
  return 2;
}

// Character code -> a code the U8g2 font surely has (ASCII + Polish letters);
// the rest (quotes, dashes, letters of other languages) is mapped to ASCII.
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

// Advance to the next character. getUTF8Width() returns for the LAST character of a string its
// visible width (without spacing; a space is 0), not the advance - the sum of such values for
// single characters is too small and lines run past the right edge. The difference between the
// widths of "cc" and "c" gives the pure advance, however the last character is measured.
static int u8Advance(const char* ch, int n) {
  char s1[4], s2[8];
  memcpy(s1, ch, n);
  s1[n] = '\0';
  memcpy(s2, ch, n);
  memcpy(s2 + n, ch, n);
  s2[2 * n] = '\0';
  int adv = u8g2.getUTF8Width(s2) - u8g2.getUTF8Width(s1);
  if (adv <= 0) adv = u8g2.getUTF8Width(s1) + 1;     // safety (missing glyph etc.)
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

  // Self-calibration: compare the sum of the table widths with the real width of a sample text.
  // If the table gives less, add the difference to every character - lines never run past the edge.
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
  Serial.printf("font: a=%d i=%d space=%d W=%d | sample text: library %d px, table %d px, correction +%d\n",
                wAscii['a' - 32], wAscii['i' - 32], wAscii[0], wAscii['W' - 32], trueW, sum, corr);
}
#endif

inline void fontInit() {
#if USE_U8G2
  u8g2.begin(fontCanvas);
#endif
}

#if USE_U8G2
// Measures the real height of the drawn glyphs (accents above letters, ogoneks below): draws
// a sample into the screen buffer and looks for the highest/lowest inked row relative to the baseline.
// getFontAscent()/getFontDescent() only give the height of "A" and "g", so without this, lines
// with Ś, Ż, ą, ę overlapped. The buffer is cleared - call before drawing a screen.
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

// Selects the book font. Call at start and after the settings change.
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
    fSynthBold = bold;                   // Unifont has no bold - we embolden it artificially (drawing twice)
  }
  u8g2.setFont(fFont);
  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(1);
  fAsc = u8g2.getFontAscent();           // height of the letter "A" - Polish characters are taller/lower
  int desc = u8g2.getFontDescent();      // negative value
  int mTop = 0, mBottom = 0;
  fontMeasureBox(fam == FAM_UNIFONT ? "ĄĆĘŁŃÓŚŹŻąćęłńóśźżgjpqy" : "ąćęłńóśźżbdfhklgjpqy", mTop, mBottom);
  if (mTop > fAsc) fAsc = mTop;          // baseline lower, so that accents do not fall outside the line
  if (mBottom > -desc) desc = -mBottom;
  fLineH = fAsc - desc + 1 + U8_LINE_GAP + fLineGap;
  fontBuildWidths();
#endif
}

inline int fontLineH() { return fLineH; }
inline int fontFixedW() { return fFixedW; }
inline int fontTopPad() { return fTopPad; }

// Width of a character (code from the UTF-8 decoder) in pixels
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

// Draws 'len' UTF-8 bytes in one line; rowTop = top edge of the line
// (already including fontTopPad), x from the LEFT edge.
#if USE_U8G2
// Interface text (menus, lists, titles). Modes ("Menu font" setting):
//   0 = Classic, 2 = Helvetica 12 (1 and 3 are former modes, treated as 2).
// Clear "l" (in Classic it looks like "1"); in the Helvetica modes a real bold.
// y = top edge of the line (as in gfxText).
inline void uiSetFont(int mode, bool bold) {
  if (mode == 2) u8g2.setFont(bold ? u8g2_font_helvB12_te : u8g2_font_helvR12_te);
  else if (mode == 3) u8g2.setFont(u8g2_font_unifont_t_polish);
  else u8g2.setFont(bold ? u8g2_font_helvB14_te : u8g2_font_helvR14_te);
  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(1);
}
inline int uiWidth(int mode, const char* s, bool bold) {
  uiSetFont(mode, bold);
  return u8g2.getUTF8Width(s);
}
inline int uiText(int mode, int x, int y, const char* s, bool bold) {
  uiSetFont(mode, bold);
  u8g2.drawUTF8(x, y + u8g2.getFontAscent() + 1, s);
  return u8g2.getUTF8Width(s);
}
#endif

inline void fontDrawLine(int x, int rowTop, const char* s, int len) {
  if (len <= 0) return;

  if (fFam == FAM_CLASSIC) {
    int px = SZ_CLASSIC[fSize];
    int w = gfxTextN(x, rowTop, s, len, px);
    // EPD_ShowChar also paints the letter background, so we embolden the finished ink (no second drawing)
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
    // artificial bold: every character drawn twice (x and x+1) with THE SAME advance that the
    // wrapping uses (width + 1) - otherwise letters touch and the line is shorter than expected
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
