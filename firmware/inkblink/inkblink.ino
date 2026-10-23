// ============================================================
//  InkBlink - e-book reader firmware for the CrowPanel ESP32 2.13" e-paper (DIE01021S)
//  Menu, Library, Reader, Statistics, Settings, Upload (own WiFi hotspot + web page)
//  Ksiazki (.txt) leza w LittleFS, czytane strona po stronie.
//
//  Zmiany 0.8:
//   - hotspot: kody QR (dolaczenie do sieci i otwarcie strony), ikona WiFi
//   - strona WWW po angielsku, nowy wyglad, wysylanie wielu plikow z paskiem postepu,
//     obraz uspienia wgrywany przez przegladarke
//   - wskaznik baterii (po ustawieniu pinu w board_crowpanel.h), test pinow w Info
//   - ekran uspienia: karta ksiazki / obraz / bez zmian
//   - statystyki: czas czytania, tempo, przewidywany czas do konca ksiazki
//   - szybkie przewracanie stron (eksperyment): odswiezanie czesciowe
//
//  Zmiany 0.7: licznik telefonow i zadan na ekranie hotspotu, diagnostyka.
//  Zmiany 0.6: legenda klawiszy, upload tylko przez hotspot.
//  Zmiany 0.5: czcionki, nawigacja z przytrzymaniem. 0.4: polskie znaki, ustawienia.
// ============================================================

// 1 = rodziny Helvetica i Unifont (potrzebne biblioteki: Adafruit GFX Library
// oraz U8g2_for_Adafruit_GFX). 0 = tylko Classic, bez dodatkowych bibliotek.
#define USE_U8G2 1

#include <Arduino.h>
#if USE_U8G2
#include <Adafruit_GFX.h>              // musza byc PRZED naszymi naglowkami (makra BLACK/WHITE)
#include <U8g2_for_Adafruit_GFX.h>
#endif
#include <WiFi.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <strings.h>

#include "qr.h"                 // generator kodow QR
#include "board_crowpanel.h"    // piny, przyciski, rysowanie, bateria
#include "ui_gfx.h"             // ikony, kod QR, prostokaty
#include "input.h"              // zdarzenia przyciskow
#include "clock.h"
#include "hyphen.h"
#include "battery_model.h"
#include "reader_fonts.h"       // czcionki ksiazki
#include "webui_assets.h"       // strona WWW (HTML/CSS/JS)

#define FW_VERSION "0.9.10"         // 0.9.N = paczka/update N (patrz docs/DEVELOPMENT.md); musi zgadzac sie z CHANGELOG.md

// ---------- Ustawienia ekranu / interfejsu ----------
#define FONT_SIZE 16            // czcionka interfejsu (menu, komunikaty)
#define FOOT_SIZE 12            // czcionka paska stanu
#define UI_CHARS 24             // maks. znakow w wierszu interfejsu (miesci sie przed legenda)
#define UI_X 4                  // lewy margines interfejsu (px)
#define LIST_ROWS 6
#define MARGIN_X 6              // lewy/prawy margines tekstu ksiazki (px)
#define STATUS_Y 112            // gorna krawedz paska stanu (px)

// ---------- Limity ----------
#define MAX_PAGES_CAP 60000     // 60000 stron = 240 KB RAM na offsety
#define MAX_BOOKS 40
#define MAX_CH 300                // maks. liczba rozdzialow w spisie tresci
#define TOC_MAX_BYTES 24000       // maks. rozmiar pliku .toc
#define NAME_LEN 48
#define PAGE_BUF 1536           // bajtow na strone (UTF-8 bywa 2-4 bajty/znak)
#define CHUNK_BUF 4096

// ---------- Cache indeksu ----------
#define USE_INDEX_CACHE 1
#define INDEX_MIN_BOOK 100000   // mniejszych ksiazek nie cache'ujemy
#define IDX_MAGIC 0x58444945UL
#define HYPHEN_VERSION 2       // zmien, gdy zmienisz reguly dzielenia wyrazow
#define WRAP_VERSION 5          // zmien, gdy zmienisz algorytm zawijania lub marginesy (5: nowy pomiar szerokosci znakow)

#define TMP_PATH "/upload.tmp"

// ---------- Hotspot ----------
#define AP_SSID_PREFIX "InkBlink-"  // nazwa sieci: InkBlink-XXXX (koncowka z adresu plytki)
#define AP_CHANNEL 1                // kanal WiFi hotspotu (1-13); przy zakloceniach sprobuj 6 lub 11
#define CAPTIVE_TRICKS 1            // 1 = DNS + odpowiedzi testow lacznosci (patrz handleNotFound)

// ---------- Obraz uspienia ----------
#define WALLPAPER_PATH "/wallpaper.bin"
#define WP_TMP "/wp.tmp"
#define WALLPAPER_BYTES 3904        // 32 bajty na wiersz * 122 wiersze (250 px, najstarszy bit pierwszy, 1 = tusz)

// ---------- Bateria / statystyki ----------
#define BAT_LOW_PCT 20              // ponizej tego poziomu ikona pokazuje sie tez w trybie "Low"
#define STATS_MAX_GAP_S 180         // dluzsza przerwa miedzy stronami nie liczy sie jako czytanie
#define STATS_MIN_TURN_S 2          // szybsze przewracanie to przegladanie, nie liczy sie do tempa
#define STATS_SAVE_EVERY 25         // zapis statystyk co tyle przewroconych stron

typedef const char* (*ItemFn)(int);

// Statystyki czytania (struktury musza byc przed pierwsza funkcja - patrz uwaga o ItemFn)
struct BookStats {
  uint32_t secs;       // czas czytania tej ksiazki (s)
  uint32_t spdBytes;   // bajty przeczytane strona po stronie (do tempa)
  uint32_t spdSecs;    // czas, w ktorym je przeczytano
  uint32_t pages;      // przewrocone strony
  uint8_t done;        // 1 = skonczona (>= 98%)
  uint8_t pad[3];
};
struct TotalStats {
  uint32_t secs, spdBytes, spdSecs, pages;
  uint16_t finished;   // skonczone ksiazki
  uint16_t pad;
};

// ============================================================
//  Ustawienia uzytkownika (zapisywane w pamieci nieulotnej)
// ============================================================
static const uint8_t sleepMinutes[5] = { 1, 5, 10, 30, 0 };   // 0 = nigdy
static const uint8_t fullEveryVals[3] = { 5, 10, 20 };        // co ile stron pelne odswiezenie
static const uint8_t marginVals[3] = { 8, 4, 14 };               // marginesy tekstu (px): Normal, Narrow, Wide
static const uint8_t lineGapVals[3] = { 0, 2, 4 };               // interlinia: dodatkowe px miedzy wierszami

struct Settings {
  uint8_t famIdx;      // rodzina czcionki (FAM_*)
  uint8_t fontIdx;     // indeks rozmiaru w rodzinie
  bool bold;           // pogrubienie
  bool showPct;        // procent postepu w pasku stanu
  bool showPage;       // numer strony n/N w pasku stanu
  bool showLeft;       // przewidywany czas do konca ksiazki
  bool showBar;        // pasek postepu (wypelnia reszte linii)
  uint8_t batMode;     // bateria w ksiazce: 0 wylaczona, 1 tylko gdy niski poziom, 2 zawsze
  bool fastFlip;       // szybkie przewracanie stron (eksperyment)
  uint8_t fullEvery;   // indeks w fullEveryVals
  uint8_t sleepIdx;    // indeks w sleepMinutes
  uint8_t sleepScreen; // ekran uspienia: 0 karta ksiazki, 1 obraz, 2 bez zmian
  bool autoOpen;       // po starcie otworz ostatnia ksiazke
  uint8_t margin;      // margines tekstu: 0 normalny, 1 waski, 2 szeroki (marginVals)
  uint8_t lineGap;     // interlinia: indeks w lineGapVals
  bool hyphen;         // automatyczne przenoszenie wyrazow
  uint8_t clockMode;   // zegar: 0 wylaczony, 1 24h, 2 12h
  bool clockBook;      // zegar w pasku stanu w ksiazce
  uint8_t tz;          // strefa czasowa (indeks w TZS)
  bool fastMenu;       // szybkie (bez migania) odswiezanie list w menu
  uint8_t uiFont;      // czcionka menu i naglowkow: 0 Classic, 2 Helvetica 12, 3 Unifont (1 = dawne Helvetica 14, traktowane jak 2)
  uint8_t apPower;     // moc nadawania hotspotu: 0 niska (8,5 dBm), 1 srednia (13 dBm), 2 wysoka (19,5 dBm)
  uint8_t powerSave;   // oszczedzanie energii: 0 wylaczone, 1 procesor 80 MHz, 2 lekkie uspienie w bezczynnosci
  bool battEst;        // szacowanie baterii (plytka nie ma czujnika)
  uint8_t battCap;     // pojemnosc ogniwa: indeks w BM_CAPS
};
Settings cfg = { FAM_CLASSIC, 1, false, true, true, false, true, 1, false, 1, 1, 0, true, 0, 0, true, 0, false, 0, true, 2, 1, 0, true, 2 };

// Czy pasek stanu w ogole jest rysowany (wplywa na uklad strony, wiec bateria go nie zmienia)
// Czy wlaczona jest choc jedna pozycja paska stanu (glowny przelacznik "Status line" pokazuje ten stan)
bool statusAnyOn() {
  return cfg.showPct || cfg.showPage || cfg.showLeft || cfg.showBar || cfg.clockBook || cfg.batMode > 0;
}
// Glowny przelacznik: wlacza albo wylacza wszystkie podopcje paska stanu naraz
void statusSetAll(bool on) {
  cfg.showPct = cfg.showPage = cfg.showLeft = cfg.showBar = cfg.clockBook = on;
  cfg.batMode = on ? 1 : 0;                  // 1 = Low (bateria tylko, gdy jest niska)
}

bool statusVisible() {
  return cfg.showPct || cfg.showPage || cfg.showLeft || cfg.showBar || (cfg.clockMode > 0 && cfg.clockBook);
}

// ---------- Biezacy uklad strony (ustawiany przez applyLayout) ----------
int linesPerPage = 7;
int maxTextW = SCREEN_W - 2 * MARGIN_X;   // szerokosc tekstu w pikselach
int textX0 = MARGIN_X;

void applyLayout() {
  if (cfg.famIdx >= fontFamilyCount()) cfg.famIdx = FAM_CLASSIC;
  if (cfg.fontIdx >= fontSizeCount(cfg.famIdx)) cfg.fontIdx = fontSizeCount(cfg.famIdx) - 1;   // np. dawne 24/32 px -> najwiekszy dostepny
  if (cfg.margin > 2) cfg.margin = 0;
  if (cfg.lineGap > 2) cfg.lineGap = 0;
  if (cfg.clockMode > 2) cfg.clockMode = 0;
  if (cfg.tz >= TZ_COUNT) cfg.tz = 0;
  if (cfg.uiFont == 1) cfg.uiFont = 2;            // dawne Helvetica 14 (zbyt ciasne w wierszach menu)
  if (cfg.uiFont > 3) cfg.uiFont = 0;
  if (cfg.apPower > 2) cfg.apPower = 1;
  if (cfg.powerSave > 2) cfg.powerSave = 0;
  if (cfg.battCap > 5) cfg.battCap = 2;
  fontSetLineGap(lineGapVals[cfg.lineGap]);
  fontSelect(cfg.famIdx, cfg.fontIdx, cfg.bold);

  int mx = marginVals[cfg.margin];
  int areaW = SCREEN_W - 2 * mx;
  int fw = fontFixedW();
  if (fw > 0) {                      // stala szerokosc: wysrodkuj reszte
    maxTextW = (areaW / fw) * fw;
    textX0 = mx + (areaW - maxTextW) / 2;
  } else {                           // proporcjonalna
    maxTextW = areaW;
    textX0 = mx;
  }
  int availH = (statusVisible() ? STATUS_Y : SCREEN_H) - fontTopPad();
  linesPerPage = availH / fontLineH();
  if (linesPerPage < 1) linesPerPage = 1;
}

// Podpis ukladu: ksiazka paginuje sie inaczej dla kazdej kombinacji ustawien
uint32_t layoutSig() {
  uint32_t h = 2166136261UL;
  uint32_t parts[10] = { (uint32_t)cfg.famIdx, (uint32_t)cfg.fontIdx, (uint32_t)cfg.bold,
                         (uint32_t)statusVisible(), (uint32_t)maxTextW, (uint32_t)linesPerPage,
                         (uint32_t)WRAP_VERSION, (uint32_t)fontLineH(), (uint32_t)cfg.hyphen,
                         (uint32_t)(cfg.hyphen ? HYPHEN_VERSION : 0) };
  for (int i = 0; i < 10; i++) h = (h ^ parts[i]) * 16777619UL;
  return h;
}

// ============================================================
//  Zawijanie tekstu (jedna funkcja dla indeksowania i rysowania)
//  Liczy SZEROKOSC W PIKSELACH, nie tnie sekwencji UTF-8 w polowie.
// ============================================================
inline int charBytes(uint8_t c) {
  if (c < 0x80) return 1;
  if ((c & 0xE0) == 0xC0) return 2;
  if ((c & 0xF0) == 0xE0) return 3;
  if ((c & 0xF8) == 0xF0) return 4;
  return 1;                       // zagubiony bajt kontynuacji
}

// Szerokosc (px) pierwszych len bajtow napisu
int textWidthPx(const char* t, int len) {
  int w = 0, i = 0;
  while (i < len) {
    int cb = charBytes((uint8_t)t[i]);
    if (i + cb > len) cb = len - i;
    uint32_t cp;
    gfxU8Decode(t + i, cb, cp);
    w += fontCharW(cp);
    i += cb;
  }
  return w;
}

// Probuje podzielic wyraz, ktory sie nie miesci (poczatek wyrazu: ws bajtow od pos).
// Zwraca true i ustawia outBytes (dlugosc wiersza) oraz needHyphen (czy dorysowac myslnik).
bool tryHyphenate(const char* t, int pos, int ws, int& outBytes, bool& needHyphen) {
  const char* w = t + pos + ws;
  int we = 0;
  while (we < 120) {
    char c = w[we];
    if (c == '\0' || c == ' ' || c == '\n' || c == '\r') break;
    we++;
  }
  // kandydaci: (przesuniecie od poczatku wyrazu, czy wymaga myslnika)
  uint8_t candOff[24];
  bool candHy[24];
  int nc = 0;
  int i = 0;
  while (i < we && nc < 22) {
    // segment liter: od i do pierwszego znaku niebedacego litera (-, znak interpunkcyjny...)
    int j = i;
    while (j < we) {
      uint8_t c = (uint8_t)w[j];
      int cb = charBytes(c);
      if (j + cb > we) break;
      uint32_t cp;
      gfxU8Decode(w + j, cb, cp);
      if (!hyIsLetter(hyLower(cp))) break;
      j += cb;
    }
    if (j > i) {                                   // segment [i, j)
      uint8_t pts[16];
      int np = hyphenPoints(w + i, j - i, pts, 16);
      for (int k = 0; k < np && nc < 22; k++) { candOff[nc] = (uint8_t)(i + pts[k]); candHy[nc++] = true; }
      i = j;
    }
    if (i < we) {
      if (w[i] == '-' && i + 1 < we && i > 0 && nc < 22) { candOff[nc] = (uint8_t)(i + 1); candHy[nc++] = false; }   // juz jest myslnik w tekscie
      i += charBytes((uint8_t)w[i]);
    }
  }
  int hyW = fontCharW('-');
  for (int k = nc - 1; k >= 0; k--) {
    int pw = textWidthPx(t + pos, ws + candOff[k]) + (candHy[k] ? hyW : 0);
    if (pw <= maxTextW) {
      outBytes = ws + candOff[k];
      needHyphen = candHy[k];
      return true;
    }
  }
  return false;
}

#define HY_MAX_RUN 2                  // najwyzej tyle kolejnych wierszy zakonczonych myslnikiem
int hyRun = 0;                         // biezacy ciag takich wierszy; zerowany na poczatku kazdej strony

// Rdzen zawijania: dlugosc wiersza w bajtach; hyph = po wierszu trzeba dorysowac myslnik.
int layoutLine(const char* t, int pos, int& advance, bool& hyph) {
  hyph = false;
  int bytes = 0;
  int w = 0;
  int lastSpace = -1;             // pozycja (w bajtach) ostatniej spacji

  for (;;) {
    uint8_t c = (uint8_t)t[pos + bytes];
    if (c == '\0' || c == '\n' || c == '\r') break;
    int cb = charBytes(c);
    for (int k = 1; k < cb; k++) {            // ucieta sekwencja na koncu bufora
      if (t[pos + bytes + k] == '\0') { cb = k; break; }
    }
    uint32_t cp;
    gfxU8Decode(t + pos + bytes, cb, cp);
    int cw = fontCharW(cp);
    if (w + cw > maxTextW && bytes > 0) break;   // nie miesci sie
    if (c == ' ') lastSpace = bytes;
    bytes += cb;
    w += cw;
  }

  char next = t[pos + bytes];
  // wyraz sie nie miesci: podziel go (jesli wlaczone), a w ostatecznosci przenies do nastepnej linii
  if (next != '\0' && next != '\n' && next != '\r' && next != ' ') {
    bool done = false;
    if (cfg.hyphen && hyRun < HY_MAX_RUN) {
      int ob;
      bool nh;
      if (tryHyphenate(t, pos, lastSpace >= 0 ? lastSpace + 1 : 0, ob, nh) && ob > 0) {
        bytes = ob;
        hyph = nh;
        done = true;
      }
    }
    if (!done && lastSpace > 0) bytes = lastSpace;
  }

  hyRun = hyph ? hyRun + 1 : 0;
  advance = bytes;
  while (t[pos + advance] == ' ') advance++;
  if (t[pos + advance] == '\r') advance++;
  if (t[pos + advance] == '\n') advance++;
  return bytes;
}

int lineLength(const char* t, int pos, int& advance) {
  bool hy;
  return layoutLine(t, pos, advance, hy);
}

int consumePage(const char* t) {
  int pos = 0;
  hyRun = 0;
  for (int row = 0; row < linesPerPage; row++) {
    int adv;
    lineLength(t, pos, adv);
    pos += adv;
    if (t[pos] == '\0') break;
  }
  return pos;
}

// NUL i tab -> spacja (spojne zachowanie przy indeksowaniu i rysowaniu)
void sanitizeBuf(char* b, int n) {
  for (int i = 0; i < n; i++) {
    if (b[i] == '\0' || b[i] == '\t') b[i] = ' ';
  }
}

// ============================================================
//  Narzedzia: nazwy plikow, klucze
// ============================================================
bool endsWithTxt(const char* s) {
  size_t l = strlen(s);
  return l > 4 && strcasecmp(s + l - 4, ".txt") == 0;
}

// Bezpieczna nazwa ksiazki: bez sciezek, tylko [0-9A-Za-z._-], zawsze konczy sie
// na .txt (trzon przycinany, rozszerzenie nie). Polskie litery sa
// transliterowane (ą -> a), inne niedozwolone znaki zamieniane na '_'.
// Zwraca false, gdy to nie .txt.
static bool makeNameExt(const char* in, char* out, const char* ext) {
  const char* b = in;
  const char* s = strrchr(b, '/');
  if (s) b = s + 1;
  s = strrchr(b, '\\');
  if (s) b = s + 1;

  int l = (int)strlen(b);
  if (l <= 4 || strcasecmp(b + l - 4, ext) != 0) return false;

  int end = l - 4;               // koniec trzonu (bez rozszerzenia)
  int i = 0, j = 0;
  while (i < end && j < NAME_LEN - 5) {
    uint32_t cp;
    int adv = gfxU8Decode(b + i, end - i, cp);
    if (adv < 1) adv = 1;
    i += adv;
    char c = '_';
    if (cp > 0 && cp < 128) {
      c = (char)cp;
    } else if (cp >= 128) {
      char base;
      uint8_t dia;
      gfxMapCp(cp, base, dia);
      c = base;
    }
    bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              c == '.' || c == '-' || c == '_';
    out[j++] = ok ? c : '_';
  }
  if (j == 0) out[j++] = '_';
  memcpy(out + j, ext, 5);       // razem z '\0'
  return true;
}

bool makeBookName(const char* in, char* out) {
  return makeNameExt(in, out, ".txt");
}
// Spis tresci (rozdzialy) ksiazki: ta sama nazwa, rozszerzenie .toc (tworzy go strona WWW przy imporcie EPUB)
bool makeTocName(const char* in, char* out) {
  return makeNameExt(in, out, ".toc");
}
// "/Nazwa.toc" dla ksiazki "Nazwa.txt"
void tocPathFor(const char* bookName, char* out, size_t n) {
  size_t l = strlen(bookName);
  if (l > 4 && l + 1 < n) {
    snprintf(out, n, "/%.*s.toc", (int)(l - 4), bookName);
  } else {
    out[0] = 0;
  }
}

const char* baseName(const char* p) {
  const char* s = strrchr(p, '/');
  return s ? s + 1 : p;
}

// nazwa bez .txt, przycieta do maxChars
void shortName(const char* name, char* out, int maxChars) {
  int l = (int)strlen(name);
  if (endsWithTxt(name)) l -= 4;
  if (l > maxChars) l = maxChars;
  memcpy(out, name, l);
  out[l] = '\0';
}

uint32_t nameHash(const char* name) {
  uint32_t h = 5381;
  for (const char* p = name; *p; p++) h = h * 33 + (uint8_t)*p;
  return h;
}

// Klucz zakladki: nazwa + rozmiar (podmieniona ksiazka nie dziedziczy starej pozycji)
void bmKey(const char* name, uint32_t size, char* key) {
  uint32_t h = nameHash(name) ^ (size * 2654435761UL);
  snprintf(key, 16, "o%08lx", (unsigned long)h);
}

// Plik z indeksem: /i<hash nazwy>_<podpis ukladu>.idx (rozmiar ksiazki sprawdza naglowek)
void idxPath(const char* name, uint32_t sig, char* path, size_t n) {
  snprintf(path, n, "/i%08lx_%08lx.idx", (unsigned long)nameHash(name), (unsigned long)sig);
}

// Usuwa wszystkie pliki indeksu danej ksiazki (dla kazdego ukladu strony)
void removeAllIdx(const char* name) {
  char prefix[16];
  snprintf(prefix, sizeof(prefix), "i%08lx_", (unsigned long)nameHash(name));
  size_t pl = strlen(prefix);

  for (int pass = 0; pass < 6; pass++) {
    char victims[8][28];
    int vn = 0;
    File root = LittleFS.open("/");
    if (!root) return;
    File f = root.openNextFile();
    while (f && vn < 8) {
      const char* n = baseName(f.name());
      if (strncmp(n, prefix, pl) == 0 && strlen(n) < 26) {
        snprintf(victims[vn++], 28, "/%s", n);
      }
      f.close();
      f = root.openNextFile();
    }
    if (f) f.close();
    root.close();
    if (vn == 0) return;
    for (int i = 0; i < vn; i++) LittleFS.remove(victims[i]);
  }
}

// ============================================================
//  Stan aplikacji
// ============================================================
enum Screen { S_MENU, S_LIBRARY, S_READER, S_UPLOAD, S_INFO, S_GOTO, S_SETTINGS, S_STATS, S_PINSCAN, S_SETTIME, S_SETBATT };
Screen screen = S_MENU;
bool fsOk = false;
Preferences prefs;

// ---------- Hotspot: nazwa i haslo ----------
// Nazwa sieci: InkBlink-XXXX (koncowka z unikalnego adresu plytki)
void apSsid(char* out, size_t n) {
  uint64_t mac = ESP.getEfuseMac();
  snprintf(out, n, "%s%04X", AP_SSID_PREFIX, (unsigned)((mac >> 32) & 0xFFFF));
}

// Haslo: 8 cyfr, losowane RAZ i zapamietane - telefon zapamieta siec i nastepnym razem
// polaczy sie sam. Mozna je wymienic w Ustawieniach.
String apPasswordNew() {
  char b[10];
  snprintf(b, sizeof(b), "%08lu", (unsigned long)random(100000000L));
  prefs.putString("ap_pw", b);
  return String(b);
}
String apPassword() {
  String p = prefs.getString("ap_pw", "");
  if (p.length() != 8) p = apPasswordNew();
  return p;
}

// ---------- Menu ----------
enum MenuId { M_CONTINUE, M_LIBRARY, M_UPLOAD, M_STATS, M_SETTINGS, M_INFO };
MenuId menuIds[6];
const char* menuLabels[6];
int menuCount = 0;
int menuSel = 0;

// ---------- Biblioteka ----------
char bookNames[MAX_BOOKS][NAME_LEN];
int bookCount = 0;
int libSel = 0;

// ---------- Czytnik ----------
File book;
uint32_t bookSize = 0;
char curBook[NAME_LEN];
uint32_t* pageOff = nullptr;
int maxPages = 0;
int totalPages = 0;
int page = 0;
bool indexTruncated = false;
bool bmDirty = false;
unsigned long lastTurnMs = 0;

// ---------- Upload ----------
WebServer server(80);
File uploadFile;
bool uploadOk = false;
bool uploadDone = false;
char uploadName[NAME_LEN];
bool uploadIsToc = false;          // wysylany plik to spis tresci (.toc), a nie ksiazka
uint32_t uploadBytes = 0;
char uploadPath[NAME_LEN + 2];
char lastUploaded[NAME_LEN];
String uploadErr;

// ---------- Statystyki czytania ----------
BookStats bst;
TotalStats tst;
unsigned long statLastTurn = 0;   // moment ostatniego wyswietlenia strony (0 = nie czytamy)
int statTurns = 0;

// ---------- Bateria ----------
int batMv = -1;
unsigned long batAt = 0;

// ---------- Szybkie przewracanie ----------
int fastLeft = 0;                 // ile szybkich odswiezen zostalo do pelnego

// ---------- Upload: krok ekranu (0 = dolacz do sieci, 1 = otworz strone) ----------
int uploadStep = 0;

// ---------- Obraz uspienia: wysylanie ----------
File wpFile;
bool wpOk = false;
String wpErr;
size_t wpBytes = 0;

// ---------- Diagnostyka hotspotu ----------
volatile bool statusDirty = false;     // cos sie zmienilo: odswiez licznik na ekranie
int httpHits = 0;                      // ile zadan HTTP dotarlo do czytnika
unsigned long lastUploadDraw = 0;

// ============================================================
//  Ekrany pomocnicze
// ============================================================
void showMessage(const char* a, const char* b = nullptr, const char* c = nullptr) {
  gfxClear();
  gfxText(UI_X, 30, a, FONT_SIZE);
  if (b) gfxText(UI_X, 52, b, FONT_SIZE);
  if (c) gfxText(UI_X, 74, c, FONT_SIZE);
  gfxPresent();
}

// Legenda klawiszy przy prawej krawedzi, na wysokosci fizycznych klawiszy:
//   top = klawisz HOME (MENU), mid = klik pokretla (OK), bot = klawisz EXIT.
// Pusty napis = klawisz nic tu nie robi (albo robi to samo co inny, wiec nie podpisujemy).
// arrows = trojkaty nad i pod srodkowa etykieta (pokretlo przewija gora/dol).
void drawLegend(const char* top, const char* mid, const char* bot, bool arrows) {
  int midTop = (SCREEN_H - TAG_H) / 2;
  if (top && top[0]) gfxTag(0, top);
  if (mid && mid[0]) gfxTag(midTop, mid);
  if (bot && bot[0]) gfxTag(SCREEN_H - TAG_H, bot);
  if (arrows) {
    int cx = SCREEN_W - TAG_COL_W / 2;
    gfxArrow(cx, midTop - 9, true);
    gfxArrow(cx, midTop + TAG_H + 3, false);
  }
}

// Czy menu i naglowki rysujemy czcionka U8g2 (ustawienie "Menu font"; bez U8g2 zawsze Classic)
#if USE_U8G2
inline bool uiHelvOn() { return cfg.uiFont >= 1 && cfg.uiFont <= 3; }
#else
inline bool uiHelvOn() { return false; }
#endif

// Naglowek ekranu: pogrubiony w Helvetice; w Unifont i Classic zwykly (pogrubianie gestych czcionek
// zlewalo "M" i "W" w czarny klocek).
void drawTitle(int x, const char* t) {
#if USE_U8G2
  if (uiHelvOn()) {
    uiText(cfg.uiFont, x, 0, t, true);
    return;
  }
#endif
  gfxText(x, 0, t, FONT_SIZE);
}

// Rysuje liste do bufora (bez wysylania na ekran)
void drawListBody(const char* title, ItemFn item, int count, int sel,
                  const char* top, const char* mid, const char* bot) {
  gfxClear();
  drawTitle(UI_X, title);

  int top0 = 0;
  if (sel >= LIST_ROWS) top0 = sel - LIST_ROWS + 1;

  for (int r = 0; r < LIST_ROWS && top0 + r < count; r++) {
#if USE_U8G2
    if (uiHelvOn()) {
      if (top0 + r == sel) gfxText(UI_X, 16 + r * 16, ">", FONT_SIZE);          // kursor
      char txt[UI_CHARS + 1];
      snprintf(txt, sizeof(txt), "%s", item(top0 + r));
      int maxW = SCREEN_W - TAG_COL_W - (UI_X + 16) - 2;                         // przed kolumna legendy
      int n = (int)strlen(txt);
      while (n > 0 && uiWidth(cfg.uiFont, txt, false) > maxW) {                          // zabezpieczenie: nie wchodz pod legende
        txt[--n] = '\0';
        while (n > 0 && ((uint8_t)txt[n] & 0xC0) == 0x80) txt[--n] = '\0';
      }
      uiText(cfg.uiFont, UI_X + 16, 16 + r * 16, txt, false);
      continue;
    }
#endif
    char line[UI_CHARS + 1];
    snprintf(line, sizeof(line), "%c %s", (top0 + r == sel) ? '>' : ' ', item(top0 + r));
    gfxText(UI_X, 16 + r * 16, line, FONT_SIZE);
  }

  drawLegend(top, mid, bot, true);
}

// Odswiezenie ekranow z lista. Przy zmianie samego zaznaczenia (listMoveOnly) robimy szybkie,
// czesciowe odswiezenie bez migania; wejscie na ekran i co MENU_FULL_EVERY ruchow - pelne.
#define MENU_FULL_EVERY 10
bool listMoveOnly = false;
int menuFastLeft = 0;
void presentList() {
  bool fast = listMoveOnly && cfg.fastMenu && gfxFastReady() && menuFastLeft > 0;
  listMoveOnly = false;
  if (fast) {
    gfxPresentFast();
    menuFastLeft--;
  } else {
    gfxPresent();
    menuFastLeft = MENU_FULL_EVERY - 1;
  }
}

void drawList(const char* title, ItemFn item, int count, int sel,
              const char* top, const char* mid, const char* bot) {
  drawListBody(title, item, count, sel, top, mid, bot);
  presentList();
}

// ============================================================
//  Czas: formatowanie
// ============================================================
// "1h20m", "35m" albo "<1m"
void fmtDuration(uint32_t s, char* out, size_t n) {
  uint32_t m = s / 60, h = m / 60;
  m %= 60;
  if (h > 0) snprintf(out, n, "%uh%02um", (unsigned)h, (unsigned)m);
  else if (s >= 60) snprintf(out, n, "%um", (unsigned)m);
  else snprintf(out, n, "<1m");
}

// "12h 34m" / "34m" / "0m" (do ekranu statystyk)
void fmtDurationLong(uint32_t s, char* out, size_t n) {
  uint32_t m = s / 60, h = m / 60;
  m %= 60;
  if (h > 0) snprintf(out, n, "%uh %um", (unsigned)h, (unsigned)m);
  else snprintf(out, n, "%um", (unsigned)m);
}

// ============================================================
//  Oszczedzanie energii (ustawienie "Power saving")
//   0 = procesor 240 MHz, 1 = 80 MHz (hotspot 160 MHz, indeksowanie ksiazki 240 MHz),
//   2 = jak 1 + lekkie uspienie w bezczynnosci (wybudzenie przyciskiem), tylko poza hotspotem.
// ============================================================
static int cpuMhzCur = 240;
static void cpuSet(int mhz) {
  if (mhz != cpuMhzCur) {
    setCpuFrequencyMhz(mhz);
    cpuMhzCur = mhz;
  }
}
void powerApply() {
  int want = 240;
  if (cfg.powerSave > 0) want = (screen == S_UPLOAD) ? 160 : 80;
  cpuSet(want);
}
// Lekkie uspienie w menu dopiero po chwili bez przyciskow (przewijanie bez opoznien wybudzania).
// W ksiazce od razu: tam lagu nie bylo, a 1,5 s na 80 MHz po kazdej stronie podwajalo jej koszt.
#define PS_LIGHT_GRACE_MS 1500
extern unsigned long lastActivityMs;
static void idleWait() {
  unsigned long grace = (screen == S_READER) ? 0 : PS_LIGHT_GRACE_MS;
  if (cfg.powerSave == 2 && screen != S_UPLOAD && millis() - lastActivityMs >= grace &&
      boardReadKey() == KEY_NONE) boardLightSleep(250);
  else delay(5);
}

// ============================================================
//  Szacowanie baterii (patrz battery_model.h)
// ============================================================
BatModel bm;
static uint32_t bmLastMs = 0, bmLastSave = 0, bmPrevFull = 0, bmPrevFast = 0;

bool batteryIsEstimate() {
  return PIN_BAT_ADC < 0 && cfg.battEst && bm.valid;
}
static const char* batTilde() {
  return batteryIsEstimate() ? "~" : "";
}

// ============================================================
//  Bateria
// ============================================================
bool batteryAvailable() {
  return PIN_BAT_ADC >= 0 || batteryIsEstimate();
}

// Przyblizony poziom naladowania ogniwa Li-Po z napiecia (mV)
int batteryPercentFromMv(int mv) {
  static const int16_t T[10][2] = {
    { 4200, 100 }, { 4100, 90 }, { 4000, 78 }, { 3900, 62 }, { 3800, 45 },
    { 3700, 28 }, { 3600, 15 }, { 3500, 7 }, { 3400, 3 }, { 3300, 0 }
  };
  if (mv >= T[0][0]) return 100;
  for (int i = 1; i < 10; i++) {
    if (mv >= T[i][0]) {
      int v0 = T[i - 1][0], p0 = T[i - 1][1], v1 = T[i][0], p1 = T[i][1];
      return p1 + (mv - v1) * (p0 - p1) / (v0 - v1);
    }
  }
  return 0;
}

// Napiecie baterii w mV z pamieci podrecznej (odswiezane co 30 s); -1 = brak pomiaru
int batteryMv() {
  if (PIN_BAT_ADC < 0) return -1;             // bez czujnika nie ma napiecia, tylko szacunek procentowy
  if (screen == S_UPLOAD) return batMv;       // przy WiFi ADC2 nie dziala - uzywamy ostatniego odczytu
  if (batMv < 0 || millis() - batAt > 30000UL) {
    batMv = boardBatteryMv();
    batAt = millis();
  }
  return batMv;
}

int batteryPercent() {
  if (PIN_BAT_ADC < 0) return batteryIsEstimate() ? (int)(bmPercent(bm) + 0.5f) : -1;
  int mv = batteryMv();
  return mv < 0 ? -1 : batteryPercentFromMv(mv);
}

// ---- zapis, aktualizacja i log modelu ----
void bmSave() {
  prefs.putBool("bm_ok", bm.valid);
  prefs.putFloat("bm_rem", bm.remMah);
  prefs.putFloat("bm_as", bm.awakeS);
  prefs.putFloat("bm_hs", bm.hotspotS);
  prefs.putFloat("bm_ss", bm.sleepS);
  prefs.putUInt("bm_ff", bm.flipsFast);
  prefs.putUInt("bm_fl", bm.flipsFull);
}
void bmLoad() {
  bmReset(bm, BM_CAPS[cfg.battCap % 6]);
  bm.valid = prefs.getBool("bm_ok", false);
  bm.remMah = prefs.getFloat("bm_rem", 0);
  bm.awakeS = prefs.getFloat("bm_as", 0);
  bm.hotspotS = prefs.getFloat("bm_hs", 0);
  bm.sleepS = prefs.getFloat("bm_ss", 0);
  bm.flipsFast = prefs.getUInt("bm_ff", 0);
  bm.flipsFull = prefs.getUInt("bm_fl", 0);
  if (bm.remMah > bm.capMah) bm.remMah = bm.capMah;
  bmLastMs = millis();
  bmPrevFull = g_presFull;
  bmPrevFast = g_presFast;
}
// Zmiana pojemnosci w ustawieniach: zachowaj procent
void bmCapSync() {
  float pct = bmPercent(bm);
  bm.capMah = BM_CAPS[cfg.battCap % 6];
  if (bm.valid) bm.remMah = bm.capMah * pct / 100.0f;
}
// Wolane w kazdej iteracji petli: dolicza czas pracy i odswiezenia ekranu od poprzedniego razu
void bmUpdate() {
  uint32_t now = millis();
  uint32_t dt = now - bmLastMs;
  bmLastMs = now;
  uint32_t df = g_presFull - bmPrevFull, dfa = g_presFast - bmPrevFast;
  bmPrevFull = g_presFull;
  bmPrevFast = g_presFast;
  if (!bm.valid) return;
  bmAddRun(bm, screen == S_UPLOAD, cfg.powerSave, dt / 1000.0f);
  bmAddFlips(bm, df, dfa);
  if (now - bmLastSave > 600000UL) {
    bmLastSave = now;
    bmSave();
  }
}
// Po wybudzeniu z gleboki snu: dolicz czas snu (wymaga ustawionego zegara przed snem i po nim)
void bmBootSleepAccount() {
  uint32_t t0 = prefs.getUInt("bm_slp", 0);
  if (t0 && esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED && clockValid() && bm.valid) {
    long d = (long)clockNow() - (long)t0;
    if (d > 0 && d < 60L * 86400L) bmAddSleep(bm, (float)d);
  }
  prefs.putUInt("bm_slp", 0);
}
// Dopisuje wiersz do /battlog.csv (liczniki od poprzedniego ustawienia + stan szacunku przed i po)
static void bmLog(const char* note, float volt, float pctBefore, float pctAfter) {
  if (!fsOk) return;
  File r = LittleFS.open("/battlog.csv", "r");
  size_t sz = r ? r.size() : 0;
  if (r) r.close();
  if (sz > 12000) {                          // log nie rosnie bez konca
    LittleFS.remove("/battlog.csv");
    sz = 0;
  }
  File f = LittleFS.open("/battlog.csv", "a");
  if (!f) return;
  char line[200];
  if (sz == 0) {
    const char* hd = "time,volt,pct_from_volt,est_before,est_after,awake_h,hotspot_h,sleep_h,flips_fast,flips_full,power_save,capacity_mah,note\n";
    f.write((const uint8_t*)hd, strlen(hd));
  }
  char tm[24], d[12], t2[12];
  if (clockValid() && clockFormatDate(d, sizeof(d)) && clockFormat(t2, sizeof(t2), 1)) snprintf(tm, sizeof(tm), "%s %s", d, t2);
  else snprintf(tm, sizeof(tm), "up%lus", (unsigned long)(millis() / 1000));
  snprintf(line, sizeof(line), "%s,%.2f,%.1f,%.1f,%.1f,%.3f,%.3f,%.3f,%u,%u,%d,%d,%s\n", tm, volt, bmPctFromVolt(volt), pctBefore, pctAfter,
           bm.awakeS / 3600.0f, bm.hotspotS / 3600.0f, bm.sleepS / 3600.0f, (unsigned)bm.flipsFast, (unsigned)bm.flipsFull,
           (int)cfg.powerSave, (int)bm.capMah, note);
  f.write((const uint8_t*)line, strlen(line));
  f.close();
}
void bmSetVoltage(float v) {
  float before = bm.valid ? bmPercent(bm) : -1;
  float pct = bmPctFromVolt(v);
  bmLog("voltage", v, before, pct);
  bmSetPercent(bm, pct);
  bmSave();
}
void bmMarkFull() {
  float before = bm.valid ? bmPercent(bm) : -1;
  bmLog("full", 4.20f, before, 100);
  bmSetPercent(bm, 100);
  bmSave();
}
void bmClear() {
  bmReset(bm, BM_CAPS[cfg.battCap % 6]);
  bmSave();
}

// Prawy gorny rog: zegar (jesli wlaczony) i bateria ("87%" + ikona)
void drawBatteryCorner() {
  int right = SCREEN_W - 4;                      // prawa krawedz zegara
  int pct = batteryPercent();
  if (pct >= 0) {
    char t[10];
    snprintf(t, sizeof(t), "%s%d%%", batTilde(), pct);
    uiBattery(SCREEN_W - 24, 2, pct);
    int tx = SCREEN_W - 24 - 4 - gfxTextWidth(t, FOOT_SIZE);
    gfxText(tx, 1, t, FOOT_SIZE);
    right = tx - 8;
  }
  if (cfg.clockMode > 0) {
    char c[12];
    if (!clockFormat(c, sizeof(c), cfg.clockMode)) snprintf(c, sizeof(c), "--:--");   // czas jeszcze nie ustawiony
    gfxText(right - gfxTextWidth(c, FOOT_SIZE), 1, c, FOOT_SIZE);
  }
}

// ============================================================
//  Statystyki czytania
// ============================================================
int readPercent();                     // (zdefiniowana w sekcji Czytnik)
bool getLastBook(char* out, size_t n); // (zdefiniowana w sekcji Menu)

void statKey(const char* name, uint32_t size, char* key) {
  uint32_t h = nameHash(name) ^ (size * 2246822519UL);
  snprintf(key, 16, "s%08lx", (unsigned long)h);
}

void statsLoadTotals() {
  memset(&tst, 0, sizeof(tst));
  if (prefs.isKey("st_tot") && prefs.getBytes("st_tot", &tst, sizeof(tst)) != sizeof(tst)) memset(&tst, 0, sizeof(tst));
}

BookStats statsReadBook(const char* name, uint32_t size) {
  BookStats b;
  memset(&b, 0, sizeof(b));
  char key[16];
  statKey(name, size, key);
  if (prefs.isKey(key) && prefs.getBytes(key, &b, sizeof(b)) != sizeof(b)) memset(&b, 0, sizeof(b));
  return b;
}

void statsLoadBook() {
  bst = statsReadBook(curBook, bookSize);
  statLastTurn = 0;
  statTurns = 0;
}

void statsSave() {
  if (curBook[0]) {
    char key[16];
    statKey(curBook, bookSize, key);
    prefs.putBytes(key, &bst, sizeof(bst));
  }
  prefs.putBytes("st_tot", &tst, sizeof(tst));
  statTurns = 0;
}

// Wolac po wyswietleniu strony w czytniku (zaczyna liczyc czas)
void statsTouch() {
  statLastTurn = millis();
}

// Wolac przy zmianie strony (po ustawieniu nowej wartosci page)
void statsOnTurn(int fromPage, int toPage) {
  unsigned long now = millis();
  if (statLastTurn) {
    uint32_t dt = (uint32_t)((now - statLastTurn) / 1000UL);
    if (dt > 0 && dt <= STATS_MAX_GAP_S) {
      bst.secs += dt;
      tst.secs += dt;
    }
    // tempo liczymy tylko ze zwyklego przewrocenia o jedna strone
    if (toPage == fromPage + 1 && dt >= STATS_MIN_TURN_S && dt <= STATS_MAX_GAP_S) {
      uint32_t bytes = pageOff[toPage] - pageOff[fromPage];
      bst.spdBytes += bytes;
      bst.spdSecs += dt;
      bst.pages++;
      tst.spdBytes += bytes;
      tst.spdSecs += dt;
      tst.pages++;
    }
  }
  statLastTurn = now;
  if (!bst.done && readPercent() >= 98) {
    bst.done = 1;
    tst.finished++;
  }
  if (++statTurns >= STATS_SAVE_EVERY) statsSave();
}

// Konczy sesje czytania (wyjscie z czytnika, uspienie): dolicza ostatnia strone i zapisuje
void statsFlush() {
  if (!statLastTurn || curBook[0] == '\0') return;
  uint32_t dt = (uint32_t)((millis() - statLastTurn) / 1000UL);
  if (dt <= STATS_MAX_GAP_S) {
    bst.secs += dt;
    tst.secs += dt;
  }
  statLastTurn = 0;
  statsSave();
}

// Przewidywany czas do konca (s) z tempa tej ksiazki, a gdy go jeszcze brak - ze sredniego tempa; -1 = nie wiadomo
long statsLeftSecondsFor(const BookStats& b, uint32_t bytesLeft) {
  double rate = 0;
  if (b.spdSecs >= 30 && b.spdBytes >= 800) rate = (double)b.spdBytes / b.spdSecs;
  else if (tst.spdSecs >= 60 && tst.spdBytes >= 2000) rate = (double)tst.spdBytes / tst.spdSecs;
  if (rate <= 0) return -1;
  double secs = bytesLeft / rate;
  if (secs > 99.0 * 3600.0) return -1;
  return (long)secs;
}

long statsLeftSeconds() {
  if (totalPages <= 0 || !pageOff) return -1;
  uint32_t off = (page >= totalPages - 1) ? bookSize : pageOff[page];
  return statsLeftSecondsFor(bst, bookSize > off ? bookSize - off : 0);
}

// Ostatnia ksiazka (nazwa, rozmiar, pozycja zakladki) bez otwierania jej do czytania
bool lastBookInfo(char* name, uint32_t& size, uint32_t& off) {
  if (!getLastBook(name, NAME_LEN)) return false;
  char path[NAME_LEN + 2];
  snprintf(path, sizeof(path), "/%s", name);
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  size = f.size();
  f.close();
  char key[16];
  bmKey(name, size, key);
  off = prefs.getUInt(key, 0);
  return true;
}

// ============================================================
//  Ekran uspienia: karta ksiazki albo obraz
// ============================================================
// Obraz wgrany przez przegladarke (/wallpaper.bin): 250x122, 1 bit, 1 = tusz
bool drawWallpaperFile() {
  File f = LittleFS.open(WALLPAPER_PATH, "r");
  if (!f) return false;
  if ((size_t)f.size() != WALLPAPER_BYTES) {
    f.close();
    return false;
  }
  static uint8_t wp[WALLPAPER_BYTES];
  size_t n = f.read(wp, WALLPAPER_BYTES);
  f.close();
  if (n != WALLPAPER_BYTES) return false;
  for (int y = 0; y < SCREEN_H; y++)
    for (int x = 0; x < SCREEN_W; x++)
      if (wp[y * 32 + (x >> 3)] & (0x80 >> (x & 7))) gfxInk(x, y);
  return true;
}

// Wbudowany obraz: ksiezyc, gwiazdki i "Zzz"
void drawDefaultWallpaper() {
  uiDisc(196, 44, 26, true);                  // sierp ksiezyca = tarcza minus przesunieta tarcza
  uiDisc(208, 36, 23, false);
  static const int8_t stars[8][3] = {         // x, y, wielkosc
    { 30, 18, 2 }, { 74, 36, 1 }, { 120, 14, 2 }, { 150, 52, 1 },
    { 236, 22, 1 }, { 148, 28, 1 }, { 96, 70, 1 }, { 232, 90, 2 }
  };
  for (int i = 0; i < 8; i++) {
    int x = stars[i][0], y = stars[i][1], r = stars[i][2];
    for (int d = -r; d <= r; d++) {
      gfxInk(x + d, y);
      gfxInk(x, y + d);
    }
  }
  gfxText(34, 52, "Z", 32);
  gfxText(58, 40, "z", 24);
  gfxText(76, 30, "z", 16);
  gfxText(6, 108, "OK = wake up", FOOT_SIZE);
}

// Tytul z nazwy pliku: bez .txt, '_' -> spacja, zawiniety do dwoch wierszy
void titleLines(const char* fileName, char* l1, char* l2, int maxChars) {
  char t[NAME_LEN];
  shortName(fileName, t, NAME_LEN - 1);
  for (char* p = t; *p; p++)
    if (*p == '_') *p = ' ';
  int len = (int)strlen(t);
  l1[0] = l2[0] = '\0';
  if (len <= maxChars) {
    strcpy(l1, t);
    return;
  }
  int cut = maxChars;
  for (int i = maxChars; i > 0; i--) {
    if (t[i] == ' ') { cut = i; break; }
  }
  memcpy(l1, t, cut);
  l1[cut] = '\0';
  const char* rest = t + cut;
  while (*rest == ' ') rest++;
  if ((int)strlen(rest) <= maxChars) {
    strcpy(l2, rest);
  } else {
    memcpy(l2, rest, maxChars - 3);
    strcpy(l2 + maxChars - 3, "...");
  }
}

// Karta biezacej (albo ostatniej) ksiazki. false = nie ma zadnej ksiazki.
bool drawSleepBookCard() {
  char name[NAME_LEN];
  uint32_t size = 0, off = 0;
  int pageNo = 0, pages = 0;
  BookStats b;
  bool inReader = (totalPages > 0 && curBook[0] != '\0' && book);

  if (inReader) {
    strcpy(name, curBook);
    size = bookSize;
    off = pageOff[page];
    pageNo = page + 1;
    pages = totalPages;
    b = bst;
  } else {
    if (!lastBookInfo(name, size, off)) return false;
    b = statsReadBook(name, size);
  }

  int pct = 0;
  if (inReader && page >= totalPages - 1) pct = 100;
  else if (size > 0) pct = (int)(((uint64_t)off * 100) / size);
  long left = statsLeftSecondsFor(b, size > off ? size - off : 0);

  char l1[48], l2[48], t[48];
  titleLines(name, l1, l2, 29);

  gfxText(6, 6, "Currently reading", FOOT_SIZE);
  gfxText(6, 22, l1, FONT_SIZE);
  if (l2[0]) gfxText(6, 40, l2, FONT_SIZE);

  uiRect(6, 64, 243, 73);                         // pasek postepu
  int fill = pct * 236 / 100;
  if (fill > 0) uiFillRect(7, 65, 6 + fill, 72);

  if (pages > 0) snprintf(t, sizeof(t), "%d%%   page %d of %d", pct, pageNo, pages);
  else snprintf(t, sizeof(t), "%d%%", pct);
  gfxText(6, 80, t, FONT_SIZE);

  if (left >= 0) {
    char d[16];
    fmtDuration((uint32_t)left, d, sizeof(d));
    snprintf(t, sizeof(t), "~%s left", d);
    gfxText(6, 98, t, FOOT_SIZE);
  }
  gfxText(6, 110, "OK = wake up", FOOT_SIZE);

  int bp = batteryPercent();
  if (bp >= 0) {
    char bt[10];
    snprintf(bt, sizeof(bt), "%s%d%%", batTilde(), bp);
    uiBattery(SCREEN_W - 28, 108, bp);
    gfxText(SCREEN_W - 28 - 4 - gfxTextWidth(bt, FOOT_SIZE), 108, bt, FOOT_SIZE);
  }
  return true;
}

void drawSleepScreen() {
  gfxClear();
  bool drawn = false;
  if (cfg.sleepScreen == 0) drawn = drawSleepBookCard();
  if (!drawn && !drawWallpaperFile()) drawDefaultWallpaper();
  gfxPresent();
}

// ============================================================
//  Biblioteka
// ============================================================
void scanBooks() {
  bookCount = 0;
  if (!fsOk) return;

  File root = LittleFS.open("/");
  if (!root) return;

  File f = root.openNextFile();
  while (f && bookCount < MAX_BOOKS) {
    if (!f.isDirectory()) {
      const char* n = baseName(f.name());
      if (endsWithTxt(n) && strlen(n) < NAME_LEN) {
        strcpy(bookNames[bookCount++], n);
      }
    }
    f.close();
    f = root.openNextFile();
  }
  if (f) f.close();
  root.close();

  // sortowanie alfabetyczne
  for (int i = 1; i < bookCount; i++) {
    char tmp[NAME_LEN];
    strcpy(tmp, bookNames[i]);
    int j = i - 1;
    while (j >= 0 && strcasecmp(bookNames[j], tmp) > 0) {
      strcpy(bookNames[j + 1], bookNames[j]);
      j--;
    }
    strcpy(bookNames[j + 1], tmp);
  }
}

char itemBuf[40];
const char* libItem(int i) {
  shortName(bookNames[i], itemBuf, UI_CHARS - 2);
  for (char* p = itemBuf; *p; p++)
    if (*p == '_') *p = ' ';
  return itemBuf;
}

void drawLibrary() {
  if (bookCount == 0) {
    gfxClear();
    drawTitle(UI_X, "Library (0)");
    gfxText(UI_X, 40, "No books yet.", FONT_SIZE);
    gfxText(UI_X, 60, "Use Upload in the menu.", FONT_SIZE);
    drawLegend("", "", "BACK", false);
    gfxPresent();
    return;
  }
  char title[24];
  snprintf(title, sizeof(title), "Library (%d)", bookCount);
  drawList(title, libItem, bookCount, libSel, "", "OPEN", "BACK");
}

void goLibrary() {
  scanBooks();
  if (libSel >= bookCount) libSel = bookCount - 1;
  if (libSel < 0) libSel = 0;
  screen = S_LIBRARY;
  drawLibrary();
}

// ============================================================
//  Czytnik
// ============================================================
void saveBookmark() {
  bmDirty = false;
  if (totalPages <= 0 || !pageOff || curBook[0] == '\0') return;
  char key[16];
  bmKey(curBook, bookSize, key);
  prefs.putUInt(key, pageOff[page]);
  prefs.putString("last", curBook);
}

// ---------- Spis tresci (rozdzialy) ----------
uint32_t chOff[MAX_CH];            // offsety poczatkow rozdzialow (bajty w pliku ksiazki)
char chTitle[MAX_CH][32];
int chCount = 0;

// obcina napis do maxBytes bajtow, nie rozcinajac znaku UTF-8
void utf8Trunc(char* s, int maxBytes) {
  if ((int)strlen(s) <= maxBytes) return;
  int n = maxBytes;
  while (n > 0 && ((uint8_t)s[n] & 0xC0) == 0x80) n--;
  s[n] = '\0';
}
// obcina napis do maxChars znakow
void utf8CutChars(char* s, int maxChars) {
  int i = 0, ch = 0;
  while (s[i]) {
    if (ch == maxChars) { s[i] = '\0'; return; }
    i += charBytes((uint8_t)s[i]);
    ch++;
  }
}

// Dodaje jeden wiersz "offset<TAB>tytul" do spisu (bledne i nierosnace pomija)
static void tocAddLine(char* line, uint32_t bookBytes) {
  char* tab = strchr(line, '\t');
  if (!tab || chCount >= MAX_CH) return;
  *tab = '\0';
  char* e;
  unsigned long off = strtoul(line, &e, 10);
  if (e == line || *e != '\0' || off >= bookBytes) return;
  if (chCount > 0 && off <= chOff[chCount - 1]) return;
  chOff[chCount] = (uint32_t)off;
  strncpy(chTitle[chCount], tab + 1, 31);
  chTitle[chCount][31] = '\0';
  utf8Trunc(chTitle[chCount], 31);
  chCount++;
}

void loadToc(const char* bookName, uint32_t bookBytes) {
  chCount = 0;
  char p[NAME_LEN + 3];
  tocPathFor(bookName, p, sizeof(p));
  if (!p[0]) return;
  File f = LittleFS.open(p, "r");
  if (!f) return;
  if (f.size() > TOC_MAX_BYTES) { f.close(); return; }
  char line[100];
  int ll = 0;
  bool skipRest = false;
  uint8_t chunk[128];
  for (;;) {
    int n = f.read(chunk, sizeof(chunk));
    if (n <= 0) break;
    for (int i = 0; i < n; i++) {
      char c = (char)chunk[i];
      if (c == '\n') {
        line[ll] = '\0';
        tocAddLine(line, bookBytes);
        ll = 0;
        skipRest = false;
      } else if (c != '\r' && !skipRest) {
        if (ll < (int)sizeof(line) - 1) line[ll++] = c;
        else skipRest = true;                // zbyt dlugi wiersz: reszte ignorujemy
      }
    }
  }
  if (ll > 0) {                              // ostatni wiersz bez konca linii
    line[ll] = '\0';
    tocAddLine(line, bookBytes);
  }
  f.close();
}

// rozdzial, w ktorym lezy dany offset w pliku (0 gdy przed pierwszym)
int chapterOfOffset(uint32_t off) {
  int r = 0;
  for (int i = 0; i < chCount; i++) {
    if (chOff[i] <= off) r = i;
    else break;
  }
  return r;
}

int findPage(uint32_t off);   // (zdefiniowana ponizej, w sekcji ksiazki)

// Rozdzial strony (numer od 0): ten, ktory zaczyna sie na tej stronie albo wczesniej - liczony po
// KONCU strony, wiec strona z poczatkiem rozdzialu w polowie nalezy juz do nowego rozdzialu.
int chapterOfPage(int p) {
  uint32_t endOff = (p + 1 < totalPages) ? pageOff[p + 1] - 1 : bookSize;
  return chapterOfOffset(endOff);
}
int chapterStartPage(int ci) {       // numer strony (od 1), na ktorej zaczyna sie rozdzial
  return findPage(chOff[ci]) + 1;
}

void closeBook() {
  statsFlush();                       // dolicz czas czytania i zapisz statystyki
  if (book) book.close();
  if (pageOff) {
    free(pageOff);
    pageOff = nullptr;
  }
  maxPages = 0;
  totalPages = 0;
  chCount = 0;
}

// Alokuje tablice offsetow (PSRAM, jesli jest; inaczej zwykla sterta).
bool allocIndex(int want) {
  if (pageOff) {
    free(pageOff);
    pageOff = nullptr;
  }
  maxPages = 0;
  if (want < 16) want = 16;
  if (want > MAX_PAGES_CAP) want = MAX_PAGES_CAP;
  while (want >= 16) {
    pageOff = (uint32_t*)(psramFound() ? ps_malloc((size_t)want * 4) : malloc((size_t)want * 4));
    if (pageOff) {
      maxPages = want;
      return true;
    }
    want /= 2;
  }
  return false;
}

// Przeglada plik raz i zapisuje, gdzie zaczyna sie kazda strona.
bool buildIndex(uint32_t startPos) {
  static char chunk[CHUNK_BUF];

  // zwykle strona to >= 100 B; zapas 2,5x
  if (!allocIndex((int)(bookSize / 40) + 8)) return false;

  totalPages = 0;
  indexTruncated = false;
  uint32_t pos = startPos;

  while (pos < bookSize && totalPages < maxPages) {
    book.seek(pos);
    int n = book.read((uint8_t*)chunk, CHUNK_BUF - 1);
    if (n <= 0) break;
    chunk[n] = '\0';
    sanitizeBuf(chunk, n);

    bool eof = (pos + (uint32_t)n >= bookSize);
    int off = 0;

    while (totalPages < maxPages && off < n) {
      // za malo danych w kawalku: doczytaj nastepny (chyba ze koniec pliku)
      if (!eof && n - off < PAGE_BUF) break;
      pageOff[totalPages++] = pos + off;
      int used = consumePage(chunk + off);
      if (used <= 0) used = 1;
      off += used;
    }
    if (off <= 0) off = n;   // zabezpieczenie przed petla
    pos += off;
  }

  if (pos < bookSize && totalPages >= maxPages) indexTruncated = true;

  if (totalPages == 0) {
    pageOff[0] = startPos;
    totalPages = 1;
  }
  return true;
}

struct IdxHeader {
  uint32_t magic;
  uint32_t layout;
  uint32_t size;
  uint32_t count;
};

bool loadIndex() {
#if USE_INDEX_CACHE
  if (bookSize < INDEX_MIN_BOOK) return false;
  uint32_t sig = layoutSig();
  char p[28];
  idxPath(curBook, sig, p, sizeof(p));
  File f = LittleFS.open(p, "r");
  if (!f) return false;

  IdxHeader h;
  bool ok = (f.read((uint8_t*)&h, sizeof(h)) == sizeof(h)) &&
            h.magic == IDX_MAGIC && h.layout == sig && h.size == bookSize &&
            h.count > 0 && h.count <= (uint32_t)MAX_PAGES_CAP &&
            (size_t)f.size() == sizeof(h) + (size_t)h.count * 4;
  if (ok) ok = allocIndex((int)h.count) && maxPages >= (int)h.count;
  if (ok) ok = (f.read((uint8_t*)pageOff, (size_t)h.count * 4) == (size_t)h.count * 4);
  f.close();
  if (!ok) return false;

  totalPages = (int)h.count;
  indexTruncated = false;
  Serial.printf("index z cache: %d stron\n", totalPages);
  return true;
#else
  return false;
#endif
}

void saveIndex() {
#if USE_INDEX_CACHE
  if (bookSize < INDEX_MIN_BOOK || indexTruncated || totalPages <= 0) return;
  size_t need = sizeof(IdxHeader) + (size_t)totalPages * 4 + 4096;
  if (LittleFS.totalBytes() - LittleFS.usedBytes() < need) return;

  uint32_t sig = layoutSig();
  char p[28];
  idxPath(curBook, sig, p, sizeof(p));
  File f = LittleFS.open(p, "w");
  if (!f) return;

  IdxHeader h = { IDX_MAGIC, sig, bookSize, (uint32_t)totalPages };
  bool ok = (f.write((uint8_t*)&h, sizeof(h)) == sizeof(h)) &&
            (f.write((uint8_t*)pageOff, (size_t)totalPages * 4) == (size_t)totalPages * 4);
  f.close();
  if (!ok) LittleFS.remove(p);   // niedokonczony indeks nie moze zostac
  Serial.println(ok ? "index zapisany" : "index: blad zapisu");
#endif
}

int findPage(uint32_t off) {
  int lo = 0, hi = totalPages - 1;
  while (lo < hi) {
    int mid = (lo + hi + 1) / 2;
    if (pageOff[mid] <= off) lo = mid;
    else hi = mid - 1;
  }
  return lo;
}

static bool openBookBody(const char* name) {
  closeBook();
  applyLayout();

  char path[NAME_LEN + 2];
  snprintf(path, sizeof(path), "/%s", name);
  book = LittleFS.open(path, "r");
  if (!book) return false;

  bookSize = book.size();
  strncpy(curBook, name, NAME_LEN - 1);
  curBook[NAME_LEN - 1] = '\0';

  // pomin BOM UTF-8
  uint32_t startPos = 0;
  uint8_t bom[3];
  book.seek(0);
  if (bookSize >= 3 && book.read(bom, 3) == 3 && bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF) {
    startPos = 3;
  }

  if (!loadIndex()) {
    if (bookSize > 150000) {
      char sn[32];
      shortName(curBook, sn, 28);
      showMessage("Indexing...", sn);
    }
    if (!buildIndex(startPos)) {
      closeBook();
      return false;
    }
    saveIndex();
  }

  if (indexTruncated) {
    showMessage("Book too long", "Only the first part", "can be read.");
    delay(2000);
  }

  char key[16];
  bmKey(curBook, bookSize, key);
  page = findPage(prefs.getUInt(key, 0));
  prefs.putString("last", curBook);
  bmDirty = false;
  statsLoadBook();
  loadToc(curBook, bookSize);
  Serial.printf("ksiazka: %s, %u B, %d stron\n", curBook, (unsigned)bookSize, totalPages);
  return true;
}

// Otwarcie ksiazki (w tym indeksowanie) na pelnej szybkosci procesora, potem wracamy do ustawionej
bool openBook(const char* name) {
  if (cfg.powerSave > 0) cpuSet(240);
  bool ok = openBookBody(name);
  powerApply();
  return ok;
}

// Procent przeczytania liczony z pozycji w pliku
int readPercent() {
  if (page >= totalPages - 1) return 100;
  if (bookSize == 0) return 0;
  return (int)(((uint64_t)pageOff[page] * 100) / bookSize);
}

// Pasek stanu: wybrane przez uzytkownika elementy, a pasek postepu
// wypelnia cale pozostale miejsce w linii.
void drawStatus() {
  if (!statusVisible()) return;

  char st[40];
  st[0] = '\0';
  if (cfg.showPct) snprintf(st, sizeof(st), "%d%%", readPercent());
  if (cfg.showPage) {
    size_t l = strlen(st);
    snprintf(st + l, sizeof(st) - l, "%s%d/%d", l ? "  " : "", page + 1, totalPages);
  }
  if (cfg.showLeft) {
    long left = statsLeftSeconds();
    char d[16];
    if (left >= 0) fmtDuration((uint32_t)left, d, sizeof(d));
    else snprintf(d, sizeof(d), "--");     // tempo czytania jeszcze nieznane (potrzeba kilku stron)
    size_t l = strlen(st);
    snprintf(st + l, sizeof(st) - l, "%s~%s", l ? "  " : "", d);
  }

  if (cfg.clockMode > 0 && cfg.clockBook) {          // zegar w ksiazce (tylko gdy czas jest ustawiony)
    char c[12];
    if (clockFormat(c, sizeof(c), cfg.clockMode)) {
      size_t l = strlen(st);
      snprintf(st + l, sizeof(st) - l, "%s%s", l ? "  " : "", c);
    }
  }

  int tw = 0;
  if (st[0]) {
    gfxText(MARGIN_X, STATUS_Y, st, FOOT_SIZE);
    tw = gfxTextWidth(st, FOOT_SIZE);
  }

  // bateria przy prawej krawedzi: zawsze albo tylko przy niskim poziomie (Settings)
  int right = SCREEN_W - MARGIN_X;
  int bp = batteryPercent();
  if (bp >= 0 && (cfg.batMode == 2 || (cfg.batMode == 1 && bp < BAT_LOW_PCT))) {
    uiBattery(right - 21, SCREEN_H - 10, bp);
    right -= 26;
  }

  if (cfg.showBar) {
    int x0 = MARGIN_X + (tw ? tw + 8 : 0);
    int x1 = right;
    int bw = x1 - x0;
    if (bw > 10) {
      int pw;
      if (page >= totalPages - 1 || bookSize == 0) {
        pw = bw;
      } else {
        pw = (int)(((uint64_t)pageOff[page + 1] * (uint64_t)bw) / bookSize);
        if (pw < 1) pw = 1;
      }
      gfxHLine(x0, x0 + pw, 119);
      gfxHLine(x0, x1, 120);
    }
  }
}

// fastOk = wolno uzyc szybkiego (czesciowego) odswiezenia, jesli wlaczone w Ustawieniach.
// Co kilka stron robimy pelne odswiezenie, zeby usunac slady po poprzednich.
void showPage(bool fastOk) {
  char buf[PAGE_BUF + 1];
  book.seek(pageOff[page]);
  int n = book.read((uint8_t*)buf, PAGE_BUF);
  if (n < 0) n = 0;
  buf[n] = '\0';
  sanitizeBuf(buf, n);

  gfxClear();

  int pos = 0;
  for (int row = 0; row < linesPerPage; row++) {
    int adv;
    bool hy;
    int len = layoutLine(buf, pos, adv, hy);
    if (hy && len < 240) {                       // wiersz przerwany w srodku wyrazu: dorysuj myslnik
      char tmp[246];
      memcpy(tmp, buf + pos, len);
      tmp[len] = '-';
      tmp[len + 1] = '\0';
      fontDrawLine(textX0, fontTopPad() + row * fontLineH(), tmp, len + 1);
    } else {
      fontDrawLine(textX0, fontTopPad() + row * fontLineH(), buf + pos, len);
    }
    pos += adv;
    if (buf[pos] == '\0') break;
  }

  drawStatus();

  if (fastOk && cfg.fastFlip && gfxFastReady() && fastLeft > 0) {
    gfxPresentFast();
    fastLeft--;
  } else {
    gfxPresent();
    fastLeft = fullEveryVals[cfg.fullEvery % 3] - 1;
  }
}

void drawMenu();   // deklaracja (uzywana w goReader)

void goReader(const char* name) {
  if (!openBook(name)) {
    showMessage("Cannot open book", name);
    delay(1500);
    if (screen == S_LIBRARY) drawLibrary();
    else drawMenu();
    return;
  }
  screen = S_READER;
  showPage(false);
  statsTouch();
}

// Przewraca o 'delta' stron (z ograniczeniem do zakresu); rysuje tylko, gdy cos sie zmienilo
void turnPages(int delta) {
  int np = page + delta;
  if (np < 0) np = 0;
  if (np > totalPages - 1) np = totalPages - 1;
  if (np == page) return;
  int old = page;
  page = np;
  statsOnTurn(old, np);
  bmDirty = true;
  lastTurnMs = millis();
  showPage(true);
}

// Przyspieszenie przy przytrzymaniu: n = numer powtorzenia (0 = pierwsze wcisniecie)
int accelStep(int n) {
  static const uint8_t t[6] = { 1, 1, 3, 10, 25, 50 };
  return t[n < 5 ? n : 5];
}

// Skrot (dlugie OK w czytniku): nastepny rozmiar czcionki, czytasz dalej w tym samym miejscu
void cycleFontSize() {
  int cnt = fontSizeCount(cfg.famIdx);
  if (cnt < 2) return;
  char name[NAME_LEN];
  strcpy(name, curBook);
  saveBookmark();                              // pozycja zapisana jako offset w bajtach
  cfg.fontIdx = (cfg.fontIdx + 1) % cnt;
  prefs.putUChar("cfg_fs", cfg.fontIdx);
  goReader(name);                              // przepaginuje i wroci na ten sam fragment
}

// ============================================================
//  Skok do strony (OK w czytniku)
// ============================================================
static const int gotoSteps[4] = { 1, 10, 100, 1000 };
#define GOTO_CHAPTER 4             // piaty "krok": skok co rozdzial (tylko gdy ksiazka ma spis tresci)
int gotoVal = 1;
int gotoStepIdx = 0;

// Czy dany krok ma sens dla biezacej ksiazki
bool gotoStepUsable(int idx) {
  if (idx == 0) return true;
  if (idx == GOTO_CHAPTER) return chCount >= 2;
  return gotoSteps[idx] <= totalPages;
}

void drawGoto() {
  gfxClear();
  drawTitle(UI_X, "Go to page");

  char l[UI_CHARS + 1];
  snprintf(l, sizeof(l), "%d / %d", gotoVal, totalPages);
  gfxText(UI_X, 30, l, 24);

  int pct = (int)(((int64_t)gotoVal * 100) / (totalPages > 0 ? totalPages : 1));
  if (gotoStepIdx == GOTO_CHAPTER) snprintf(l, sizeof(l), "Step: chapter");
  else snprintf(l, sizeof(l), "Step: %d  (%d%%)", gotoSteps[gotoStepIdx], pct);
  gfxText(UI_X, 70, l, FONT_SIZE);

  if (chCount >= 2 && totalPages > 0) {            // nazwa rozdzialu, w ktorym wyladujemy
    int ci = chapterOfPage(gotoVal - 1);
    char t[48];
    snprintf(t, sizeof(t), "%d/%d %s", ci + 1, chCount, chTitle[ci]);
    utf8CutChars(t, (SCREEN_W - TAG_COL_W - UI_X - 2) / 6);
    gfxText(UI_X, 94, t, FOOT_SIZE);
  }

  drawLegend("GO", "STEP", "CANCEL", true);
  gfxPresent();
}

void goGoto() {
  gotoVal = page + 1;
  if (!gotoStepUsable(gotoStepIdx)) gotoStepIdx = 0;
  screen = S_GOTO;
  drawGoto();
}

// Skok do poczatku nastepnego (dir > 0) albo biezacego/poprzedniego (dir < 0) rozdzialu.
// Rozdzialy zaczynajace sie na tej samej stronie sa pomijane (nie da sie na nie "wejsc").
void gotoChapterStep(int dir) {
  int ci = chapterOfPage(gotoVal - 1);
  if (dir > 0) {
    for (int k = 1; k <= chCount; k++) {                 // z ostatniego rozdzialu wracamy na pierwszy
      int sp = chapterStartPage((ci + k) % chCount);
      if (sp != gotoVal) { gotoVal = sp; return; }
    }
  } else {
    int sp = chapterStartPage(ci);
    if (gotoVal > sp) { gotoVal = sp; return; }          // w srodku rozdzialu: na jego poczatek
    for (int k = 1; k <= chCount; k++) {                 // na poczatku: poprzedni rozdzial
      sp = chapterStartPage((ci + chCount - k) % chCount);
      if (sp != gotoVal) { gotoVal = sp; return; }
    }
  }
}

void onKeyGoto(const KeyEvent& e) {
  int step = gotoSteps[gotoStepIdx % 4];
  if (e.key == KEY_DOWN && isStep(e)) {
    if (gotoStepIdx == GOTO_CHAPTER) gotoChapterStep(1);
    else if (gotoVal >= totalPages) gotoVal = 1;          // z ostatniej strony na pierwsza
    else {
      gotoVal += step;
      if (gotoVal > totalPages) gotoVal = totalPages;
    }
    drawGoto();
  } else if (e.key == KEY_UP && isStep(e)) {
    if (gotoStepIdx == GOTO_CHAPTER) gotoChapterStep(-1);
    else if (gotoVal <= 1) gotoVal = totalPages;          // z pierwszej strony na ostatnia
    else {
      gotoVal -= step;
      if (gotoVal < 1) gotoVal = 1;
    }
    drawGoto();
  } else if (e.key == KEY_OK && isShort(e)) {
    do {
      gotoStepIdx = (gotoStepIdx + 1) % 5;
    } while (!gotoStepUsable(gotoStepIdx));
    drawGoto();
  } else if (e.key == KEY_HOME && isShort(e)) {          // zatwierdz skok
    int old = page;
    page = gotoVal - 1;
    statsOnTurn(old, page);
    bmDirty = true;
    lastTurnMs = millis();
    screen = S_READER;
    showPage(false);
  } else if (e.key == KEY_BACK && isShort(e)) {          // anuluj
    screen = S_READER;
    showPage(false);
  }
}

// ============================================================
//  Ustawienia (zapisywane w pamieci nieulotnej)
// ============================================================
void goMenu();   // deklaracja (uzywana w ustawieniach)

int setSel = 0;                  // pozycja na liscie grup albo w grupie
int setGroup = -1;               // -1 = lista grup, 0.. = wnetrze grupy
int setTopSel = 0;               // zapamietana grupa po powrocie

void loadSettings() {
  cfg.famIdx = prefs.getUChar("cfg_fm", cfg.famIdx);
  cfg.fontIdx = prefs.getUChar("cfg_fs", cfg.fontIdx);
  cfg.bold = prefs.getBool("cfg_bd", cfg.bold);
  cfg.showPct = prefs.getBool("cfg_pc", cfg.showPct);
  cfg.showPage = prefs.getBool("cfg_pg", cfg.showPage);
  cfg.showLeft = prefs.getBool("cfg_tl", cfg.showLeft);
  cfg.showBar = prefs.getBool("cfg_pb", cfg.showBar);
  cfg.batMode = prefs.getUChar("cfg_bm", cfg.batMode);
  cfg.fastFlip = prefs.getBool("cfg_ff", cfg.fastFlip);
  cfg.fullEvery = prefs.getUChar("cfg_fe", cfg.fullEvery);
  cfg.sleepIdx = prefs.getUChar("cfg_sl", cfg.sleepIdx);
  cfg.sleepScreen = prefs.getUChar("cfg_ss", cfg.sleepScreen);
  cfg.autoOpen = prefs.getBool("cfg_ao", cfg.autoOpen);
  cfg.margin = prefs.getUChar("cfg_mg", cfg.margin);
  cfg.lineGap = prefs.getUChar("cfg_lg", cfg.lineGap);
  cfg.hyphen = prefs.getBool("cfg_hy", cfg.hyphen);
  cfg.clockMode = prefs.getUChar("cfg_ck", cfg.clockMode);
  cfg.clockBook = prefs.getBool("cfg_cb", cfg.clockBook);
  cfg.tz = prefs.getUChar("cfg_tz", cfg.tz);
  cfg.fastMenu = prefs.getBool("cfg_mf", cfg.fastMenu);
  cfg.uiFont = prefs.getUChar("cfg_uf", cfg.uiFont);
  cfg.apPower = prefs.getUChar("cfg_ap", cfg.apPower);
  cfg.powerSave = prefs.getUChar("cfg_ps", cfg.powerSave);
  cfg.battEst = prefs.getBool("cfg_be", cfg.battEst);
  cfg.battCap = prefs.getUChar("cfg_bc", cfg.battCap);
  if (cfg.sleepIdx > 4) cfg.sleepIdx = 1;
  if (cfg.batMode > 2) cfg.batMode = 1;
  if (cfg.fullEvery > 2) cfg.fullEvery = 1;
  if (cfg.sleepScreen > 2) cfg.sleepScreen = 0;
  applyLayout();                 // poprawia tez nieprawidlowe indeksy czcionki
  clockApplyTz(cfg.tz);
}

void saveSettings() {
  prefs.putUChar("cfg_fm", cfg.famIdx);
  prefs.putUChar("cfg_fs", cfg.fontIdx);
  prefs.putBool("cfg_bd", cfg.bold);
  prefs.putBool("cfg_pc", cfg.showPct);
  prefs.putBool("cfg_pg", cfg.showPage);
  prefs.putBool("cfg_tl", cfg.showLeft);
  prefs.putBool("cfg_pb", cfg.showBar);
  prefs.putUChar("cfg_bm", cfg.batMode);
  prefs.putBool("cfg_ff", cfg.fastFlip);
  prefs.putUChar("cfg_fe", cfg.fullEvery);
  prefs.putUChar("cfg_sl", cfg.sleepIdx);
  prefs.putUChar("cfg_ss", cfg.sleepScreen);
  prefs.putBool("cfg_ao", cfg.autoOpen);
  prefs.putUChar("cfg_mg", cfg.margin);
  prefs.putUChar("cfg_lg", cfg.lineGap);
  prefs.putBool("cfg_hy", cfg.hyphen);
  prefs.putUChar("cfg_ck", cfg.clockMode);
  prefs.putBool("cfg_cb", cfg.clockBook);
  prefs.putUChar("cfg_tz", cfg.tz);
  prefs.putBool("cfg_mf", cfg.fastMenu);
  prefs.putUChar("cfg_uf", cfg.uiFont);
  prefs.putUChar("cfg_ap", cfg.apPower);
  prefs.putUChar("cfg_ps", cfg.powerSave);
  prefs.putBool("cfg_be", cfg.battEst);
  prefs.putUChar("cfg_bc", cfg.battCap);
}

const char* onOff(bool b) {
  return b ? "ON" : "OFF";
}

// Ustawienia sa podzielone na grupy (menu dwupoziomowe)
#define SET_GROUPS 5
static const char* const SET_GROUP_NAMES[SET_GROUPS] = { "Reading", "Status line", "Clock", "Power & system", "Battery" };
static const uint8_t SET_GROUP_ITEMS[SET_GROUPS] = { 6, 7, 3, 10, 5 };

// Teksty pozycji maja co najwyzej UI_CHARS - 2 znakow (przed nimi jest "> ")
const char* groupItem(int g, int i) {
  static char b[UI_CHARS + 1];
  static const char* const batNames[3] = { "Off", "Low", "Always" };
  static const char* const sleepNames[3] = { "Book", "Image", "Keep" };
  static const char* const marginNames[3] = { "Normal", "Narrow", "Wide" };
  static const char* const gapNames[3] = { "Normal", "Relaxed", "Loose" };
  static const char* const clockNames[3] = { "Off", "24h", "12h" };
  b[0] = 0;
  if (g == 0) {
    switch (i) {
      case 0: snprintf(b, sizeof(b), "Font: %s", fontFamilyName(cfg.famIdx)); break;
      case 1: snprintf(b, sizeof(b), "Size: %d px", fontSizeValue(cfg.famIdx, cfg.fontIdx)); break;
      case 2: snprintf(b, sizeof(b), "Bold: %s", onOff(cfg.bold)); break;
      case 3: snprintf(b, sizeof(b), "Line spacing: %s", gapNames[cfg.lineGap % 3]); break;
      case 4: snprintf(b, sizeof(b), "Margins: %s", marginNames[cfg.margin % 3]); break;
      default: snprintf(b, sizeof(b), "Hyphenation: %s", onOff(cfg.hyphen)); break;
    }
  } else if (g == 1) {
    switch (i) {
      case 0: snprintf(b, sizeof(b), "Status line: %s", onOff(statusAnyOn())); break;   // OFF = wszystko wylaczone = wiecej wierszy
      case 1: snprintf(b, sizeof(b), "Percent: %s", onOff(cfg.showPct)); break;
      case 2: snprintf(b, sizeof(b), "Page n/N: %s", onOff(cfg.showPage)); break;
      case 3: snprintf(b, sizeof(b), "Time left: %s", onOff(cfg.showLeft)); break;
      case 4: snprintf(b, sizeof(b), "Progress bar: %s", onOff(cfg.showBar)); break;
      case 5: snprintf(b, sizeof(b), "Clock in book: %s", onOff(cfg.clockBook)); break;
      default:
        if (!batteryAvailable()) snprintf(b, sizeof(b), "Battery: pin not set");   // patrz Info -> PINS
        else snprintf(b, sizeof(b), "Battery: %s", batNames[cfg.batMode % 3]);
        break;
    }
  } else if (g == 2) {
    switch (i) {
      case 0: snprintf(b, sizeof(b), "Clock: %s", clockNames[cfg.clockMode % 3]); break;
      case 1: snprintf(b, sizeof(b), "Zone: %s", TZS[cfg.tz % TZ_COUNT].name); break;
      default: {                                   // OK otwiera reczne ustawianie czasu
        char t[12];
        if (clockFormat(t, sizeof(t), cfg.clockMode == 2 ? 2 : 1)) snprintf(b, sizeof(b), "Set time: %s", t);
        else snprintf(b, sizeof(b), "Set time: not set");
      }
    }
  } else if (g == 3) {
    switch (i) {
      case 0:
        if (sleepMinutes[cfg.sleepIdx] == 0) snprintf(b, sizeof(b), "Sleep: never");
        else snprintf(b, sizeof(b), "Sleep: %d min", sleepMinutes[cfg.sleepIdx]);
        break;
      case 1: snprintf(b, sizeof(b), "Sleep screen: %s", sleepNames[cfg.sleepScreen % 3]); break;
      case 2: snprintf(b, sizeof(b), "Page flip: %s", cfg.fastFlip ? "Fast beta" : "Full"); break;
      case 3: snprintf(b, sizeof(b), "Menu flip: %s", cfg.fastMenu ? "Fast" : "Full"); break;
      case 4: {
        static const char* const uiNames[4] = { "Classic", "Helv 14", "Helv 12", "Unifont" };
        snprintf(b, sizeof(b), "Menu font: %s", uiNames[cfg.uiFont % 4]);
        break;
      }
      case 5: snprintf(b, sizeof(b), "Full every: %d pages", fullEveryVals[cfg.fullEvery % 3]); break;
      case 6: snprintf(b, sizeof(b), "Open last book: %s", onOff(cfg.autoOpen)); break;
      case 7: {
        static const char* const apNames[3] = { "Low", "Medium", "High" };
        snprintf(b, sizeof(b), "Hotspot power: %s", apNames[cfg.apPower % 3]);
        break;
      }
      case 8: {
        static const char* const psNames[3] = { "Off", "CPU 80", "Light" };
        snprintf(b, sizeof(b), "Power saving: %s", psNames[cfg.powerSave % 3]);
        break;
      }
      default: snprintf(b, sizeof(b), "Hotspot: %s", apPassword().c_str()); break;
    }
  } else {
    switch (i) {
      case 0: snprintf(b, sizeof(b), "Estimate: %s", onOff(cfg.battEst)); break;
      case 1: snprintf(b, sizeof(b), "Capacity: %d mAh", (int)BM_CAPS[cfg.battCap % 6]); break;
      case 2:
        if (bm.valid) snprintf(b, sizeof(b), "Set voltage: ~%d%%", (int)(bmPercent(bm) + 0.5f));
        else snprintf(b, sizeof(b), "Set voltage: ---");
        break;
      case 3: snprintf(b, sizeof(b), "Mark battery full"); break;
      default: snprintf(b, sizeof(b), "Clear estimate"); break;
    }
  }
  return b;
}

const char* settingsItem(int i) {
  if (setGroup < 0) return SET_GROUP_NAMES[i % SET_GROUPS];
  return groupItem(setGroup, i);
}
int settingsCount() {
  return setGroup < 0 ? SET_GROUPS : SET_GROUP_ITEMS[setGroup];
}

void goSetTime();                      // (ponizej, po ustawieniach)
void goSetBatt();
bool settingsFromReader = false;       // wejscie skrotem z ksiazki (dlugie BACK): BACK wraca do ksiazki
char settingsResumeBook[NAME_LEN];

void drawSettings() {
  if (setGroup < 0) drawList("Settings", settingsItem, SET_GROUPS, setSel, "", "OPEN", "BACK");
  else drawList(SET_GROUP_NAMES[setGroup], settingsItem, SET_GROUP_ITEMS[setGroup], setSel,
                (settingsFromReader && setGroup <= 1) ? (setGroup == 0 ? "STATUS" : "TEXT") : "", "CHANGE",
                settingsFromReader ? "BOOK" : "BACK");
}

void goSettings() {
  screen = S_SETTINGS;
  settingsFromReader = false;
  setGroup = -1;
  setSel = setTopSel;
  drawSettings();
}

void groupChange(int g, int i) {
  if (g == 0) {
    switch (i) {
      case 0:
        cfg.famIdx = (cfg.famIdx + 1) % fontFamilyCount();
        if (cfg.fontIdx >= fontSizeCount(cfg.famIdx)) cfg.fontIdx = fontSizeCount(cfg.famIdx) - 1;
        break;
      case 1: cfg.fontIdx = (cfg.fontIdx + 1) % fontSizeCount(cfg.famIdx); break;
      case 2: cfg.bold = !cfg.bold; break;
      case 3: cfg.lineGap = (cfg.lineGap + 1) % 3; break;
      case 4: cfg.margin = (cfg.margin + 1) % 3; break;
      default: cfg.hyphen = !cfg.hyphen; break;
    }
  } else if (g == 1) {
    switch (i) {
      case 0: statusSetAll(!statusAnyOn()); break;          // ON/OFF zmienia wszystkie podopcje naraz
      case 1: cfg.showPct = !cfg.showPct; break;
      case 2: cfg.showPage = !cfg.showPage; break;
      case 3: cfg.showLeft = !cfg.showLeft; break;
      case 4: cfg.showBar = !cfg.showBar; break;
      case 5: cfg.clockBook = !cfg.clockBook; break;
      default: cfg.batMode = (cfg.batMode + 1) % 3; break;
    }
  } else if (g == 2) {
    switch (i) {
      case 0: cfg.clockMode = (cfg.clockMode + 1) % 3; break;
      case 1:
        cfg.tz = (cfg.tz + 1) % TZ_COUNT;
        clockApplyTz(cfg.tz);
        break;
      default: break;                        // informacja: czas ustawia sie ze strony WWW
    }
  } else if (g == 3) {
    switch (i) {
      case 0: cfg.sleepIdx = (cfg.sleepIdx + 1) % 5; break;
      case 1: cfg.sleepScreen = (cfg.sleepScreen + 1) % 3; break;
      case 2: cfg.fastFlip = !cfg.fastFlip; break;
      case 3: cfg.fastMenu = !cfg.fastMenu; break;
      case 4: cfg.uiFont = (cfg.uiFont == 0) ? 2 : (cfg.uiFont == 2) ? 3 : 0; break;      // Classic -> Helvetica 12 -> Unifont
      case 5: cfg.fullEvery = (cfg.fullEvery + 1) % 3; break;
      case 6: cfg.autoOpen = !cfg.autoOpen; break;
      case 7: cfg.apPower = (cfg.apPower + 1) % 3; break;
      case 8:
        cfg.powerSave = (cfg.powerSave + 1) % 3;
        powerApply();
        break;
      default: apPasswordNew(); break;       // nowe haslo hotspotu
    }
  } else {
    switch (i) {
      case 0: cfg.battEst = !cfg.battEst; break;
      case 1:
        cfg.battCap = (cfg.battCap + 1) % 6;
        bmCapSync();
        break;
      case 3: bmMarkFull(); break;
      case 4: bmClear(); break;
      default: break;                        // 2: edytor napiecia (otwierany w onKeySettings)
    }
  }
}

void onKeySettings(const KeyEvent& e) {
  int n = settingsCount();
  if (e.key == KEY_UP && isStep(e)) {
    setSel = (setSel + n - 1) % n;
    listMoveOnly = true;
    drawSettings();
  } else if (e.key == KEY_DOWN && isStep(e)) {
    setSel = (setSel + 1) % n;
    listMoveOnly = true;
    drawSettings();
  } else if (e.key == KEY_OK && isShort(e)) {
    if (setGroup < 0) {                      // wejscie do grupy
      setTopSel = setSel;
      setGroup = setSel;
      setSel = 0;
    } else if (setGroup == 2 && setSel == 2) {
      goSetTime();                           // reczne ustawianie daty i godziny
      return;
    } else if (setGroup == 4 && setSel == 2) {
      goSetBatt();                           // reczne wpisanie napiecia baterii
      return;
    } else {
      groupChange(setGroup, setSel);
      applyLayout();
      listMoveOnly = true;                   // zmiana wartosci: szybkie odswiezenie
    }
    drawSettings();
  } else if (e.key == KEY_HOME && isShort(e) && settingsFromReader && setGroup >= 0 && setGroup <= 1) {
    setGroup = 1 - setGroup;                 // skrot z ksiazki: HOME przelacza tekst <-> pasek stanu
    setSel = 0;
    drawSettings();
  } else if (e.key == KEY_BACK && isShort(e)) {
    saveSettings();
    if (settingsFromReader) {                // skrot z ksiazki: wracamy do niej (z nowym ukladem)
      settingsFromReader = false;
      setGroup = -1;
      goReader(settingsResumeBook);
      return;
    }
    if (setGroup >= 0) {                     // z grupy wracamy na liste grup
      setGroup = -1;
      setSel = setTopSel;
      drawSettings();
    } else {
      goMenu();
    }
  }
}

// ---------- Reczne ustawienie zegara ----------
// Pola: 0 rok, 1 miesiac, 2 dzien, 3 godzina, 4 minuta (czas lokalny wybranej strefy).
// DOWN zwieksza, UP zmniejsza (jak w "Go to page"); OK = nastepne pole; HOME = zapisz; BACK = anuluj.
int stY = 2026, stMo = 1, stD = 1, stH = 12, stMi = 0, stField = 0;

static int wrapRange(int v, int lo, int hi) {
  int n = hi - lo + 1;
  return lo + (((v - lo) % n) + n) % n;
}

// Wartosci poczatkowe: biezacy czas; gdy zegar nie jest ustawiony - ostatni znany (zapisany) albo data kompilacji
static void stLoadDefaults() {
  time_t t = 0;
  if (clockValid()) t = clockNow();
  else {
    uint32_t last = prefs.getUInt("clk_last", 0);
    if (last > 1700000000UL) t = (time_t)last;
  }
  if (t > 0) {
    struct tm tmv;
    localtime_r(&t, &tmv);
    stY = tmv.tm_year + 1900;
    stMo = tmv.tm_mon + 1;
    stD = tmv.tm_mday;
    stH = tmv.tm_hour;
    stMi = tmv.tm_min;
  } else {
    static const char* const MONTHS = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char* d = __DATE__;                // np. "Oct 14 2026"
    stMo = 1;
    for (int i = 0; i < 12; i++) if (strncmp(d, MONTHS + 3 * i, 3) == 0) stMo = i + 1;
    stD = atoi(d + 4);
    stY = atoi(d + 7);
    stH = atoi(__TIME__);                    // np. "12:50:33"
    stMi = atoi(__TIME__ + 3);
  }
  stY = wrapRange(stY, 2024, 2099);
  stMo = wrapRange(stMo, 1, 12);
  int dm = clockDaysInMonth(stY, stMo);
  if (stD < 1) stD = 1;
  if (stD > dm) stD = dm;
}

void drawSetTime() {
  gfxClear();
  drawTitle(UI_X, "Set time");
  char l[16];
  snprintf(l, sizeof(l), "%04d-%02d-%02d", stY, stMo, stD);
  gfxText(UI_X, 26, l, 24);
  snprintf(l, sizeof(l), "%02d:%02d", stH, stMi);
  gfxText(UI_X, 62, l, 24);
  static const int fx[5] = { 0, 60, 96, 0, 36 };       // poczatek pola w px (12 px na znak)
  static const int fw[5] = { 48, 24, 24, 24, 24 };
  int under = (stField < 3 ? 26 : 62) + 26;
  gfxHLine(UI_X + fx[stField], UI_X + fx[stField] + fw[stField] - 1, under);        // podkreslenie aktywnego pola
  gfxHLine(UI_X + fx[stField], UI_X + fx[stField] + fw[stField] - 1, under + 1);
  char z[28];
  snprintf(z, sizeof(z), "Zone: %s", TZS[cfg.tz % TZ_COUNT].name);
  gfxText(UI_X, 98, z, FOOT_SIZE);
  drawLegend("SAVE", "NEXT", "CANCEL", true);
  gfxPresent();
}

void goSetTime() {
  stLoadDefaults();
  stField = 3;                               // najczesciej poprawia sie godzine
  screen = S_SETTIME;
  drawSetTime();
}

static void stLeave() {
  screen = S_SETTINGS;
  setGroup = 2;
  setSel = 2;
  drawSettings();
}

void onKeySetTime(const KeyEvent& e) {
  if ((e.key == KEY_UP || e.key == KEY_DOWN) && isStep(e)) {
    int dir = (e.key == KEY_DOWN) ? 1 : -1;
    int step = (stField == 4) ? accelStep(e.repeat) : 1;   // minuty: przy przytrzymaniu szybciej
    if (step > 10) step = 10;
    switch (stField) {
      case 0: stY = wrapRange(stY + dir * step, 2024, 2099); break;
      case 1: stMo = wrapRange(stMo + dir * step, 1, 12); break;
      case 2: stD = wrapRange(stD + dir * step, 1, clockDaysInMonth(stY, stMo)); break;
      case 3: stH = wrapRange(stH + dir * step, 0, 23); break;
      default: stMi = wrapRange(stMi + dir * step, 0, 59); break;
    }
    int dm = clockDaysInMonth(stY, stMo);                 // zmiana roku/miesiaca nie moze zostawic nieistniejacego dnia
    if (stD > dm) stD = dm;
    drawSetTime();
  } else if (e.key == KEY_OK && isShort(e)) {
    stField = (stField + 1) % 5;
    drawSetTime();
  } else if (e.key == KEY_HOME && isShort(e)) {          // zapisz
    clockApplyTz(cfg.tz);
    if (clockSetLocal(stY, stMo, stD, stH, stMi)) prefs.putUInt("clk_last", (uint32_t)clockNow());
    stLeave();
  } else if (e.key == KEY_BACK && isShort(e)) {          // anuluj
    stLeave();
  }
}

// ---------- Reczne wpisanie napiecia baterii ----------
// Napiecie zmierzone multimetrem NA SPOCZYNKU (bateria bez obciazenia, po kilkudziesieciu minutach od
// ladowania/pracy) ustawia punkt startowy szacunku wg krzywej Li-Po. DOWN zwieksza, UP zmniejsza,
// OK = krok 0,01 / 0,05 V, HOME = zapisz, BACK = anuluj.
int sbCv = 390;                  // centywolty
int sbStep = 1;

void drawSetBatt() {
  gfxClear();
  drawTitle(UI_X, "Battery voltage");
  char l[28];
  snprintf(l, sizeof(l), "%d.%02d V", sbCv / 100, sbCv % 100);
  gfxText(UI_X, 24, l, 24);
  snprintf(l, sizeof(l), "= ~%d%% on the curve", (int)(bmPctFromVolt(sbCv / 100.0f) + 0.5f));
  gfxText(UI_X, 56, l, FOOT_SIZE);
  if (bm.valid) snprintf(l, sizeof(l), "Estimate now: ~%d%%", (int)(bmPercent(bm) + 0.5f));
  else snprintf(l, sizeof(l), "Estimate now: not set");
  gfxText(UI_X, 70, l, FOOT_SIZE);
  snprintf(l, sizeof(l), "Step: %d.%02d V", sbStep / 100, sbStep % 100);
  gfxText(UI_X, 84, l, FOOT_SIZE);
  gfxText(UI_X, 98, "Measure at rest, no load", FOOT_SIZE);
  drawLegend("SAVE", "STEP", "CANCEL", true);
  gfxPresent();
}

void goSetBatt() {
  sbCv = bm.valid ? (int)(bmVoltFromPct(bmPercent(bm)) * 100.0f + 0.5f) : 390;
  if (sbCv < 300) sbCv = 300;
  if (sbCv > 425) sbCv = 425;
  sbStep = 1;
  screen = S_SETBATT;
  drawSetBatt();
}

static void sbLeave() {
  screen = S_SETTINGS;
  setGroup = 4;
  setSel = 2;
  drawSettings();
}

void onKeySetBatt(const KeyEvent& e) {
  if ((e.key == KEY_UP || e.key == KEY_DOWN) && isStep(e)) {
    int dir = (e.key == KEY_DOWN) ? 1 : -1;
    sbCv += dir * sbStep * (e.repeat > 8 ? 2 : 1);          // przytrzymanie: szybciej
    if (sbCv < 300) sbCv = 300;
    if (sbCv > 425) sbCv = 425;
    drawSetBatt();
  } else if (e.key == KEY_OK && isShort(e)) {
    sbStep = (sbStep == 1) ? 5 : 1;
    drawSetBatt();
  } else if (e.key == KEY_HOME && isShort(e)) {
    bmSetVoltage(sbCv / 100.0f);
    sbLeave();
  } else if (e.key == KEY_BACK && isShort(e)) {
    sbLeave();
  }
}

// ============================================================
//  Menu
// ============================================================
bool getLastBook(char* out, size_t n) {
  if (!fsOk) return false;
  String s = prefs.getString("last", "");
  if (s.length() == 0 || s.length() >= n) return false;
  strcpy(out, s.c_str());
  char path[NAME_LEN + 2];
  snprintf(path, sizeof(path), "/%s", out);
  return LittleFS.exists(path);
}

const char* menuItem(int i) {
  return menuLabels[i];
}

void drawMenu() {
  char last[NAME_LEN];
  bool canRead = getLastBook(last, sizeof(last));      // BACK = wroc do ksiazki
  drawListBody("Menu", menuItem, menuCount, menuSel, "", "SELECT", canRead ? "READ" : "");
  drawBatteryCorner();
  presentList();
}

void buildMenu() {
  menuCount = 0;
  char last[NAME_LEN];
  if (getLastBook(last, sizeof(last))) {
    menuIds[menuCount] = M_CONTINUE;
    menuLabels[menuCount++] = "Continue";
  }
  menuIds[menuCount] = M_LIBRARY;
  menuLabels[menuCount++] = "Library";
  menuIds[menuCount] = M_UPLOAD;
  menuLabels[menuCount++] = "Upload books";
  menuIds[menuCount] = M_STATS;
  menuLabels[menuCount++] = "Stats";
  menuIds[menuCount] = M_SETTINGS;
  menuLabels[menuCount++] = "Settings";
  menuIds[menuCount] = M_INFO;
  menuLabels[menuCount++] = "Info";

  if (menuSel >= menuCount) menuSel = 0;
}

void goMenu() {
  powerApply();
  buildMenu();
  screen = S_MENU;
  drawMenu();
}

// ============================================================
//  Info
// ============================================================
void goInfo() {
  char l1[UI_CHARS + 1], l3[UI_CHARS + 1], l4[UI_CHARS + 1];
  scanBooks();
  if (fsOk) {
    snprintf(l1, sizeof(l1), "Free %uKB Books %d", (unsigned)((LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024), bookCount);
  } else {
    snprintf(l1, sizeof(l1), "Filesystem error");
  }
  if (PIN_BAT_ADC >= 0) {
    int mv = batteryMv();
    snprintf(l3, sizeof(l3), "Battery: %d%% %d.%02dV", batteryPercentFromMv(mv), mv / 1000, (mv % 1000) / 10);
  } else if (batteryIsEstimate()) {
    snprintf(l3, sizeof(l3), "Battery: ~%d%% (est.)", batteryPercent());
  } else {
    snprintf(l3, sizeof(l3), "Battery: not set");
  }
  char tm[12], dt[12];
  if (clockFormat(tm, sizeof(tm), cfg.clockMode == 2 ? 2 : 1) && clockFormatDate(dt, sizeof(dt))) snprintf(l4, sizeof(l4), "Time: %s %s", tm, dt);
  else snprintf(l4, sizeof(l4), "Time: not set");

  gfxClear();
  drawTitle(UI_X, "Info");
  drawBatteryCorner();
  gfxText(UI_X, 22, l1, FONT_SIZE);
  gfxText(UI_X, 40, l3, FONT_SIZE);
  gfxText(UI_X, 58, l4, FONT_SIZE);
  gfxText(UI_X, 76, BOARD_NAME, FONT_SIZE);
  gfxText(UI_X, 94, "InkBlink v" FW_VERSION, FONT_SIZE);
  drawLegend("", batteryAvailable() ? "" : "PINS", "BACK", false);   // PINS: szukanie pinu baterii
  gfxPresent();
  screen = S_INFO;
}

// Test pinow: pokazuje napiecie na wolnych pinach ADC. Pin dzielnika baterii (przy podlaczonej
// baterii) pokaze ok. polowy jej napiecia, czyli 1700-2150 mV. Wpisz go jako PIN_BAT_ADC.
static const uint8_t scanPins[7] = { 3, 8, 15, 16, 17, 18, 20 };

void goPinScan() {
  gfxClear();
  drawTitle(UI_X, "Pin test (mV)");
  char l[24];
  for (int i = 0; i < 7; i++) {
    int mv = boardPinMv(scanPins[i]);
    snprintf(l, sizeof(l), "GPIO%-2d %4d%s", scanPins[i], mv, (mv >= 1500 && mv <= 2300) ? "*" : "");
    gfxText(UI_X, 20 + i * 13, l, FOOT_SIZE);
    Serial.printf("pin GPIO%d = %d mV\n", scanPins[i], mv);
  }
  gfxText(84, 22, "Battery plugged in?", FOOT_SIZE);
  gfxText(84, 36, "Look for the pin at", FOOT_SIZE);
  gfxText(84, 49, "1700-2150 mV (*).", FOOT_SIZE);
  gfxText(84, 62, "Put its number in", FOOT_SIZE);
  gfxText(84, 75, "PIN_BAT_ADC in", FOOT_SIZE);
  gfxText(84, 88, "board_crowpanel.h", FOOT_SIZE);
  drawLegend("", "", "BACK", false);
  gfxPresent();
  screen = S_PINSCAN;
}

// ---------- Statystyki ----------
void goStats() {
  char l[UI_CHARS + 1], d[16];
  gfxClear();
  drawTitle(UI_X, "Stats");
  drawBatteryCorner();

  fmtDurationLong(tst.secs, d, sizeof(d));
  snprintf(l, sizeof(l), "Read: %s", d);
  gfxText(UI_X, 18, l, FONT_SIZE);
  snprintf(l, sizeof(l), "Books done: %u", (unsigned)tst.finished);
  gfxText(UI_X, 34, l, FONT_SIZE);
  snprintf(l, sizeof(l), "Pages read: %u", (unsigned)tst.pages);
  gfxText(UI_X, 50, l, FONT_SIZE);
  if (tst.spdSecs >= 60 && tst.spdBytes >= 2000) {
    // ok. 7 bajtow na slowo (UTF-8 z polskimi znakami), wiec to tylko przyblizenie
    int wpm = (int)(((double)tst.spdBytes / 7.0) / ((double)tst.spdSecs / 60.0));
    snprintf(l, sizeof(l), "Speed: ~%d wpm", wpm);
  } else {
    snprintf(l, sizeof(l), "Speed: --");
  }
  gfxText(UI_X, 66, l, FONT_SIZE);

  char name[NAME_LEN];
  uint32_t size = 0, off = 0;
  if (lastBookInfo(name, size, off)) {
    BookStats b = statsReadBook(name, size);
    fmtDurationLong(b.secs, d, sizeof(d));
    snprintf(l, sizeof(l), "This book: %s", d);
    gfxText(UI_X, 82, l, FONT_SIZE);
    long left = statsLeftSecondsFor(b, size > off ? size - off : 0);
    if (left >= 0) {
      fmtDurationLong((uint32_t)left, d, sizeof(d));
      snprintf(l, sizeof(l), "Left: ~%s", d);
    } else {
      snprintf(l, sizeof(l), "Left: --");
    }
    gfxText(UI_X, 98, l, FONT_SIZE);
  } else {
    gfxText(UI_X, 82, "No book yet", FONT_SIZE);
  }
  drawLegend("", "", "BACK", false);
  gfxPresent();
  screen = S_STATS;
}

// ============================================================
//  Hotspot i strona WWW: ksiazki, ustawienia, obraz uspienia
//  Czytnik sam tworzy siec WiFi (haslo stale, telefon je zapamieta) i serwuje
//  strone pod adresem 192.168.4.1. Dostep ma tylko ten, kto zna haslo sieci,
//  wiec dodatkowy PIN nie jest potrzebny. Strona jest po angielsku.
// ============================================================
DNSServer dns;

String htmlEsc(const String& s) {
  String o;
  o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') o += "&amp;";
    else if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '"') o += "&quot;";
    else if (c == '\'') o += "&#39;";
    else o += c;
  }
  return o;
}

// Zlicza zadanie HTTP (diagnostyka: widac, czy telefon w ogole dociera do czytnika)
void countHit(const char* what) {
  httpHits++;
  statusDirty = true;
  Serial.printf("HTTP %s\n", what);
}

// Zdarzenia hotspotu: ktos sie polaczyl / rozlaczyl / dostal adres IP
void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
    Serial.println("AP: telefon polaczony");
    statusDirty = true;
  } else if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
    Serial.println("AP: telefon rozlaczony");
    statusDirty = true;
  } else if (event == ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED) {
    Serial.println("AP: telefon dostal adres IP");
    statusDirty = true;
  }
}

// ---------- Pomocnicze: strona wysylana kawalkami (bez duzych buforow w RAM) ----------
void webBegin() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent_P(WEB_HEAD);
}

void webEnd() {
  server.sendContent_P(WEB_TAIL);
  server.sendContent("");
}

String chip(const char* label, const char* value) {
  String c = "<span class='chip'>";
  c += label;
  c += " <b>";
  c += value;
  c += "</b></span>";
  return c;
}

void optTag(String& h, int value, const char* label, bool selected) {
  h += "<option value='";
  h += String(value);
  h += "'";
  if (selected) h += " selected";
  h += ">";
  h += label;
  h += "</option>";
}

void checkTag(String& h, const char* name, const char* label, bool on) {
  h += "<label class='c'><input type='checkbox' name='";
  h += name;
  h += "'";
  if (on) h += " checked";
  h += "> ";
  h += label;
  h += "</label>";
}

// Nazwa do wyswietlenia: bez .txt, '_' -> spacja
void displayName(const char* fileName, char* out, int maxChars) {
  shortName(fileName, out, maxChars);
  for (char* p = out; *p; p++)
    if (*p == '_') *p = ' ';
}

// ---------- Strona glowna: ksiazki ----------
void handleRoot() {
  countHit("/");
  scanBooks();
  webBegin();

  char b[32];
  String h;
  h.reserve(800);
  h += "<div class='chips'>";
  snprintf(b, sizeof(b), "%.1f MB", (LittleFS.totalBytes() - LittleFS.usedBytes()) / 1048576.0);
  h += chip("Free", b);
  snprintf(b, sizeof(b), "%d", bookCount);
  h += chip("Books", b);
  int bp = batteryPercent();
  if (bp >= 0) {
    snprintf(b, sizeof(b), "%s%d%%", batTilde(), bp);
    h += chip("Battery", b);
  }
  h += "</div>";
  server.sendContent(h);

  server.sendContent_P(WEB_BOOKS_UPLOAD);

  h = "<div class='card'><h2>Library</h2>";
  if (bookCount == 0) {
    h += "<p class='empty'>No books yet. Add your first one above.</p>";
  } else {
    h += "<ul class='books'>";
    for (int i = 0; i < bookCount; i++) {
      char path[NAME_LEN + 2], title[NAME_LEN];
      snprintf(path, sizeof(path), "/%s", bookNames[i]);
      File f = LittleFS.open(path, "r");
      unsigned kb = f ? (unsigned)(f.size() / 1024) : 0;
      if (f) f.close();
      displayName(bookNames[i], title, NAME_LEN - 1);
      h += "<li><div class='ic'>TXT</div><div class='t'><div>";
      h += htmlEsc(title);
      h += "</div><div>";
      h += String(kb);
      h += " KB";
      char tp[NAME_LEN + 3];
      tocPathFor(bookNames[i], tp, sizeof(tp));
      File tf = tp[0] ? LittleFS.open(tp, "r") : File();
      if (tf) {
        h += " &middot; chapters";
        tf.close();
      }
      h += "</div></div><button class='del' data-f='";
      h += htmlEsc(bookNames[i]);
      h += "'>Delete</button></li>";
      if (h.length() > 900) {
        server.sendContent(h);
        h = "";
      }
    }
    h += "</ul>";
  }
  h += "</div>";
  server.sendContent(h);

  server.sendContent_P(WEB_JS_BOOKS);
  webEnd();
}

void handleDelete() {
  countHit("/delete");
  char name[NAME_LEN];
  if (makeBookName(server.arg("f").c_str(), name)) {
    char path[NAME_LEN + 2];
    snprintf(path, sizeof(path), "/%s", name);
    LittleFS.remove(path);
    char tp[NAME_LEN + 3];
    tocPathFor(name, tp, sizeof(tp));
    if (tp[0]) LittleFS.remove(tp);                // razem ze spisem tresci
    removeAllIdx(name);
  }
  server.send(200, "text/plain", "OK");
}

// Przegladarki i systemy sprawdzaja lacznosc, pytajac o znane adresy. Skoro DNS zwraca
// tu wszystko, odpowiadamy tak, jak odpowiadalby internet - wtedy telefon uznaje siec
// za sprawna i NIE przerzuca ruchu na dane komorkowe (przez to strona by sie nie otwierala).
// Pozostale adresy przekierowujemy na strone glowna.
void handleNotFound() {
  countHit(server.uri().c_str());
#if CAPTIVE_TRICKS
  String u = server.uri();
  if (u == "/generate_204" || u == "/gen_204") {                    // Android
    server.send(204, "text/plain", "");
    return;
  }
  if (u == "/hotspot-detect.html" || u == "/library/test/success.html") {   // iOS / macOS
    server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    return;
  }
  if (u == "/connecttest.txt") {                                    // Windows
    server.send(200, "text/plain", "Microsoft Connect Test");
    return;
  }
  if (u == "/ncsi.txt") {
    server.send(200, "text/plain", "Microsoft NCSI");
    return;
  }
#endif
  server.sendHeader("Location", "http://192.168.4.1/");
  server.send(302, "text/plain", "");
}

// ---------- Ustawienia ----------
void handleConfigGet() {
  countHit("/config");
  webBegin();
  if (server.hasArg("ok")) server.sendContent("<div class='toast'>Settings saved.</div>");

  String h;
  h.reserve(1200);
  h = "<form method='POST' action='/config'>";

  h += "<div class='card'><h2>Reading</h2><p class='sub'>Applies to the book you open next.</p>"
       "<label class='f'>Font and size</label><select name='font'>";
  for (int f = 0; f < fontFamilyCount(); f++)
    for (int sz = 0; sz < fontSizeCount(f); sz++) {
      char lab[32];
      snprintf(lab, sizeof(lab), "%s %d px", fontFamilyName(f), fontSizeValue(f, sz));
      optTag(h, f * 10 + sz, lab, f == cfg.famIdx && sz == cfg.fontIdx);
    }
  h += "</select>";
  checkTag(h, "bold", "Bold text", cfg.bold);
  h += "<label class='f'>Text margins</label><select name='margin'>";
  optTag(h, 0, "Normal", cfg.margin == 0);
  optTag(h, 1, "Narrow (more text)", cfg.margin == 1);
  optTag(h, 2, "Wide", cfg.margin == 2);
  h += "</select><label class='f'>Line spacing</label><select name='lgap'>";
  optTag(h, 0, "Normal", cfg.lineGap == 0);
  optTag(h, 1, "Relaxed (+2 px)", cfg.lineGap == 1);
  optTag(h, 2, "Loose (+4 px)", cfg.lineGap == 2);
  h += "</select>";
  checkTag(h, "hy", "Hyphenate long words (Polish rules)", cfg.hyphen);
  h += "</div>";
  server.sendContent(h);

  h = "<div class='card'><h2>Status line</h2>";
  h += "<label class='c'><input type='checkbox' id='stm'";
  if (statusAnyOn()) h += " checked";
  h += " onclick=\"var c=this.checked;['pct','pg','left','bar','clkbook'].forEach(function(n){var e=document.getElementsByName(n)[0];if(e)e.checked=c;});var b=document.getElementsByName('bat')[0];if(b)b.value=c?'1':'0';\"> <b>Show the status line</b> (turn off for more lines of text per page)</label>";
  checkTag(h, "pct", "Percent read", cfg.showPct);
  checkTag(h, "pg", "Page number (n/N)", cfg.showPage);
  checkTag(h, "left", "Estimated time left", cfg.showLeft);
  checkTag(h, "bar", "Progress bar", cfg.showBar);
  h += "<label class='f'>Battery in the book</label><select name='bat'>";
  optTag(h, 0, "Hidden", cfg.batMode == 0);
  optTag(h, 1, "Only when low", cfg.batMode == 1);
  optTag(h, 2, "Always", cfg.batMode == 2);
  h += "</select><p class='note'>Needs the battery pin set in the firmware (see Info &rarr; PINS on the reader).</p></div>";
  server.sendContent(h);

  h = "<div class='card'><h2>Clock</h2><p class='sub'>The reader has no clock battery: it takes the time from this device every time you open this page. Time survives sleep, not a full power-off.</p>"
      "<label class='f'>Show the time</label><select name='clk'>";
  optTag(h, 0, "Off", cfg.clockMode == 0);
  optTag(h, 1, "24-hour, e.g. 14:35", cfg.clockMode == 1);
  optTag(h, 2, "12-hour, e.g. 2:35pm", cfg.clockMode == 2);
  h += "</select>";
  checkTag(h, "clkbook", "Also show it in the book's status line", cfg.clockBook);
  h += "<label class='f'>Time zone</label><select name='tz'>";
  for (int i = 0; i < TZ_COUNT; i++) optTag(h, i, TZS[i].name, cfg.tz == i);
  char tm[32], t1[12], d1[12];
  if (clockFormat(t1, sizeof(t1), cfg.clockMode == 2 ? 2 : 1) && clockFormatDate(d1, sizeof(d1))) snprintf(tm, sizeof(tm), "%s, %s", t1, d1);
  else snprintf(tm, sizeof(tm), "not set");
  h += "</select><p class='note'>Time on the reader now: <b id='devtime'>";
  h += tm;
  h += "</b></p></div>";
  server.sendContent(h);

  h = "<div class='card'><h2>Battery</h2><p class='sub'>This board cannot measure its battery, so the reader estimates it from the time spent awake, in hotspot mode and asleep. Enter the voltage you measured with a multimeter (battery at rest, no load) to set the starting point.</p>";
  checkTag(h, "battest", "Show the estimate", cfg.battEst);
  h += "<label class='f'>Battery capacity</label><select name='battcap'>";
  for (int i = 0; i < 6; i++) {
    char cl[16];
    snprintf(cl, sizeof(cl), "%d mAh", (int)BM_CAPS[i]);
    optTag(h, i, cl, cfg.battCap == i);
  }
  char be[24];
  if (bm.valid) snprintf(be, sizeof(be), "~%d%%", (int)(bmPercent(bm) + 0.5f));
  else snprintf(be, sizeof(be), "not set");
  h += "</select><p class='note'>Estimate now: <b>";
  h += be;
  h += "</b> &middot; <a href='/battlog.csv'>download the log (CSV)</a></p>"
       "<label class='f'>Set from measured voltage (V)</label><input name='bv' type='number' step='0.01' min='3.0' max='4.25' placeholder='e.g. 3.92'>";
  checkTag(h, "bfull", "The battery is full", false);
  h += "<p class='note'>Applied when you save. Every setting is logged together with the counters, so the estimate can be refined later.</p></div>";
  server.sendContent(h);

  h = "<div class='card'><h2>Display</h2><p class='sub'>Fast page flip skips the black flash but is experimental.</p>"
      "<label class='f'>Page flip</label><select name='flip'>";
  optTag(h, 0, "Full refresh (flashes, cleanest)", !cfg.fastFlip);
  optTag(h, 1, "Fast (beta, no flash)", cfg.fastFlip);
  h += "</select><label class='f'>Full refresh every</label><select name='fullev'>";
  for (int i = 0; i < 3; i++) {
    char lab[24];
    snprintf(lab, sizeof(lab), "%d pages", fullEveryVals[i]);
    optTag(h, i, lab, cfg.fullEvery == i);
  }
  h += "</select><p class='note'>Only used with Fast page flip: removes ghost images.</p>"
       "<label class='f'>Menu refresh</label><select name='mflip'>";
  optTag(h, 1, "Fast (no flash when moving in lists)", cfg.fastMenu);
  optTag(h, 0, "Full refresh", !cfg.fastMenu);
  h += "</select><label class='f'>Menu font</label><select name='uifont'>";
  optTag(h, 2, "Helvetica 12 (thin)", cfg.uiFont == 2);
  optTag(h, 3, "Unifont (pixel, wide)", cfg.uiFont == 3);
  optTag(h, 0, "Classic (pixel, old)", cfg.uiFont == 0);
  h += "</select><label class='f'>Hotspot power</label><select name='appower'>";
  optTag(h, 0, "Low (best when the phone or PC is next to the reader)", cfg.apPower == 0);
  optTag(h, 1, "Medium", cfg.apPower == 1);
  optTag(h, 2, "High (longer range, needs a strong power source)", cfg.apPower == 2);
  h += "</select><label class='f'>Power saving (experimental)</label><select name='psave'>";
  optTag(h, 0, "Off (processor 240 MHz)", cfg.powerSave == 0);
  optTag(h, 1, "Processor 80 MHz", cfg.powerSave == 1);
  optTag(h, 2, "80 MHz + light sleep when idle", cfg.powerSave == 2);
  h += "</select></div>";
  server.sendContent(h);

  h = "<div class='card'><h2>Power</h2>"
      "<label class='f'>Go to sleep after</label><select name='sleep'>";
  for (int i = 0; i < 5; i++) {
    char lab[24];
    if (sleepMinutes[i] == 0) snprintf(lab, sizeof(lab), "Never");
    else snprintf(lab, sizeof(lab), "%d min", sleepMinutes[i]);
    optTag(h, i, lab, cfg.sleepIdx == i);
  }
  h += "</select><label class='f'>Sleep screen</label><select name='sscr'>";
  optTag(h, 0, "Book card (title, progress)", cfg.sleepScreen == 0);
  optTag(h, 1, "Sleep image", cfg.sleepScreen == 1);
  optTag(h, 2, "Keep the last page", cfg.sleepScreen == 2);
  h += "</select>";
  checkTag(h, "ao", "Open the last book on start", cfg.autoOpen);
  h += "</div><p><button class='btn'>Save settings</button></p></form>";
  server.sendContent(h);

  webEnd();
}

void handleConfigPost() {
  countHit("/config (save)");
  int fv = server.arg("font").toInt();
  int fam = fv / 10;
  int si = fv % 10;
  if (fam >= 0 && fam < fontFamilyCount() && si >= 0 && si < fontSizeCount(fam)) {
    cfg.famIdx = fam;
    cfg.fontIdx = si;
  }
  cfg.bold = server.hasArg("bold");
  int mg = server.arg("margin").toInt();
  if (mg >= 0 && mg <= 2) cfg.margin = mg;
  int lg = server.arg("lgap").toInt();
  if (lg >= 0 && lg <= 2) cfg.lineGap = lg;
  cfg.hyphen = server.hasArg("hy");
  int ck = server.arg("clk").toInt();
  if (ck >= 0 && ck <= 2) cfg.clockMode = ck;
  cfg.clockBook = server.hasArg("clkbook");
  int tzv = server.arg("tz").toInt();
  if (tzv >= 0 && tzv < TZ_COUNT) cfg.tz = tzv;
  clockApplyTz(cfg.tz);
  cfg.showPct = server.hasArg("pct");
  cfg.showPage = server.hasArg("pg");
  cfg.showLeft = server.hasArg("left");
  cfg.showBar = server.hasArg("bar");
  int bm = server.arg("bat").toInt();
  if (bm >= 0 && bm <= 2) cfg.batMode = bm;
  cfg.fastFlip = server.arg("flip").toInt() == 1;
  if (server.hasArg("mflip")) cfg.fastMenu = server.arg("mflip").toInt() == 1;
  cfg.battEst = server.hasArg("battest");
  if (server.hasArg("battcap")) {
    int bc = server.arg("battcap").toInt();
    if (bc >= 0 && bc <= 5 && bc != cfg.battCap) {
      cfg.battCap = bc;
      bmCapSync();
    }
  }
  if (server.hasArg("psave")) {
    int ps = server.arg("psave").toInt();
    if (ps >= 0 && ps <= 2) cfg.powerSave = ps;
    powerApply();
  }
  {
    String bv = server.arg("bv");
    if (bv.length() > 0) {
      float v = bv.toFloat();
      if (v >= 3.0f && v <= 4.25f) bmSetVoltage(v);
    }
    if (server.hasArg("bfull")) bmMarkFull();
  }
  if (server.hasArg("appower")) {
    int ap = server.arg("appower").toInt();
    if (ap >= 0 && ap <= 2) cfg.apPower = ap;
  }
  if (server.hasArg("uifont")) {
    int uf = server.arg("uifont").toInt();
    if (uf == 1) uf = 2;                    // dawne Helvetica 14
    if (uf >= 0 && uf <= 3) cfg.uiFont = uf;
  }
  int fe = server.arg("fullev").toInt();
  if (fe >= 0 && fe <= 2) cfg.fullEvery = fe;
  int sl = server.arg("sleep").toInt();
  if (sl >= 0 && sl < 5) cfg.sleepIdx = sl;
  int ss = server.arg("sscr").toInt();
  if (ss >= 0 && ss <= 2) cfg.sleepScreen = ss;
  cfg.autoOpen = server.hasArg("ao");

  saveSettings();
  applyLayout();

  server.sendHeader("Location", "/config?ok=1");
  server.send(303);
}

// Log szacowania baterii (CSV) do pobrania ze strony WWW
void handleBattLog() {
  String out;
  if (fsOk) {
    File f = LittleFS.open("/battlog.csv", "r");
    if (f) {
      char buf[256];
      int n;
      while ((n = f.read((uint8_t*)buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n; i++) out += buf[i];
      }
      f.close();
    }
  }
  if (out.length() == 0) out = "time,volt,pct_from_volt,est_before,est_after,awake_h,hotspot_h,sleep_h,flips_fast,flips_full,power_save,capacity_mah,note\n";
  server.send(200, "text/csv", out);
}

// Ustawianie zegara: przegladarka wysyla aktualny czas (sekundy od 1970) przy kazdym otwarciu strony
void handleTime() {
  long t = server.arg("t").toInt();
  if (t < 1700000000L) {
    server.send(400, "text/plain", "bad time");
    return;
  }
  clockSet((time_t)t);
  prefs.putUInt("clk_last", (uint32_t)t);                  // domyslna wartosc przy recznym ustawianiu po utracie zasilania
  char b[32], tm[12], dt[12];
  if (clockFormat(tm, sizeof(tm), cfg.clockMode == 2 ? 2 : 1) && clockFormatDate(dt, sizeof(dt))) snprintf(b, sizeof(b), "%s, %s", tm, dt);
  else snprintf(b, sizeof(b), "not set");
  server.send(200, "text/plain", b);
}

// ---------- Obraz uspienia ----------
void handleWallpaperPage() {
  countHit("/wallpaper");
  webBegin();
  server.sendContent_P(WEB_WP_BODY);
  server.sendContent_P(WEB_JS_WALLPAPER);
  webEnd();
}

void handleWpData() {
  HTTPUpload& up = server.upload();

  if (up.status == UPLOAD_FILE_START) {
    wpOk = false;
    wpErr = "";
    wpBytes = 0;
    if (wpFile) wpFile.close();
    wpFile = LittleFS.open(WP_TMP, "w");
    if (!wpFile) {
      wpErr = "Cannot create file";
      return;
    }
    wpOk = true;
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (wpOk && wpFile) {
      if (wpBytes + up.currentSize > WALLPAPER_BYTES) {
        wpOk = false;
        wpErr = "Image too large";
      } else if (wpFile.write(up.buf, up.currentSize) != up.currentSize) {
        wpOk = false;
        wpErr = "Write failed (disk full?)";
      } else {
        wpBytes += up.currentSize;
      }
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (wpFile) wpFile.close();
    if (wpOk && wpBytes == WALLPAPER_BYTES) {
      LittleFS.remove(WALLPAPER_PATH);
      if (!LittleFS.rename(WP_TMP, WALLPAPER_PATH)) {
        wpOk = false;
        wpErr = "Rename failed";
        LittleFS.remove(WP_TMP);
      }
    } else {
      if (wpOk) {
        wpOk = false;
        wpErr = "Wrong image size";
      }
      LittleFS.remove(WP_TMP);
    }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (wpFile) wpFile.close();
    LittleFS.remove(WP_TMP);
    wpOk = false;
  }
}

void handleWpFinished() {
  countHit("/wp");
  if (!wpOk) {
    server.send(400, "text/plain", wpErr.length() ? wpErr : String("Upload failed"));
    return;
  }
  server.send(200, "text/plain", "OK");
}

void handleWpReset() {
  countHit("/wp_reset");
  LittleFS.remove(WALLPAPER_PATH);
  server.send(200, "text/plain", "OK");
}

// ---------- Upload ksiazek ----------
// Zapis idzie do pliku tymczasowego; dopiero po udanym zakonczeniu
// podmieniamy nim ksiazke. Przerwany upload nie rusza starej wersji.
void handleUploadData() {
  HTTPUpload& up = server.upload();

  if (up.status == UPLOAD_FILE_START) {
    uploadOk = false;
    uploadErr = "";
    uploadPath[0] = '\0';
    if (uploadFile) uploadFile.close();

    uploadIsToc = false;
    uploadBytes = 0;
    if (!makeBookName(up.filename.c_str(), uploadName)) {
      if (makeTocName(up.filename.c_str(), uploadName)) {
        uploadIsToc = true;                // spis tresci z importu EPUB
      } else {
        uploadErr = "Only .txt files are accepted";
        return;
      }
    }
    snprintf(uploadPath, sizeof(uploadPath), "/%s", uploadName);
    uploadFile = LittleFS.open(TMP_PATH, "w");
    if (!uploadFile) {
      uploadErr = "Cannot create file";
      return;
    }
    uploadOk = true;
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (uploadOk && uploadFile) {
      uploadBytes += up.currentSize;
      if (uploadIsToc && uploadBytes > TOC_MAX_BYTES) {
        uploadOk = false;
        uploadErr = "Chapter list too large";
      } else if (uploadFile.write(up.buf, up.currentSize) != up.currentSize) {
        uploadOk = false;
        uploadErr = "Write failed (disk full?)";
      }
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (uploadFile) uploadFile.close();
    if (uploadOk) {
      if (!uploadIsToc) removeAllIdx(uploadName);   // stary indeks juz nie pasuje
      LittleFS.remove(uploadPath);         // stara wersja (jesli byla)
      if (LittleFS.rename(TMP_PATH, uploadPath)) {
        if (!uploadIsToc) {
          strcpy(lastUploaded, uploadName);
          uploadDone = true;
        }
      } else {
        uploadOk = false;
        uploadErr = "Rename failed";
        LittleFS.remove(TMP_PATH);
      }
    } else {
      LittleFS.remove(TMP_PATH);
    }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) uploadFile.close();
    LittleFS.remove(TMP_PATH);
    uploadOk = false;
  }
}

void handleUploadFinished() {
  if (uploadErr.length() > 0) {
    server.send(400, "text/plain", uploadErr);
    return;
  }
  server.send(200, "text/plain", "OK");
}

// ---------- Ekran trybu upload: kody QR ----------
// Krok 0: kod QR z danymi sieci (aparat telefonu dolacza do WiFi).
// Krok 1: kod QR z adresem strony (aparat otwiera przegladarke). OK przelacza kroki.
// Kod ma wokol siebie spory pusty margines (min. 4 moduly), bo ciemna ramka wyswietlacza
// tuz obok kodu psuje jego odczyt.
#define QR_SCALE 3                   // px na modul kodu QR
#define QR_X 16                      // lewy brzeg kodu (px)
#define QR_Y(h) ((SCREEN_H - (h)) / 2) // gorny brzeg kodu: srodek wysokosci ekranu

void drawUpload() {
  char ssid[24];
  apSsid(ssid, sizeof(ssid));
  String pass = apPassword();
  char ip[20];
  snprintf(ip, sizeof(ip), "%s", WiFi.softAPIP().toString().c_str());
  static QrCode qr;
  char l[40];

  gfxClear();

  char payload[80];
  if (uploadStep == 0) snprintf(payload, sizeof(payload), "WIFI:T:WPA;S:%s;P:%s;;", ssid, pass.c_str());
  else snprintf(payload, sizeof(payload), "http://%s", ip);

  // Kod na srodku wysokosci ekranu, tytul nad kolumna tekstu: nad i pod kodem zostaje
  // co najmniej ~5 modulow pustego marginesu (wczesniej tytul stal tuz nad kodem).
  int tx = 104;                              // lewy brzeg kolumny tekstu
  if (qrEncode(payload, qr)) {
    int h = qr.size * QR_SCALE;
    uiQr(qr, QR_X, QR_Y(h), QR_SCALE);
    tx = QR_X + h + 12;                      // 12 px (4 moduly) odstepu od kodu
  }
  drawTitle(tx, uploadStep == 0 ? "Join WiFi" : "Open page");   // bez ikony WiFi: z nia tytul wchodzil w legende
  const int maxCh = (SCREEN_W - TAG_COL_W - tx) / 6;   // ile znakow 12 px miesci sie przed legenda

  if (uploadStep == 0) {
    gfxText(tx, 18, "Scan the code,", FOOT_SIZE);
    gfxText(tx, 31, "or pick WiFi:", FOOT_SIZE);
    gfxText(tx, 46, ssid, FONT_SIZE);
    gfxText(tx, 66, "password:", FOOT_SIZE);
    gfxText(tx, 79, pass.c_str(), FONT_SIZE);
  } else {
    gfxText(tx, 18, "Scan to open", FOOT_SIZE);
    gfxText(tx, 31, "the page, or type:", FOOT_SIZE);
    gfxText(tx, 46, ip, FONT_SIZE);
    gfxText(tx, 66, "in your browser", FOOT_SIZE);
  }

  if (lastUploaded[0]) {                     // po udanym wgraniu: nazwa ksiazki
    char sn[24];
    displayName(lastUploaded, sn, max(4, maxCh - 5));
    snprintf(l, sizeof(l), "Got: %s", sn);
    gfxText(tx, 97, l, FOOT_SIZE);
  }
  snprintf(l, sizeof(l), "Phones %d Req %d", (int)WiFi.softAPgetStationNum(), httpHits);
  gfxText(tx, 109, l, FOOT_SIZE);

  drawLegend("", uploadStep == 0 ? "NEXT" : "PREV", "STOP", false);
  gfxPresent();
  lastUploadDraw = millis();
}

void uploadStop() {
  server.stop();
#if CAPTIVE_TRICKS
  dns.stop();
#endif
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);                      // dopiero potem mozna bezpiecznie ponownie wlaczyc WiFi
}

// Uruchamia hotspot. Czysty start stosu WiFi i do 3 prob zmniejszaja ryzyko, ze siec "czasem sie nie pojawia"
// (np. przy szybkim ponownym wejsciu w Upload albo chwilowym spadku napiecia).
static bool apStart(const char* ssid, const char* pass) {
  WiFi.persistent(false);                     // nie zapisuj ustawien WiFi we flash
  for (int attempt = 1; attempt <= 3; attempt++) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(150);
    WiFi.mode(WIFI_AP);
    delay(50);
    WiFi.setSleep(false);
    if (WiFi.softAP(ssid, pass, AP_CHANNEL, 0, 4)) {                       // widoczna, do 4 klientow
      String ip = WiFi.softAPIP().toString();
      if (ip != "0.0.0.0") {
        // Mala moc nadawania: urzadzenie stoi zwykle tuz obok telefonu/komputera, a pelna moc z bliska
        // przesterowuje odbiornik (siec bywa wtedy widoczna jako "Hidden network") i obciaza zasilanie.
        static const wifi_power_t apPowerVals[3] = { WIFI_POWER_8_5dBm, WIFI_POWER_13dBm, WIFI_POWER_19_5dBm };
        WiFi.setTxPower(apPowerVals[cfg.apPower % 3]);
        Serial.printf("hotspot OK (proba %d): kanal %d, moc %d, wolna pamiec %u B\n", attempt, AP_CHANNEL, cfg.apPower, (unsigned)ESP.getFreeHeap());
        wifi_config_t wc;
        if (esp_wifi_get_config(WIFI_IF_AP, &wc) == ESP_OK) {          // diagnostyka: czy siec na pewno jest widoczna i szyfrowana
          Serial.printf("AP: ssid_len=%d ukryta=%d authmode=%d kanal=%d\n", (int)wc.ap.ssid_len, (int)wc.ap.ssid_hidden, (int)wc.ap.authmode, (int)wc.ap.channel);
        }
        return true;
      }
    }
    Serial.printf("hotspot: proba %d nieudana\n", attempt);
  }
  WiFi.mode(WIFI_OFF);
  return false;
}

void goUpload() {
  batteryMv();                     // zmierz baterie PRZED wlaczeniem WiFi (ADC2 przy WiFi nie dziala)

  // Siec startuje OD RAZU. Odswiezenie e-papieru trwa ok. 2 s, wiec rysowanie ekranu
  // przed uruchomieniem hotspotu opozniloby pojawienie sie sieci na telefonie.
  char ssid[24];
  apSsid(ssid, sizeof(ssid));
  String pass = apPassword();

  if (!apStart(ssid, pass.c_str())) {
    showMessage("Hotspot error", "Try again");
    delay(2000);
    goMenu();
    return;
  }
#if CAPTIVE_TRICKS
  dns.start(53, "*", WiFi.softAPIP());                  // kazda nazwa -> czytnik
#endif
  Serial.printf("Hotspot: %s, adres %s\n", ssid, WiFi.softAPIP().toString().c_str());

  lastUploaded[0] = '\0';
  uploadPath[0] = '\0';
  uploadErr = "";
  uploadDone = false;
  uploadStep = 0;
  httpHits = 0;
  statusDirty = false;
  LittleFS.remove(TMP_PATH);       // resztka po przerwanym uploadzie
  LittleFS.remove(WP_TMP);
  server.begin();
  screen = S_UPLOAD;
  powerApply();                    // hotspot: procesor min. 160 MHz
  drawUpload();
}

// ============================================================
//  Obsluga przyciskow dla kazdego ekranu
// ============================================================
void onKeyMenu(const KeyEvent& e) {
  if (e.key == KEY_UP && isStep(e)) {
    menuSel = (menuSel + menuCount - 1) % menuCount;      // z pierwszej pozycji na ostatnia
    listMoveOnly = true;
    drawMenu();
  } else if (e.key == KEY_DOWN && isStep(e)) {
    menuSel = (menuSel + 1) % menuCount;                  // z ostatniej na pierwsza
    listMoveOnly = true;
    drawMenu();
  } else if (e.key == KEY_BACK && isShort(e)) {            // BACK: od razu z powrotem do ksiazki
    char last[NAME_LEN];
    if (getLastBook(last, sizeof(last))) goReader(last);
  } else if (e.key == KEY_OK && isShort(e)) {
    switch (menuIds[menuSel]) {
      case M_CONTINUE: {
        char last[NAME_LEN];
        if (getLastBook(last, sizeof(last))) goReader(last);
        break;
      }
      case M_LIBRARY: goLibrary(); break;
      case M_UPLOAD: goUpload(); break;
      case M_STATS: goStats(); break;
      case M_SETTINGS: goSettings(); break;
      case M_INFO: goInfo(); break;
    }
  }
}

void onKeyLibrary(const KeyEvent& e) {
  if (e.key == KEY_UP && isStep(e) && bookCount > 0) {
    libSel = (libSel + bookCount - 1) % bookCount;
    listMoveOnly = true;
    drawLibrary();
  } else if (e.key == KEY_DOWN && isStep(e) && bookCount > 0) {
    libSel = (libSel + 1) % bookCount;
    listMoveOnly = true;
    drawLibrary();
  } else if (e.key == KEY_OK && isShort(e) && bookCount > 0) {
    char name[NAME_LEN];
    strcpy(name, bookNames[libSel]);
    goReader(name);
  } else if (e.key == KEY_BACK && isShort(e)) {
    goMenu();
  }
}

void onKeyReader(const KeyEvent& e) {
  if (e.key == KEY_DOWN && isStep(e)) {
    turnPages(accelStep(e.repeat));
  } else if (e.key == KEY_UP && isStep(e)) {
    turnPages(-accelStep(e.repeat));
  } else if (e.key == KEY_OK && isShort(e)) {
    goGoto();
  } else if (e.key == KEY_OK && isLong(e)) {
    cycleFontSize();
  } else if (e.key == KEY_BACK && isShort(e)) {
    saveBookmark();
    closeBook();
    goLibrary();
  } else if (e.key == KEY_BACK && isLong(e)) {           // dlugie BACK: czcionka, interlinia itd. bez przechodzenia przez menu
    saveBookmark();
    strncpy(settingsResumeBook, curBook, NAME_LEN - 1);
    settingsResumeBook[NAME_LEN - 1] = '\0';
    closeBook();
    screen = S_SETTINGS;
    settingsFromReader = true;
    setGroup = 0;                            // grupa Reading
    setSel = 0;
    drawSettings();
  } else if (e.key == KEY_HOME && isShort(e)) {
    saveBookmark();
    closeBook();
    goMenu();
  }
}

// ============================================================
//  Gleboki sen
// ============================================================
unsigned long lastActivityMs = 0;

void goSleep() {
  if (clockValid()) prefs.putUInt("clk_last", (uint32_t)clockNow());   // domyslna wartosc przy recznym ustawianiu po utracie zasilania
  Serial.println("sleep");
  statsFlush();              // dolicz czas czytania (tylko, gdy czytamy) i zapisz statystyki
  saveBookmark();            // bezpieczne takze poza czytnikiem (ma zabezpieczenia)
  if (fsOk && cfg.sleepScreen != 2) drawSleepScreen();   // karta ksiazki albo obraz (jedno odswiezenie)
  if (fsOk) LittleFS.end();
  bmUpdate();                // dolicz czas pracy do snu
  prefs.putUInt("bm_slp", clockValid() ? (uint32_t)clockNow() : 0);   // po wybudzeniu dolicz czas snu
  bmSave();
  boardDeepSleep();          // nie wraca; po wybudzeniu setup() startuje od nowa
}

// ============================================================
//  setup / loop
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("InkBlink start");

  boardInit();
  fontInit();

  // Formatowanie kasuje ksiazki, wiec tylko za zgoda (przycisk OK).
  // Przy pierwszym uruchomieniu na swiezej partycji tak ma byc.
  fsOk = LittleFS.begin(false);
  if (!fsOk) {
    showMessage("No filesystem", "OK = format", "(erases all books)");
    while (boardReadKey() != KEY_OK) delay(10);
    fsOk = LittleFS.begin(true);
  }
  Serial.println(fsOk ? "LittleFS OK" : "LittleFS ERROR");

  prefs.begin("reader", false);
  loadSettings();
  statsLoadTotals();
  bmLoad();
  bmBootSleepAccount();
  powerApply();
  WiFi.onEvent(onWifiEvent);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/delete", HTTP_POST, handleDelete);
  server.on("/config", HTTP_GET, handleConfigGet);
  server.on("/config", HTTP_POST, handleConfigPost);
  server.on("/upload", HTTP_POST, handleUploadFinished, handleUploadData);
  server.on("/wallpaper", HTTP_GET, handleWallpaperPage);
  server.on("/wp", HTTP_POST, handleWpFinished, handleWpData);
  server.on("/wp_reset", HTTP_POST, handleWpReset);
  server.on("/time", HTTP_POST, handleTime);
  server.on("/battlog.csv", HTTP_GET, handleBattLog);
  server.onNotFound(handleNotFound);

  curBook[0] = '\0';
  lastUploaded[0] = '\0';
  uploadPath[0] = '\0';

  boardWaitRelease();        // przycisk budzacy nie moze zadzialac jako komenda

  buildMenu();
  screen = S_MENU;
  char last[NAME_LEN];
  if (cfg.autoOpen && getLastBook(last, sizeof(last))) {
    goReader(last);          // przy bledzie sam pokaze menu
  } else {
    drawMenu();
  }
  lastActivityMs = millis();
}

void loop() {
  bmUpdate();
  if (screen == S_UPLOAD) {
#if CAPTIVE_TRICKS
    dns.processNextRequest();
#endif
    server.handleClient();
    if (uploadDone) {
      uploadDone = false;
      statusDirty = false;
      drawUpload();
    } else if (statusDirty && !uploadFile && millis() - lastUploadDraw > 4000) {
      statusDirty = false;       // licznik odswiezamy rzadko i nigdy w trakcie wysylania pliku
      drawUpload();
    }
  }

  // zakladka: zapis po 4 s bezczynnosci (oszczedza flash)
  if (screen == S_READER && bmDirty && millis() - lastTurnMs > 4000) {
    saveBookmark();
  }

  KeyEvent e = inputPoll();
  if (e.type == EV_NONE) {
    unsigned long lim = (unsigned long)sleepMinutes[cfg.sleepIdx] * 60000UL;
    if (lim && screen != S_UPLOAD && millis() - lastActivityMs > lim) goSleep();
    idleWait();                 // delay(5) albo lekkie uspienie (Power saving)
    return;
  }
  lastActivityMs = millis();

  // skrot: dlugie HOME = uspij teraz (poza trybem upload)
  if (e.key == KEY_HOME && isLong(e) && screen != S_UPLOAD) {
    goSleep();
    return;
  }

  switch (screen) {
    case S_MENU: onKeyMenu(e); break;
    case S_LIBRARY: onKeyLibrary(e); break;
    case S_READER: onKeyReader(e); break;
    case S_UPLOAD:
      if (e.key == KEY_BACK && isShort(e)) {
        uploadStop();
        goMenu();
      } else if (e.key == KEY_OK && isShort(e)) {      // przelacz kod QR: siec / strona
        uploadStep = 1 - uploadStep;
        drawUpload();
      }
      break;
    case S_INFO:
      if (e.key == KEY_BACK && isShort(e)) goMenu();
      else if (e.key == KEY_OK && isShort(e) && !batteryAvailable()) goPinScan();
      break;
    case S_STATS:
      if (e.key == KEY_BACK && isShort(e)) goMenu();
      break;
    case S_PINSCAN:
      if (e.key == KEY_BACK && isShort(e)) goInfo();
      break;
    case S_GOTO: onKeyGoto(e); break;
    case S_SETTINGS: onKeySettings(e); break;
    case S_SETTIME: onKeySetTime(e); break;
    case S_SETBATT: onKeySetBatt(e); break;
  }
  lastActivityMs = millis();
}
