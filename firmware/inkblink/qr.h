#pragma once
// ============================================================
//  Minimalny generator kodow QR (bez zewnetrznych bibliotek).
//  Tryb bajtowy, korekcja bledow M, wersje 1-5 (do 84 znakow).
//  Uzycie:
//    static QrCode qr;
//    if (qrEncode("WIFI:T:WPA;S:siec;P:haslo;;", qr)) {
//      bool ciemny = qrDark(qr, x, y);      // 0 <= x,y < qr.size
//    }
// ============================================================
#include <Arduino.h>
#include <string.h>

#define QR_MAX_VER 5
#define QR_MAX_SIZE (17 + 4 * QR_MAX_VER)     // 37 modulow

struct QrCode {
  uint8_t version;
  uint8_t size;                               // modulow na bok
  uint8_t m[QR_MAX_SIZE * QR_MAX_SIZE];       // 1 = ciemny
};

inline bool qrDark(const QrCode& q, int x, int y) {
  if (x < 0 || y < 0 || x >= q.size || y >= q.size) return false;
  return q.m[y * q.size + x] != 0;
}

// ---------- Cialo GF(256), wielomian 0x11D ----------
static uint8_t qrExpT[512];
static uint8_t qrLogT[256];
static bool qrGfReady = false;

inline void qrGfSetup() {
  if (qrGfReady) return;
  int x = 1;
  for (int i = 0; i < 255; i++) {
    qrExpT[i] = (uint8_t)x;
    qrLogT[x] = (uint8_t)i;
    x <<= 1;
    if (x & 0x100) x ^= 0x11D;
  }
  for (int i = 255; i < 512; i++) qrExpT[i] = qrExpT[i - 255];
  qrGfReady = true;
}

inline uint8_t qrMul(uint8_t a, uint8_t b) {
  return (a && b) ? qrExpT[qrLogT[a] + qrLogT[b]] : 0;
}

// Wielomian generujacy stopnia n (wspolczynniki bez wiodacej jedynki, od najwyzszej potegi)
inline void qrRsGenerator(int n, uint8_t* g) {
  memset(g, 0, n);
  g[n - 1] = 1;
  uint8_t root = 1;
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      g[j] = qrMul(g[j], root);
      if (j + 1 < n) g[j] ^= g[j + 1];
    }
    root = qrMul(root, 2);
  }
}

// Reszta z dzielenia (bajty korekcji bledow)
inline void qrRsRemainder(const uint8_t* data, int len, const uint8_t* g, int n, uint8_t* res) {
  memset(res, 0, n);
  for (int i = 0; i < len; i++) {
    uint8_t f = data[i] ^ res[0];
    memmove(res, res + 1, n - 1);
    res[n - 1] = 0;
    for (int j = 0; j < n; j++) res[j] ^= qrMul(g[j], f);
  }
}

// ---------- Parametry wersji (korekcja M) ----------
struct QrVerInfo {
  uint8_t ecPerBlock;
  uint8_t blocks;
  uint8_t dataPerBlock;
};
static const QrVerInfo QR_VER[QR_MAX_VER + 1] = {
  { 0, 0, 0 }, { 10, 1, 16 }, { 16, 1, 28 }, { 26, 1, 44 }, { 18, 2, 32 }, { 24, 2, 43 }
};

// ---------- Budowa macierzy ----------
struct QrBuild {
  uint8_t* m;          // ciemne moduly
  uint8_t* fn;         // 1 = modul funkcyjny (nie zawiera danych)
  int size;
};

inline void qrSet(QrBuild& b, int x, int y, bool dark) {
  b.m[y * b.size + x] = dark ? 1 : 0;
  b.fn[y * b.size + x] = 1;
}

inline void qrFinder(QrBuild& b, int cx, int cy) {
  for (int dy = -4; dy <= 4; dy++) {
    for (int dx = -4; dx <= 4; dx++) {
      int xx = cx + dx, yy = cy + dy;
      if (xx < 0 || xx >= b.size || yy < 0 || yy >= b.size) continue;
      int dist = max(abs(dx), abs(dy));
      qrSet(b, xx, yy, dist != 2 && dist != 4);
    }
  }
}

inline void qrAlignment(QrBuild& b, int cx, int cy) {
  for (int dy = -2; dy <= 2; dy++)
    for (int dx = -2; dx <= 2; dx++) qrSet(b, cx + dx, cy + dy, max(abs(dx), abs(dy)) != 1);
}

inline void qrFormatBits(QrBuild& b, int mask) {
  int data = (0 << 3) | mask;                 // poziom M = 00
  int rem = data;
  for (int i = 0; i < 10; i++) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
  int bits = ((data << 10) | rem) ^ 0x5412;
  auto bit = [&](int i) { return ((bits >> i) & 1) != 0; };

  for (int i = 0; i <= 5; i++) qrSet(b, 8, i, bit(i));
  qrSet(b, 8, 7, bit(6));
  qrSet(b, 8, 8, bit(7));
  qrSet(b, 7, 8, bit(8));
  for (int i = 9; i < 15; i++) qrSet(b, 14 - i, 8, bit(i));

  for (int i = 0; i < 8; i++) qrSet(b, b.size - 1 - i, 8, bit(i));
  for (int i = 8; i < 15; i++) qrSet(b, 8, b.size - 15 + i, bit(i));
  qrSet(b, 8, b.size - 8, true);              // zawsze ciemny modul
}

inline bool qrMaskBit(int mask, int x, int y) {
  switch (mask) {
    case 0: return (x + y) % 2 == 0;
    case 1: return y % 2 == 0;
    case 2: return x % 3 == 0;
    case 3: return (x + y) % 3 == 0;
    case 4: return (x / 3 + y / 2) % 2 == 0;
    case 5: return x * y % 2 + x * y % 3 == 0;
    case 6: return (x * y % 2 + x * y % 3) % 2 == 0;
    default: return ((x + y) % 2 + x * y % 3) % 2 == 0;
  }
}

inline void qrApplyMask(QrBuild& b, int mask) {
  for (int y = 0; y < b.size; y++)
    for (int x = 0; x < b.size; x++)
      if (!b.fn[y * b.size + x] && qrMaskBit(mask, x, y)) b.m[y * b.size + x] ^= 1;
}

// Kara za wzorzec (wybor najlepszej maski)
inline long qrPenalty(const QrBuild& b) {
  int n = b.size;
  long result = 0;
  auto get = [&](int x, int y) { return b.m[y * n + x] != 0; };

  // N1: ciagi >= 5 tych samych modulow (wiersze i kolumny)
  for (int pass = 0; pass < 2; pass++) {
    for (int i = 0; i < n; i++) {
      int run = 1;
      bool prev = pass ? get(i, 0) : get(0, i);
      for (int j = 1; j < n; j++) {
        bool cur = pass ? get(i, j) : get(j, i);
        if (cur == prev) {
          run++;
          if (run == 5) result += 3;
          else if (run > 5) result += 1;
        } else {
          run = 1;
          prev = cur;
        }
      }
    }
  }
  // N2: bloki 2x2
  for (int y = 0; y < n - 1; y++)
    for (int x = 0; x < n - 1; x++) {
      bool c = get(x, y);
      if (c == get(x + 1, y) && c == get(x, y + 1) && c == get(x + 1, y + 1)) result += 3;
    }
  // N3: wzorzec przypominajacy znacznik poszukiwacza (1:1:3:1:1 + 4 jasne)
  static const uint8_t PA[11] = { 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0 };
  static const uint8_t PB[11] = { 0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1 };
  for (int pass = 0; pass < 2; pass++) {
    for (int i = 0; i < n; i++) {
      for (int j = 0; j + 11 <= n; j++) {
        bool ma = true, mb = true;
        for (int k = 0; k < 11; k++) {
          int v = pass ? get(i, j + k) : get(j + k, i);
          if (v != PA[k]) ma = false;
          if (v != PB[k]) mb = false;
        }
        if (ma) result += 40;
        if (mb) result += 40;
      }
    }
  }
  // N4: rownowaga ciemnych/jasnych
  long dark = 0;
  for (int i = 0; i < n * n; i++) dark += b.m[i];
  long total = (long)n * n;
  long diff = labs(dark * 20 - total * 10);
  long k = (diff + total - 1) / total - 1;
  if (k > 0) result += k * 10;
  return result;
}

// Zwraca false, gdy tekst jest za dlugi (> 84 znaki) lub pusty
inline bool qrEncode(const char* text, QrCode& q) {
  qrGfSetup();
  int len = (int)strlen(text);
  if (len <= 0) return false;

  int ver = 0;
  for (int v = 1; v <= QR_MAX_VER; v++) {
    int dcw = QR_VER[v].blocks * QR_VER[v].dataPerBlock;
    if (4 + 8 + len * 8 <= dcw * 8) { ver = v; break; }
  }
  if (!ver) return false;

  const QrVerInfo& vi = QR_VER[ver];
  int dataCw = vi.blocks * vi.dataPerBlock;
  int totalCw = dataCw + vi.blocks * vi.ecPerBlock;

  // --- bity danych: tryb 0100, liczba znakow (8 bitow), bajty, terminator, wypelnienie ---
  uint8_t data[96];
  memset(data, 0, sizeof(data));
  int bitPos = 0;
  auto put = [&](uint32_t val, int nbits) {
    for (int i = nbits - 1; i >= 0; i--) {
      if ((val >> i) & 1) data[bitPos >> 3] |= (uint8_t)(0x80 >> (bitPos & 7));
      bitPos++;
    }
  };
  put(0x4, 4);
  put((uint32_t)len, 8);
  for (int i = 0; i < len; i++) put((uint8_t)text[i], 8);
  int cap = dataCw * 8;
  int term = cap - bitPos;
  if (term > 4) term = 4;
  put(0, term);
  bitPos = (bitPos + 7) & ~7;
  for (uint8_t pad = 0xEC; (bitPos >> 3) < dataCw; pad ^= 0xEC ^ 0x11) {
    data[bitPos >> 3] = pad;
    bitPos += 8;
  }

  // --- bloki, korekcja bledow, przeplatanie ---
  uint8_t gen[32];
  qrRsGenerator(vi.ecPerBlock, gen);
  uint8_t ec[4][32];
  for (int b = 0; b < vi.blocks; b++)
    qrRsRemainder(data + b * vi.dataPerBlock, vi.dataPerBlock, gen, vi.ecPerBlock, ec[b]);

  uint8_t all[160];
  int idx = 0;
  for (int i = 0; i < vi.dataPerBlock; i++)
    for (int b = 0; b < vi.blocks; b++) all[idx++] = data[b * vi.dataPerBlock + i];
  for (int i = 0; i < vi.ecPerBlock; i++)
    for (int b = 0; b < vi.blocks; b++) all[idx++] = ec[b][i];
  (void)totalCw;

  // --- macierz ---
  int size = 17 + 4 * ver;
  uint8_t fn[QR_MAX_SIZE * QR_MAX_SIZE];
  memset(q.m, 0, sizeof(q.m));
  memset(fn, 0, sizeof(fn));
  q.version = (uint8_t)ver;
  q.size = (uint8_t)size;
  QrBuild b = { q.m, fn, size };

  for (int i = 0; i < size; i++) {          // znaczniki czasu
    qrSet(b, 6, i, i % 2 == 0);
    qrSet(b, i, 6, i % 2 == 0);
  }
  qrFinder(b, 3, 3);
  qrFinder(b, size - 4, 3);
  qrFinder(b, 3, size - 4);
  if (ver >= 2) qrAlignment(b, size - 7, size - 7);
  qrFormatBits(b, 0);                       // rezerwuje obszary formatu

  // dane: zygzak od prawego dolnego rogu
  int totalBits = totalCw * 8;
  int bi = 0;
  for (int right = size - 1; right >= 1; right -= 2) {
    if (right == 6) right = 5;
    for (int vert = 0; vert < size; vert++) {
      for (int j = 0; j < 2; j++) {
        int x = right - j;
        bool upward = ((right + 1) & 2) == 0;
        int y = upward ? size - 1 - vert : vert;
        if (!fn[y * size + x] && bi < totalBits) {
          q.m[y * size + x] = (all[bi >> 3] >> (7 - (bi & 7))) & 1;
          bi++;
        }
      }
    }
  }

  // wybor maski o najmniejszej karze
  int best = 0;
  long bestPen = 0x7FFFFFFF;
  for (int mask = 0; mask < 8; mask++) {
    qrApplyMask(b, mask);
    qrFormatBits(b, mask);
    long p = qrPenalty(b);
    if (p < bestPen) { bestPen = p; best = mask; }
    qrApplyMask(b, mask);                   // cofnij (XOR)
  }
  qrApplyMask(b, best);
  qrFormatBits(b, best);
  return true;
}
