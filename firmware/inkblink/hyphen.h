#pragma once
// ============================================================
//  Polish hyphenation (rules, no dictionary).
//
//  hyphenPoints() returns the places (byte offsets from the start of the word) where
//  a hyphen may be inserted. The word must consist of letters (ASCII + Polish).
//
//  Rules (simplified, following the usual division into syllables):
//   - a syllable has one vowel (a e i o u y ą ę ó); "i" before a vowel only softens the consonant
//     (miasto = mia-sto, ciemny = cie-mny) and is not a separate syllable,
//   - the digraphs cz sz rz ch dz dź dż are never split,
//   - between two vowels: the break goes between them (po-eta, na-uka),
//   - a single consonant between vowels goes to the next syllable (ma-ma),
//   - a consonant cluster: the longest end of the cluster that can start a syllable goes to the next
//     syllable (st sp sk zw sł tr dr pl kr ... str spr), the rest stays (pol-ska, kost-ka, wie-trze),
//   - at least 2 letters stay on each side of the hyphen; words shorter than 6 letters and
//     words with capitals in the middle (abbreviations) are not split.
// ============================================================
#include <stdint.h>
#include <string.h>

#define HY_MAXL 40
#define HY_MIN_WORD 6
#define HY_MIN_SIDE 2

static inline uint16_t hyLower(uint32_t cp) {
  if (cp >= 'A' && cp <= 'Z') return cp + 32;
  switch (cp) {
    case 0x0104: return 0x0105;  // Ą
    case 0x0106: return 0x0107;  // Ć
    case 0x0118: return 0x0119;  // Ę
    case 0x0141: return 0x0142;  // Ł
    case 0x0143: return 0x0144;  // Ń
    case 0x00D3: return 0x00F3;  // Ó
    case 0x015A: return 0x015B;  // Ś
    case 0x0179: return 0x017A;  // Ź
    case 0x017B: return 0x017C;  // Ż
  }
  return (uint16_t)cp;
}
static inline bool hyIsUpper(uint32_t cp) {
  return (cp >= 'A' && cp <= 'Z') || cp == 0x0104 || cp == 0x0106 || cp == 0x0118 || cp == 0x0141 ||
         cp == 0x0143 || cp == 0x00D3 || cp == 0x015A || cp == 0x0179 || cp == 0x017B;
}
static inline bool hyIsLetter(uint32_t lc) {
  if (lc >= 'a' && lc <= 'z') return true;
  switch (lc) {
    case 0x0105: case 0x0107: case 0x0119: case 0x0142: case 0x0144:
    case 0x00F3: case 0x015B: case 0x017A: case 0x017C: return true;
  }
  return false;
}
static inline bool hyIsVowel(uint32_t lc) {
  return lc == 'a' || lc == 'e' || lc == 'i' || lc == 'o' || lc == 'u' || lc == 'y' ||
         lc == 0x0105 || lc == 0x0119 || lc == 0x00F3;
}
// one letter -> ASCII character used to describe consonant units
static inline char hySym(uint32_t lc) {
  switch (lc) {
    case 0x0107: return 'C';  // ć
    case 0x015B: return 'S';  // ś
    case 0x017A: return 'X';  // ź
    case 0x017C: return 'J';  // ż
    case 0x0144: return 'N';  // ń
    case 0x0142: return 'L';  // ł
  }
  return (char)lc;
}

// ---------- consonant classes (unit text: not "tr", only single units: "t", "rz", "ch") ----------
static inline bool hyIn(const char* u, const char* const* set) {
  for (; *set; set++) if (strcmp(u, *set) == 0) return true;
  return false;
}
static const char* const HY_STOP[] = { "p", "b", "t", "d", "k", "g", nullptr };
static const char* const HY_OBSTR[] = { "p", "b", "t", "d", "k", "g", "f", "w", "ch", "c", "dz", "cz", "h", "dX", "dJ", "C", nullptr };
static const char* const HY_LIQ[] = { "r", "l", "rz", "L", nullptr };
static const char* const HY_SIB[] = { "s", "z", "sz", "J", "S", "X", nullptr };
static const char* const HY_SIB_VL[] = { "s", "sz", "S", nullptr };          // voiceless
static const char* const HY_SIB_VD[] = { "z", "J", "X", nullptr };           // voiced
static const char* const HY_AFTER_VL[] = { "p", "t", "k", "m", "n", "w", "L", "l", "f", "c", "ch", "N", nullptr };
static const char* const HY_AFTER_VD[] = { "b", "d", "g", "m", "n", "w", "L", "l", "N", nullptr };   // "zp" does not start a syllable (bez-pie-czny)

static inline bool hyOnset2(const char* a, const char* b) {
  return (hyIn(a, HY_OBSTR) && hyIn(b, HY_LIQ)) || (hyIn(a, HY_SIB_VL) && hyIn(b, HY_AFTER_VL)) ||
         (hyIn(a, HY_SIB_VD) && hyIn(b, HY_AFTER_VD));
}
static inline bool hyOnset3(const char* a, const char* b, const char* c) {
  if (hyIn(a, HY_SIB) && hyIn(b, HY_STOP) && hyIn(c, HY_LIQ)) return true;           // str, spr, zdr ...
  // kszt (wy-kształ-cenie), pszcz (pszczo-ła)
  return (strcmp(a, "k") == 0 || strcmp(a, "p") == 0) && strcmp(b, "sz") == 0 &&
         (strcmp(c, "t") == 0 || strcmp(c, "cz") == 0 || strcmp(c, "k") == 0 || strcmp(c, "p") == 0);
}

// Returns the number of break points (max maxPts); pts[] ascending, in bytes from the start of the word.
inline int hyphenPoints(const char* w, int len, uint8_t* pts, int maxPts) {
  uint16_t L[HY_MAXL + 1];
  uint8_t bpos[HY_MAXL + 2];
  int n = 0;
  for (int i = 0; i < len;) {
    uint8_t c = (uint8_t)w[i];
    uint32_t cp;
    int nb;
    if (c < 0x80) { cp = c; nb = 1; }
    else if ((c & 0xE0) == 0xC0 && i + 1 < len) { cp = ((c & 0x1F) << 6) | (w[i + 1] & 0x3F); nb = 2; }
    else return 0;
    uint16_t lc = hyLower(cp);
    if (!hyIsLetter(lc)) return 0;
    if (n > 0 && hyIsUpper(cp)) return 0;          // abbreviation / name with a capital in the middle
    if (n >= HY_MAXL) return 0;
    L[n] = lc;
    bpos[n] = (uint8_t)i;
    n++;
    i += nb;
  }
  bpos[n] = (uint8_t)len;
  if (n < HY_MIN_WORD) return 0;

  // ---- units: a vowel or a consonant (with a digraph and a softening "i") ----
  struct Unit { uint8_t start; uint8_t nl; bool vowel; char base[4]; };
  Unit U[HY_MAXL + 1];
  int nu = 0;
  for (int i = 0; i < n;) {
    Unit u;
    u.start = (uint8_t)i;
    u.vowel = false;
    u.base[0] = 0;
    if (hyIsVowel(L[i])) {
      u.vowel = true;
      u.nl = 1;
      i++;
    } else {
      int k = 1;
      uint16_t a = L[i], b = (i + 1 < n) ? L[i + 1] : 0;
      bool dig = (a == 'c' && (b == 'z' || b == 'h')) || (a == 's' && b == 'z') || (a == 'r' && b == 'z') ||
                 (a == 'd' && (b == 'z' || b == 0x017A || b == 0x017C));
      if (dig) k = 2;
      int bl = 0;
      for (int j = 0; j < k && bl < 3; j++) u.base[bl++] = hySym(L[i + j]);
      u.base[bl] = 0;
      i += k;
      // a softening "i" right before a vowel belongs to the consonant (mia-sto)
      if (i + 1 < n && L[i] == 'i' && hyIsVowel(L[i + 1])) { k++; i++; }
      u.nl = (uint8_t)k;
    }
    U[nu++] = u;
  }

  // ---- break points between consecutive vowels ----
  bool brk[HY_MAXL + 1];                 // brk[l] = a break is allowed after l letters
  for (int i = 0; i <= n; i++) brk[i] = false;
  int prevV = -1;
  for (int ui = 0; ui < nu; ui++) {
    if (!U[ui].vowel) continue;
    if (prevV >= 0) {
      int cs = prevV + 1, ce = ui;       // consonants: units cs .. ce-1
      int k = ce - cs;
      int breakUnit;                     // the break comes right before this unit
      if (k == 0) {
        // two vowels next to each other; "au"/"eu" at the start of a word is a diphthong
        bool diph = (prevV == 0 && ui == 1 && (L[0] == 'a' || L[0] == 'e') && L[1] == 'u');
        bool same = L[U[prevV].start] == L[U[ui].start];      // aa, ee: not split (Nilfgaard)
        breakUnit = (diph || same) ? -1 : ui;
      } else if (k == 1) {
        breakUnit = cs;
      } else {
        int m = 1;
        if (k >= 3 && hyOnset3(U[ce - 3].base, U[ce - 2].base, U[ce - 1].base)) m = 3;
        else if (hyOnset2(U[ce - 2].base, U[ce - 1].base)) m = 2;
        breakUnit = ce - m;
        // no cluster starts with itself except for the first word: when the whole cluster can
        // start a syllable (m == k), the break comes right after the vowel
        if (m == k) breakUnit = cs;
      }
      if (breakUnit >= 0) brk[U[breakUnit].start] = true;
    }
    prevV = ui;
  }

  int cnt = 0;
  for (int l = HY_MIN_SIDE; l <= n - HY_MIN_SIDE; l++) {
    if (brk[l] && cnt < maxPts) pts[cnt++] = bpos[l];
  }
  return cnt;
}
