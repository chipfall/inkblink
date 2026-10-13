#pragma once
// ============================================================
//  Dzielenie wyrazow polskich (reguly, bez slownika).
//
//  hyphenPoints() zwraca miejsca (przesuniecia w bajtach od poczatku wyrazu), w ktorych
//  wolno wstawic myslnik. Wyraz musi skladac sie z liter (ASCII + polskie).
//
//  Zasady (uproszczone, zgodne z typowym podzialem na sylaby):
//   - sylaba ma jedna samogloske (a e i o u y ą ę ó); "i" przed samogloska to zmiekczenie
//     (miasto = mia-sto, ciemny = cie-mny), a nie osobna sylaba,
//   - dwuznaki cz sz rz ch dz dź dż nie sa rozdzielane,
//   - miedzy dwiema samogloskami: przerwa miedzy nimi (po-eta, na-uka),
//   - jedna spolgloska miedzy samogloskami idzie do nastepnej sylaby (ma-ma),
//   - grupa spolglosek: do nastepnej sylaby idzie najdluzszy koniec grupy, ktory moze zaczynac
//     sylabe (st sp sk zw sł tr dr pl kr ... str spr), reszta zostaje (pol-ska, kost-ka, wie-trze),
//   - po obu stronach myslnika zostaja co najmniej 2 litery; wyrazy krotsze niz 6 liter oraz
//     wyrazy z wielkimi literami w srodku (skroty) nie sa dzielone.
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
// jedna litera -> znak ASCII uzywany w opisie jednostek spolgloskowych
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

// ---------- klasy spolglosek (tekst jednostki, np. "tr" nie, tylko pojedyncza jednostka: "t", "rz", "ch") ----------
static inline bool hyIn(const char* u, const char* const* set) {
  for (; *set; set++) if (strcmp(u, *set) == 0) return true;
  return false;
}
static const char* const HY_STOP[] = { "p", "b", "t", "d", "k", "g", nullptr };
static const char* const HY_OBSTR[] = { "p", "b", "t", "d", "k", "g", "f", "w", "ch", "c", "dz", "cz", "h", "dX", "dJ", "C", nullptr };
static const char* const HY_LIQ[] = { "r", "l", "rz", "L", nullptr };
static const char* const HY_SIB[] = { "s", "z", "sz", "J", "S", "X", nullptr };
static const char* const HY_SIB_VL[] = { "s", "sz", "S", nullptr };          // bezdzwieczne
static const char* const HY_SIB_VD[] = { "z", "J", "X", nullptr };           // dzwieczne
static const char* const HY_AFTER_VL[] = { "p", "t", "k", "m", "n", "w", "L", "l", "f", "c", "ch", "N", nullptr };
static const char* const HY_AFTER_VD[] = { "b", "d", "g", "m", "n", "w", "L", "l", "N", nullptr };   // "zp" nie zaczyna sylaby (bez-pie-czny)

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

// Zwraca liczbe punktow podzialu (maks. maxPts); pts[] rosnaco, w bajtach od poczatku wyrazu.
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
    if (n > 0 && hyIsUpper(cp)) return 0;          // skrot / nazwa z wielka litera w srodku
    if (n >= HY_MAXL) return 0;
    L[n] = lc;
    bpos[n] = (uint8_t)i;
    n++;
    i += nb;
  }
  bpos[n] = (uint8_t)len;
  if (n < HY_MIN_WORD) return 0;

  // ---- jednostki: samogloska albo spolgloska (z dwuznakiem i zmiekczajacym "i") ----
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
      // zmiekczajace "i" tuz przed samogloska nalezy do spolgloski (mia-sto)
      if (i + 1 < n && L[i] == 'i' && hyIsVowel(L[i + 1])) { k++; i++; }
      u.nl = (uint8_t)k;
    }
    U[nu++] = u;
  }

  // ---- punkty podzialu pomiedzy kolejnymi samogloskami ----
  bool brk[HY_MAXL + 1];                 // brk[l] = mozna przerwac po l literach
  for (int i = 0; i <= n; i++) brk[i] = false;
  int prevV = -1;
  for (int ui = 0; ui < nu; ui++) {
    if (!U[ui].vowel) continue;
    if (prevV >= 0) {
      int cs = prevV + 1, ce = ui;       // spolgloski: jednostki cs .. ce-1
      int k = ce - cs;
      int breakUnit;                     // przerwa tuz przed ta jednostka
      if (k == 0) {
        // dwie samogloski obok siebie; "au"/"eu" na poczatku wyrazu to dyftong
        bool diph = (prevV == 0 && ui == 1 && (L[0] == 'a' || L[0] == 'e') && L[1] == 'u');
        bool same = L[U[prevV].start] == L[U[ui].start];      // aa, ee: nie dzielimy (Nilfgaard)
        breakUnit = (diph || same) ? -1 : ui;
      } else if (k == 1) {
        breakUnit = cs;
      } else {
        int m = 1;
        if (k >= 3 && hyOnset3(U[ce - 3].base, U[ce - 2].base, U[ce - 1].base)) m = 3;
        else if (hyOnset2(U[ce - 2].base, U[ce - 1].base)) m = 2;
        breakUnit = ce - m;
        // zadna grupa nie zaczyna sie od samej siebie poza pierwszym wyrazem: gdy cala grupa jest
        // poczatkiem sylaby (m == k), przerwa jest tuz za samogloska
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
