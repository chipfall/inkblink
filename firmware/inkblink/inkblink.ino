// ============================================================
//  InkBlink - e-book reader firmware for the CrowPanel ESP32 2.13" e-paper (DIE01021S)
//  Menu, Library, Reader, Statistics, Settings, Upload (own WiFi hotspot + web page)
//  Books (.txt) live in LittleFS and are read page by page.
//
//  Changes in 0.8:
//   - hotspot: QR codes (join the network, open the page), WiFi icon
//   - web page in English, new look, multi-file upload with a progress bar,
//     sleep image uploaded through the browser
//   - battery indicator (once the pin is set in board_crowpanel.h), pin test
//   - sleep screen: book card / image / unchanged
//   - statistics: reading time, speed, estimated time to the end of the book
//   - fast page flip (experiment): partial refresh
//
//  Changes in 0.7: phone and request counters on the hotspot screen, diagnostics.
//  Changes in 0.6: key legend, upload only through the hotspot.
//  Changes in 0.5: fonts, hold-to-repeat navigation. 0.4: Polish characters, settings.
// ============================================================

// 1 = Helvetica and Unifont families (needs the libraries Adafruit GFX Library
// and U8g2_for_Adafruit_GFX). 0 = Classic only, no extra libraries.
#define USE_U8G2 1

#include <Arduino.h>
#if USE_U8G2
#include <Adafruit_GFX.h>              // must come BEFORE our headers (BLACK/WHITE macros)
#include <U8g2_for_Adafruit_GFX.h>
#endif
#include <WiFi.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <strings.h>

#include "qr.h"                 // QR code generator
#include "board_crowpanel.h"    // pins, buttons, drawing, battery
#include "ui_gfx.h"             // icons, QR code, rectangles
#include "input.h"              // button events
#include "clock.h"
#include "hyphen.h"
#include "battery_model.h"
#include "reader_fonts.h"       // book fonts
#include "webui_assets.h"       // web page (HTML/CSS/JS)

#define FW_VERSION "0.9.14"         // 0.9.N = package/update N (see docs/DEVELOPMENT.md); must match CHANGELOG.md

// ---------- Screen / interface settings ----------
#define FONT_SIZE 16            // interface font (menus, messages)
#define FOOT_SIZE 12            // status line font
#define UI_CHARS 24             // max characters in an interface line (fits before the key legend)
#define UI_X 4                  // left margin of the interface (px)
#define LIST_ROWS 6
#define MARGIN_X 6              // left/right margin of the book text (px)
#define STATUS_Y 112            // top edge of the status line (px)

// ---------- Limits ----------
#define MAX_PAGES_CAP 60000     // 60000 pages = 240 KB of RAM for offsets
#define MAX_BOOKS 40
#define MAX_CH 300                // max number of chapters in the table of contents
#define TOC_MAX_BYTES 24000       // max size of a .toc file
#define NAME_LEN 48
#define PAGE_BUF 1536           // bytes per page (UTF-8 can take 2-4 bytes per character)
#define CHUNK_BUF 4096

// ---------- Index cache ----------
#define USE_INDEX_CACHE 1
#define INDEX_MIN_BOOK 100000   // smaller books are not cached
#define IDX_MAGIC 0x58444945UL
#define HYPHEN_VERSION 2       // change when the hyphenation rules change
#define WRAP_VERSION 5          // change when the wrapping algorithm or margins change (5: new character width measurement)

#define TMP_PATH "/upload.tmp"

// ---------- Hotspot ----------
#define AP_SSID_PREFIX "InkBlink-"  // network name: InkBlink-XXXX (suffix from the board address)
#define AP_CHANNEL 1                // WiFi channel of the hotspot (1-13); with interference try 6 or 11
#define CAPTIVE_TRICKS 1            // 1 = DNS + answers to connectivity checks (see handleNotFound)

// ---------- Sleep image ----------
#define WALLPAPER_PATH "/wallpaper.bin"
#define WP_TMP "/wp.tmp"
#define WALLPAPER_BYTES 3904        // 32 bytes per row * 122 rows (250 px, most significant bit first, 1 = ink)

// ---------- Battery / statistics ----------
#define BAT_LOW_PCT 20              // below this level the icon is also shown in "Low" mode
#define STATS_MAX_GAP_S 180         // a longer pause between pages does not count as reading
#define STATS_MIN_TURN_S 2          // faster page turns are browsing and do not count towards the speed
#define STATS_SAVE_EVERY 25         // statistics are saved every this many page turns

typedef const char* (*ItemFn)(int);

// Reading statistics (the structs must come before the first function - see the note about ItemFn)
struct BookStats {
  uint32_t secs;       // reading time of this book (s)
  uint32_t spdBytes;   // bytes read page by page (for the speed)
  uint32_t spdSecs;    // time it took to read them
  uint32_t pages;      // pages turned
  uint8_t done;        // 1 = finished (>= 98%)
  uint8_t pad[3];
};
struct TotalStats {
  uint32_t secs, spdBytes, spdSecs, pages;
  uint16_t finished;   // finished books
  uint16_t pad;
};

// ============================================================
//  User settings (stored in non-volatile memory)
// ============================================================
static const uint8_t sleepMinutes[5] = { 1, 5, 10, 30, 0 };   // 0 = never
static const uint8_t fullEveryVals[3] = { 5, 10, 20 };        // full refresh every N pages
static const uint8_t marginVals[3] = { 8, 4, 14 };               // text margins (px): Normal, Narrow, Wide
static const uint8_t lineGapVals[3] = { 0, 2, 4 };               // line spacing: extra px between lines

struct Settings {
  uint8_t famIdx;      // font family (FAM_*)
  uint8_t fontIdx;     // size index within the family
  bool bold;           // bold
  bool showPct;        // progress percentage in the status line
  bool showPage;       // page number n/N in the status line
  bool showLeft;       // estimated time to the end of the book
  bool showBar;        // progress bar (fills the rest of the line)
  uint8_t batMode;     // battery in the book: 0 off, 1 only when low, 2 always
  bool fastFlip;       // fast page flip (partial refresh)
  uint8_t fullEvery;   // index into fullEveryVals
  uint8_t sleepIdx;    // index into sleepMinutes
  uint8_t sleepScreen; // sleep screen (Cover): 0 book card (Book info), 1 image, 2 current page
  bool autoOpen;       // open the last book after start
  uint8_t margin;      // text margin: 0 normal, 1 narrow, 2 wide (marginVals)
  uint8_t lineGap;     // line spacing: index into lineGapVals
  bool hyphen;         // automatic hyphenation
  uint8_t clockMode;   // clock: 0 off, 1 24h, 2 12h
  bool clockBook;      // clock in the status line of a book
  uint8_t tz;          // time zone (index into TZS)
  bool fastMenu;       // fast (no flash) refresh of menu lists
  uint8_t uiFont;      // menu and title font: 0 Classic, 2 Helvetica 12 (1 = former Helvetica 14 and 3 = former Unifont, treated as 2)
  uint8_t apPower;     // hotspot transmit power: 0 low (8.5 dBm), 1 medium (13 dBm), 2 high (19.5 dBm)
  uint8_t powerSave;   // power saving: 0 off, 1 processor 80 MHz, 2 light sleep when idle
  bool battEst;        // battery estimate (the board has no sensor)
  uint8_t battCap;     // cell capacity: index into BM_CAPS
};
Settings cfg = { FAM_CLASSIC, 1, false, true, true, false, true, 1, false, 1, 1, 0, true, 0, 0, true, 0, false, 0, true, 2, 1, 2, true, 2 };
bool devMode = false;    // hidden options (Developer): toggled by holding MENU for 10 s in the main menu

// Is the status line drawn at all (affects the page layout, so the battery does not change it)
// Is at least one status line item on (the main "Status line" switch shows this state)
bool statusAnyOn() {
  return cfg.showPct || cfg.showPage || cfg.showLeft || cfg.showBar || cfg.clockBook || cfg.batMode > 0;
}
// Main switch: turns all status line items on or off at once
void statusSetAll(bool on) {
  cfg.showPct = cfg.showPage = cfg.showLeft = cfg.showBar = cfg.clockBook = on;
  cfg.batMode = on ? 1 : 0;                  // 1 = Low (battery only when it is low)
}

bool statusVisible() {
  return cfg.showPct || cfg.showPage || cfg.showLeft || cfg.showBar || (cfg.clockMode > 0 && cfg.clockBook);
}

// ---------- Current page layout (set by applyLayout) ----------
int linesPerPage = 7;
int maxTextW = SCREEN_W - 2 * MARGIN_X;   // text width in pixels
int textX0 = MARGIN_X;

void applyLayout() {
  if (cfg.famIdx >= fontFamilyCount()) cfg.famIdx = FAM_CLASSIC;
  if (cfg.fontIdx >= fontSizeCount(cfg.famIdx)) cfg.fontIdx = fontSizeCount(cfg.famIdx) - 1;   // e.g. former 24/32 px -> the largest available
  if (cfg.margin > 2) cfg.margin = 0;
  if (cfg.lineGap > 2) cfg.lineGap = 0;
  if (cfg.clockMode > 2) cfg.clockMode = 0;
  if (cfg.tz >= TZ_COUNT) cfg.tz = 0;
  if (cfg.uiFont == 1 || cfg.uiFont == 3) cfg.uiFont = 2;   // former Helvetica 14 (too tight) and Unifont (removed)
  if (cfg.uiFont > 3) cfg.uiFont = 0;
  if (cfg.apPower > 2) cfg.apPower = 1;
  if (cfg.powerSave > 2) cfg.powerSave = 0;
  if (cfg.battCap > 5) cfg.battCap = 2;
  fontSetLineGap(lineGapVals[cfg.lineGap]);
  fontSelect(cfg.famIdx, cfg.fontIdx, cfg.bold);

  int mx = marginVals[cfg.margin];
  int areaW = SCREEN_W - 2 * mx;
  int fw = fontFixedW();
  if (fw > 0) {                      // fixed width: centre the rest
    maxTextW = (areaW / fw) * fw;
    textX0 = mx + (areaW - maxTextW) / 2;
  } else {                           // proportional
    maxTextW = areaW;
    textX0 = mx;
  }
  int availH = (statusVisible() ? STATUS_Y : SCREEN_H) - fontTopPad();
  linesPerPage = availH / fontLineH();
  if (linesPerPage < 1) linesPerPage = 1;
}

// Layout signature: a book paginates differently for every combination of settings
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
//  Text wrapping (one function for indexing and drawing)
//  Measures WIDTH IN PIXELS and never splits a UTF-8 sequence.
// ============================================================
inline int charBytes(uint8_t c) {
  if (c < 0x80) return 1;
  if ((c & 0xE0) == 0xC0) return 2;
  if ((c & 0xF0) == 0xE0) return 3;
  if ((c & 0xF8) == 0xF0) return 4;
  return 1;                       // stray continuation byte
}

// Width (px) of the first len bytes of a string
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

// Tries to split a word that does not fit (the word starts ws bytes from pos).
// Returns true and sets outBytes (line length) and needHyphen (whether to draw a hyphen).
bool tryHyphenate(const char* t, int pos, int ws, int& outBytes, bool& needHyphen) {
  const char* w = t + pos + ws;
  int we = 0;
  while (we < 120) {
    char c = w[we];
    if (c == '\0' || c == ' ' || c == '\n' || c == '\r') break;
    we++;
  }
  // candidates: (offset from the start of the word, needs a hyphen)
  uint8_t candOff[24];
  bool candHy[24];
  int nc = 0;
  int i = 0;
  while (i < we && nc < 22) {
    // run of letters: from i to the first non-letter character (-, punctuation...)
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
      if (w[i] == '-' && i + 1 < we && i > 0 && nc < 22) { candOff[nc] = (uint8_t)(i + 1); candHy[nc++] = false; }   // the text already has a hyphen here
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

#define HY_MAX_RUN 2                  // at most this many consecutive lines ending with a hyphen
int hyRun = 0;                         // current run of such lines; reset at the start of every page

// Wrapping core: line length in bytes; hyph = a hyphen must be drawn after the line.
int layoutLine(const char* t, int pos, int& advance, bool& hyph) {
  hyph = false;
  int bytes = 0;
  int w = 0;
  int lastSpace = -1;             // position (in bytes) of the last space

  for (;;) {
    uint8_t c = (uint8_t)t[pos + bytes];
    if (c == '\0' || c == '\n' || c == '\r') break;
    int cb = charBytes(c);
    for (int k = 1; k < cb; k++) {            // sequence cut off at the end of the buffer
      if (t[pos + bytes + k] == '\0') { cb = k; break; }
    }
    uint32_t cp;
    gfxU8Decode(t + pos + bytes, cb, cp);
    int cw = fontCharW(cp);
    if (w + cw > maxTextW && bytes > 0) break;   // does not fit
    if (c == ' ') lastSpace = bytes;
    bytes += cb;
    w += cw;
  }

  char next = t[pos + bytes];
  // the word does not fit: split it (if enabled), as a last resort move it to the next line
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

// NUL and tab -> space (same behaviour when indexing and drawing)
void sanitizeBuf(char* b, int n) {
  for (int i = 0; i < n; i++) {
    if (b[i] == '\0' || b[i] == '\t') b[i] = ' ';
  }
}

// ============================================================
//  Tools: file names, keys
// ============================================================
bool endsWithTxt(const char* s) {
  size_t l = strlen(s);
  return l > 4 && strcasecmp(s + l - 4, ".txt") == 0;
}

// Safe book name: no paths, only [0-9A-Za-z._-], always ends
// in .txt (the stem is shortened, the extension is not). Polish letters are
// transliterated (ą -> a), other characters that are not allowed become '_'.
// Returns false when it is not a .txt file.
static bool makeNameExt(const char* in, char* out, const char* ext) {
  const char* b = in;
  const char* s = strrchr(b, '/');
  if (s) b = s + 1;
  s = strrchr(b, '\\');
  if (s) b = s + 1;

  int l = (int)strlen(b);
  if (l <= 4 || strcasecmp(b + l - 4, ext) != 0) return false;

  int end = l - 4;               // end of the stem (without the extension)
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
  memcpy(out + j, ext, 5);       // including '\0'
  return true;
}

bool makeBookName(const char* in, char* out) {
  return makeNameExt(in, out, ".txt");
}
// Table of contents (chapters) of a book: same name, extension .toc (made by the web page when importing an EPUB)
bool makeTocName(const char* in, char* out) {
  return makeNameExt(in, out, ".toc");
}
// "/Name.toc" for the book "Name.txt"
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

// name without .txt, cut to maxChars
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

// Bookmark key: name + size (a replaced book does not inherit the old position)
void bmKey(const char* name, uint32_t size, char* key) {
  uint32_t h = nameHash(name) ^ (size * 2654435761UL);
  snprintf(key, 16, "o%08lx", (unsigned long)h);
}

// Index file: /i<name hash>_<layout signature>.idx (the header checks the book size)
void idxPath(const char* name, uint32_t sig, char* path, size_t n) {
  snprintf(path, n, "/i%08lx_%08lx.idx", (unsigned long)nameHash(name), (unsigned long)sig);
}

// Removes all index files of a book (for every page layout)
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
//  Application state
// ============================================================
enum Screen { S_MENU, S_LIBRARY, S_READER, S_UPLOAD, S_INFO, S_GOTO, S_SETTINGS, S_STATS, S_PINSCAN, S_SETTIME, S_SETBATT };
Screen screen = S_MENU;
bool fsOk = false;
Preferences prefs;

// ---------- Hotspot: name and password ----------
// Network name: InkBlink-XXXX (suffix from the unique board address)
void apSsid(char* out, size_t n) {
  uint64_t mac = ESP.getEfuseMac();
  snprintf(out, n, "%s%04X", AP_SSID_PREFIX, (unsigned)((mac >> 32) & 0xFFFF));
}

// Password: 8 digits, drawn ONCE and remembered - the phone remembers the network and connects
// by itself next time. It can be replaced in Settings.
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
enum MenuId { M_CONTINUE, M_LIBRARY, M_UPLOAD, M_STATS, M_SETTINGS, M_INFO, M_DEV };
MenuId menuIds[7];
const char* menuLabels[7];
int menuCount = 0;
int menuSel = 0;

// ---------- Library ----------
char bookNames[MAX_BOOKS][NAME_LEN];
int bookCount = 0;
int libSel = 0;

// ---------- Reader ----------
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
bool uploadIsToc = false;          // the file being uploaded is a table of contents (.toc), not a book
uint32_t uploadBytes = 0;
char uploadPath[NAME_LEN + 2];
char lastUploaded[NAME_LEN];
String uploadErr;

// ---------- Reading statistics ----------
BookStats bst;
TotalStats tst;
unsigned long statLastTurn = 0;   // when the last page was shown (0 = not reading)
int statTurns = 0;

// ---------- Battery ----------
int batMv = -1;
unsigned long batAt = 0;

// ---------- Fast page flip ----------
int fastLeft = 0;                 // fast refreshes left until the next full one

// ---------- Upload: screen step (0 = join the network, 1 = open the page) ----------
int uploadStep = 0;

// ---------- Sleep image: upload ----------
File wpFile;
bool wpOk = false;
String wpErr;
size_t wpBytes = 0;

// ---------- Hotspot diagnostics ----------
volatile bool statusDirty = false;     // something changed: refresh the counter on the screen
int httpHits = 0;                      // how many HTTP requests reached the reader
unsigned long lastUploadDraw = 0;
#define DEV_HOLD_MS 10000UL               // how long MENU must be held in the main menu (developer mode)
unsigned long devHoldMs = 0;              // != 0: MENU held since this moment (main menu)

// ============================================================
//  Helper screens
// ============================================================
void showMessage(const char* a, const char* b = nullptr, const char* c = nullptr) {
  gfxClear();
  gfxText(UI_X, 30, a, FONT_SIZE);
  if (b) gfxText(UI_X, 52, b, FONT_SIZE);
  if (c) gfxText(UI_X, 74, c, FONT_SIZE);
  gfxPresent();
}

// Key legend at the right edge, level with the physical keys:
//   top = HOME key (MENU), mid = wheel click (OK), bot = EXIT key.
// Empty text = the key does nothing here (or the same as another key, so it gets no label).
// arrows = triangles above and below the middle label (the wheel scrolls up/down).
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

// Are menus and titles drawn with a U8g2 font ("Menu font" setting; without U8g2 always Classic)
#if USE_U8G2
inline bool uiHelvOn() { return cfg.uiFont >= 1 && cfg.uiFont <= 3; }
#else
inline bool uiHelvOn() { return false; }
#endif

// Screen title: bold in Helvetica; plain in Classic (emboldening dense fonts
// merged "M" and "W" into a black block).
void drawTitle(int x, const char* t) {
#if USE_U8G2
  if (uiHelvOn()) {
    uiText(cfg.uiFont, x, 0, t, true);
    return;
  }
#endif
  gfxText(x, 0, t, FONT_SIZE);
}

// Draws a list into the buffer (without sending it to the screen)
void drawListBody(const char* title, ItemFn item, int count, int sel,
                  const char* top, const char* mid, const char* bot) {
  gfxClear();
  drawTitle(UI_X, title);

  int top0 = 0;
  if (sel >= LIST_ROWS) top0 = sel - LIST_ROWS + 1;

  for (int r = 0; r < LIST_ROWS && top0 + r < count; r++) {
#if USE_U8G2
    if (uiHelvOn()) {
      if (top0 + r == sel) gfxText(UI_X, 16 + r * 16, ">", FONT_SIZE);          // cursor
      char txt[UI_CHARS + 1];
      snprintf(txt, sizeof(txt), "%s", item(top0 + r));
      int maxW = SCREEN_W - TAG_COL_W - (UI_X + 16) - 2;                         // before the key legend column
      int n = (int)strlen(txt);
      while (n > 0 && uiWidth(cfg.uiFont, txt, false) > maxW) {                          // safety: never run under the key legend
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

// Refreshing list screens. When only the selection changes (listMoveOnly) we do a fast
// partial refresh without flashing; entering a screen and every MENU_FULL_EVERY moves - full.
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
//  Time: formatting
// ============================================================
// "1h20m", "35m" or "<1m"
void fmtDuration(uint32_t s, char* out, size_t n) {
  uint32_t m = s / 60, h = m / 60;
  m %= 60;
  if (h > 0) snprintf(out, n, "%uh%02um", (unsigned)h, (unsigned)m);
  else if (s >= 60) snprintf(out, n, "%um", (unsigned)m);
  else snprintf(out, n, "<1m");
}

// "12h 34m" / "34m" / "0m" (for the statistics screen)
void fmtDurationLong(uint32_t s, char* out, size_t n) {
  uint32_t m = s / 60, h = m / 60;
  m %= 60;
  if (h > 0) snprintf(out, n, "%uh %um", (unsigned)h, (unsigned)m);
  else snprintf(out, n, "%um", (unsigned)m);
}

// ============================================================
//  Power saving ("Power saving" setting)
//   0 = processor at 240 MHz, 1 = 80 MHz (hotspot 160 MHz, book indexing 240 MHz),
//   2 = like 1 + light sleep when idle (woken by a button), only outside the hotspot.
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
// Light sleep in menus only after a short pause without buttons (scrolling without wake-up lag).
// In a book at once: there was no lag there, and 1.5 s at 80 MHz after every page doubled its cost.
#define PS_LIGHT_GRACE_MS 1500
extern unsigned long lastActivityMs;
static void idleWait() {
  unsigned long grace = (screen == S_READER) ? 0 : PS_LIGHT_GRACE_MS;
  if (cfg.powerSave == 2 && screen != S_UPLOAD && millis() - lastActivityMs >= grace &&
      boardReadKey() == KEY_NONE) boardLightSleep(250);
  else delay(5);
}

// ============================================================
//  Battery estimate (see battery_model.h)
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
//  Battery
// ============================================================
bool batteryAvailable() {
  return PIN_BAT_ADC >= 0 || batteryIsEstimate();
}

// Approximate state of charge of a Li-Po cell from its voltage (mV)
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

// Battery voltage in mV from the cache (refreshed every 30 s); -1 = no measurement
int batteryMv() {
  if (PIN_BAT_ADC < 0) return -1;             // without a sensor there is no voltage, only the estimated percentage
  if (screen == S_UPLOAD) return batMv;       // ADC2 does not work with WiFi on - use the last reading
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

// ---- saving, updating and logging the model ----
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
// Capacity changed in the settings: keep the percentage
void bmCapSync() {
  float pct = bmPercent(bm);
  bm.capMah = BM_CAPS[cfg.battCap % 6];
  if (bm.valid) bm.remMah = bm.capMah * pct / 100.0f;
}
// Called on every loop pass: adds the awake time and screen refreshes since the previous call
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
// After waking from deep sleep: add the sleep time (needs the clock set before and after sleeping)
void bmBootSleepAccount() {
  uint32_t t0 = prefs.getUInt("bm_slp", 0);
  if (t0 && esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED && clockValid() && bm.valid) {
    long d = (long)clockNow() - (long)t0;
    if (d > 0 && d < 60L * 86400L) bmAddSleep(bm, (float)d);
  }
  prefs.putUInt("bm_slp", 0);
}
// Appends a row to /battlog.csv (counters since the previous setting + the estimate before and after)
static void bmLog(const char* note, float volt, float pctBefore, float pctAfter) {
  if (!fsOk) return;
  File r = LittleFS.open("/battlog.csv", "r");
  size_t sz = r ? r.size() : 0;
  if (r) r.close();
  if (sz > 12000) {                          // the log does not grow forever
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

// Top right corner: clock (if on) and battery ("87%" + icon)
void drawBatteryCorner() {
  int right = SCREEN_W - 4;                      // right edge of the clock
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
    if (!clockFormat(c, sizeof(c), cfg.clockMode)) snprintf(c, sizeof(c), "--:--");   // time not set yet
    gfxText(right - gfxTextWidth(c, FOOT_SIZE), 1, c, FOOT_SIZE);
  }
}

// ============================================================
//  Reading statistics
// ============================================================
int readPercent();                     // (defined in the Reader section)
bool getLastBook(char* out, size_t n); // (defined in the Menu section)

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

// Call after a page is shown in the reader (starts counting time)
void statsTouch() {
  statLastTurn = millis();
}

// Call when the page changes (after setting the new page value)
void statsOnTurn(int fromPage, int toPage) {
  unsigned long now = millis();
  if (statLastTurn) {
    uint32_t dt = (uint32_t)((now - statLastTurn) / 1000UL);
    if (dt > 0 && dt <= STATS_MAX_GAP_S) {
      bst.secs += dt;
      tst.secs += dt;
    }
    // speed is only measured from ordinary one-page turns
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

// Ends a reading session (leaving the reader, sleep): adds the last page and saves
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

// Estimated time to the end (s) from this book's speed, or the average speed when that is missing; -1 = unknown
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

// Last book (name, size, bookmark position) without opening it for reading
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
//  Sleep screen: book card or image
// ============================================================
// Image uploaded through the browser (/wallpaper.bin): 250x122, 1 bit, 1 = ink
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

// Built-in image: moon, stars and "Zzz"
void drawDefaultWallpaper() {
  uiDisc(196, 44, 26, true);                  // crescent moon = disc minus a shifted disc
  uiDisc(208, 36, 23, false);
  static const int8_t stars[8][3] = {         // x, y, size
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

// Title from the file name: without .txt, '_' -> space, wrapped to two lines
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

// Card of the current (or last) book. false = there is no book at all.
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

  uiRect(6, 64, 243, 73);                         // progress bar
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
//  Library
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

  // alphabetical sorting
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
//  Reader
// ============================================================
void saveBookmark() {
  bmDirty = false;
  if (totalPages <= 0 || !pageOff || curBook[0] == '\0') return;
  char key[16];
  bmKey(curBook, bookSize, key);
  prefs.putUInt(key, pageOff[page]);
  prefs.putString("last", curBook);
}

// ---------- Table of contents (chapters) ----------
uint32_t chOff[MAX_CH];            // offsets of chapter starts (bytes in the book file)
char chTitle[MAX_CH][32];
int chCount = 0;

// cuts a string to maxBytes bytes without splitting a UTF-8 character
void utf8Trunc(char* s, int maxBytes) {
  if ((int)strlen(s) <= maxBytes) return;
  int n = maxBytes;
  while (n > 0 && ((uint8_t)s[n] & 0xC0) == 0x80) n--;
  s[n] = '\0';
}
// cuts a string to maxChars characters
void utf8CutChars(char* s, int maxChars) {
  int i = 0, ch = 0;
  while (s[i]) {
    if (ch == maxChars) { s[i] = '\0'; return; }
    i += charBytes((uint8_t)s[i]);
    ch++;
  }
}

// Adds one "offset<TAB>title" line to the table of contents (skips invalid and non-increasing ones)
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
        else skipRest = true;                // line too long: ignore the rest
      }
    }
  }
  if (ll > 0) {                              // last line without a line ending
    line[ll] = '\0';
    tocAddLine(line, bookBytes);
  }
  f.close();
}

// chapter that contains a given file offset (0 when before the first one)
int chapterOfOffset(uint32_t off) {
  int r = 0;
  for (int i = 0; i < chCount; i++) {
    if (chOff[i] <= off) r = i;
    else break;
  }
  return r;
}

int findPage(uint32_t off);   // (defined below, in the book section)

// Chapter of a page (numbered from 0): the one that starts on this page or earlier - counted from
// the END of the page, so a page with a chapter start in the middle already belongs to the new chapter.
int chapterOfPage(int p) {
  uint32_t endOff = (p + 1 < totalPages) ? pageOff[p + 1] - 1 : bookSize;
  return chapterOfOffset(endOff);
}
int chapterStartPage(int ci) {       // page number (from 1) where the chapter starts
  return findPage(chOff[ci]) + 1;
}

void closeBook() {
  statsFlush();                       // add the reading time and save the statistics
  if (book) book.close();
  if (pageOff) {
    free(pageOff);
    pageOff = nullptr;
  }
  maxPages = 0;
  totalPages = 0;
  chCount = 0;
}

// Allocates the offset table (PSRAM if present; otherwise the normal heap).
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

// Reads the file once and stores where every page starts.
bool buildIndex(uint32_t startPos) {
  static char chunk[CHUNK_BUF];

  // a page is usually >= 100 B; 2.5x headroom
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
      // not enough data in this chunk: read the next one (unless at the end of the file)
      if (!eof && n - off < PAGE_BUF) break;
      pageOff[totalPages++] = pos + off;
      int used = consumePage(chunk + off);
      if (used <= 0) used = 1;
      off += used;
    }
    if (off <= 0) off = n;   // guard against an endless loop
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
  Serial.printf("index from cache: %d pages\n", totalPages);
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
  if (!ok) LittleFS.remove(p);   // an unfinished index must not stay
  Serial.println(ok ? "index saved" : "index: write error");
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

  // skip the UTF-8 BOM
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
  Serial.printf("book: %s, %u B, %d pages\n", curBook, (unsigned)bookSize, totalPages);
  return true;
}

// Opening a book (including indexing) at full processor speed, then back to the configured speed
bool openBook(const char* name) {
  if (cfg.powerSave > 0) cpuSet(240);
  bool ok = openBookBody(name);
  powerApply();
  return ok;
}

// Reading progress in percent from the position in the file
int readPercent() {
  if (page >= totalPages - 1) return 100;
  if (bookSize == 0) return 0;
  return (int)(((uint64_t)pageOff[page] * 100) / bookSize);
}

// Status line: the items chosen by the user, and the progress bar
// fills all the remaining space in the line.
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
    else snprintf(d, sizeof(d), "--");     // reading speed not known yet (needs a few pages)
    size_t l = strlen(st);
    snprintf(st + l, sizeof(st) - l, "%s~%s", l ? "  " : "", d);
  }

  if (cfg.clockMode > 0 && cfg.clockBook) {          // clock in the book (only when the time is set)
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

  // battery at the right edge: always or only when low (Settings)
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

// fastOk = a fast (partial) refresh may be used, if enabled in Settings.
// Every few pages a full refresh removes traces of the previous pages.
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
    if (hy && len < 240) {                       // line broken in the middle of a word: draw a hyphen
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

void drawMenu();   // declaration (used in goReader)

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

// Turns 'delta' pages (limited to the valid range); draws only when something changed
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

// Acceleration while held: n = repeat number (0 = first press)
int accelStep(int n) {
  static const uint8_t t[6] = { 1, 1, 3, 10, 25, 50 };
  return t[n < 5 ? n : 5];
}

// Shortcut (long OK in the reader): next font size, reading continues at the same place
void cycleFontSize() {
  int cnt = fontSizeCount(cfg.famIdx);
  if (cnt < 2) return;
  char name[NAME_LEN];
  strcpy(name, curBook);
  saveBookmark();                              // position saved as a byte offset
  cfg.fontIdx = (cfg.fontIdx + 1) % cnt;
  prefs.putUChar("cfg_fs", cfg.fontIdx);
  goReader(name);                              // repaginates and returns to the same passage
}

// ============================================================
//  Go to page (OK in the reader)
// ============================================================
static const int gotoSteps[4] = { 1, 10, 100, 1000 };
#define GOTO_CHAPTER 4             // fifth "step": jump by chapter (only when the book has a table of contents)
int gotoVal = 1;
int gotoStepIdx = 0;

// Does a step make sense for the current book
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

  if (chCount >= 2 && totalPages > 0) {            // name of the chapter we will land in
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

// Jump to the start of the next (dir > 0) or current/previous (dir < 0) chapter.
// Chapters starting on the same page are skipped (they cannot be "entered").
void gotoChapterStep(int dir) {
  int ci = chapterOfPage(gotoVal - 1);
  if (dir > 0) {
    for (int k = 1; k <= chCount; k++) {                 // from the last chapter back to the first
      int sp = chapterStartPage((ci + k) % chCount);
      if (sp != gotoVal) { gotoVal = sp; return; }
    }
  } else {
    int sp = chapterStartPage(ci);
    if (gotoVal > sp) { gotoVal = sp; return; }          // inside a chapter: to its start
    for (int k = 1; k <= chCount; k++) {                 // at the start: previous chapter
      sp = chapterStartPage((ci + chCount - k) % chCount);
      if (sp != gotoVal) { gotoVal = sp; return; }
    }
  }
}

void onKeyGoto(const KeyEvent& e) {
  int step = gotoSteps[gotoStepIdx % 4];
  if (e.key == KEY_DOWN && isStep(e)) {
    if (gotoStepIdx == GOTO_CHAPTER) gotoChapterStep(1);
    else if (gotoVal >= totalPages) gotoVal = 1;          // from the last page to the first
    else {
      gotoVal += step;
      if (gotoVal > totalPages) gotoVal = totalPages;
    }
    drawGoto();
  } else if (e.key == KEY_UP && isStep(e)) {
    if (gotoStepIdx == GOTO_CHAPTER) gotoChapterStep(-1);
    else if (gotoVal <= 1) gotoVal = totalPages;          // from the first page to the last
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
  } else if (e.key == KEY_HOME && isShort(e)) {          // confirm the jump
    int old = page;
    page = gotoVal - 1;
    statsOnTurn(old, page);
    bmDirty = true;
    lastTurnMs = millis();
    screen = S_READER;
    showPage(false);
  } else if (e.key == KEY_BACK && isShort(e)) {          // cancel
    screen = S_READER;
    showPage(false);
  }
}

// ============================================================
//  Settings (stored in non-volatile memory)
// ============================================================
void goMenu();   // declaration (used in the settings)

int setSel = 0;                  // position in the group list or inside a group
int setGroup = -1;               // -1 = group list, 0.. = inside a group
int setTopSel = 0;               // group remembered when coming back

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
  devMode = prefs.getBool("dev_on", false);
  // 0.9.11: Hotspot power and Power saving moved to the hidden Developer menu. Once,
  // we set the tested values so that nobody is left with a worse setting they cannot see.
  if (prefs.getUChar("cfg_ver", 0) < 2) {
    cfg.apPower = 1;                 // Medium
    cfg.powerSave = 2;               // Light
    prefs.putUChar("cfg_ap", cfg.apPower);
    prefs.putUChar("cfg_ps", cfg.powerSave);
    prefs.putUChar("cfg_ver", 2);
  }
  applyLayout();                 // also fixes invalid font indexes
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

// Settings are split into groups (two-level menu)
#define SET_GROUPS 5               // groups shown in the Settings list
#define SET_DEV 5                  // hidden Developer group (opened from the main menu in developer mode)
static const char* const SET_GROUP_NAMES[SET_GROUPS + 1] = { "Reading", "Status line", "Clock", "Power & system", "Battery", "Developer" };
static const uint8_t SET_GROUP_ITEMS[SET_GROUPS + 1] = { 6, 7, 3, 7, 5, 4 };

// Item texts have at most UI_CHARS - 2 characters (they are preceded by "> ")
const char* groupItem(int g, int i) {
  static char b[UI_CHARS + 1];
  static const char* const batNames[3] = { "OFF", "Low", "Always" };
  static const char* const sleepNames[3] = { "Book info", "Image", "Current page" };
  static const char* const marginNames[3] = { "Normal", "Narrow", "Wide" };
  static const char* const gapNames[3] = { "Normal", "Relaxed", "Loose" };
  static const char* const clockNames[3] = { "OFF", "24h", "12h" };
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
      case 0: snprintf(b, sizeof(b), "Status line: %s", onOff(statusAnyOn())); break;   // OFF = everything off = more lines of text
      case 1: snprintf(b, sizeof(b), "Percent: %s", onOff(cfg.showPct)); break;
      case 2: snprintf(b, sizeof(b), "Page n/N: %s", onOff(cfg.showPage)); break;
      case 3: snprintf(b, sizeof(b), "Time left: %s", onOff(cfg.showLeft)); break;
      case 4: snprintf(b, sizeof(b), "Progress bar: %s", onOff(cfg.showBar)); break;
      case 5: snprintf(b, sizeof(b), "Clock in book: %s", onOff(cfg.clockBook)); break;
      default:
        if (!batteryAvailable()) snprintf(b, sizeof(b), "Battery: OFF");   // no reading: turn on Settings -> Battery -> Estimate
        else snprintf(b, sizeof(b), "Battery: %s", batNames[cfg.batMode % 3]);
        break;
    }
  } else if (g == 2) {
    switch (i) {
      case 0: snprintf(b, sizeof(b), "Clock: %s", clockNames[cfg.clockMode % 3]); break;
      case 1: snprintf(b, sizeof(b), "Zone: %s", TZS[cfg.tz % TZ_COUNT].name); break;
      default: {                                   // OK opens manual time setting
        char t[12];
        if (clockFormat(t, sizeof(t), cfg.clockMode == 2 ? 2 : 1)) snprintf(b, sizeof(b), "Set time: %s", t);
        else snprintf(b, sizeof(b), "Set time: not set");
      }
    }
  } else if (g == 3) {
    switch (i) {
      case 0:
        if (sleepMinutes[cfg.sleepIdx] == 0) snprintf(b, sizeof(b), "Sleep time: Never");
        else snprintf(b, sizeof(b), "Sleep time: %d min", sleepMinutes[cfg.sleepIdx]);
        break;
      case 1: snprintf(b, sizeof(b), "Cover: %s", sleepNames[cfg.sleepScreen % 3]); break;
      case 2: snprintf(b, sizeof(b), "Page flip: %s", cfg.fastFlip ? "Fast" : "Full"); break;
      case 3: snprintf(b, sizeof(b), "Menu flip: %s", cfg.fastMenu ? "Fast" : "Full"); break;
      case 4: {
        static const char* const uiNames[4] = { "Classic", "Helv 14", "Helv 12", "Unifont" };
        snprintf(b, sizeof(b), "Menu font: %s", uiNames[cfg.uiFont % 4]);
        break;
      }
      case 5: snprintf(b, sizeof(b), "Full every: %d pages", fullEveryVals[cfg.fullEvery % 3]); break;
      default: snprintf(b, sizeof(b), "Open last book: %s", onOff(cfg.autoOpen)); break;
    }
  } else if (g == SET_DEV) {
    switch (i) {
      case 0: {
        static const char* const apNames[3] = { "Low", "Medium", "High" };
        snprintf(b, sizeof(b), "Hotspot power: %s", apNames[cfg.apPower % 3]);
        break;
      }
      case 1: {
        static const char* const psNames[3] = { "OFF", "CPU 80", "Light" };
        snprintf(b, sizeof(b), "Power saving: %s", psNames[cfg.powerSave % 3]);
        break;
      }
      case 2: snprintf(b, sizeof(b), "Hotspot: %s", apPassword().c_str()); break;   // OK draws a new password
      default: snprintf(b, sizeof(b), "Battery pin test"); break;
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

void goSetTime();                      // (below, after the settings)
void goSetBatt();
void goPinScan();
bool settingsFromReader = false;       // entered through the shortcut from a book (long BACK): BACK returns to the book
char settingsResumeBook[NAME_LEN];

void drawSettings() {
  if (setGroup < 0) drawList("Settings", settingsItem, SET_GROUPS, setSel, "", "OPEN", "BACK");
  else drawList(SET_GROUP_NAMES[setGroup], settingsItem, SET_GROUP_ITEMS[setGroup], setSel,
                (settingsFromReader && setGroup <= 1) ? (setGroup == 0 ? "STATUS" : "TEXT") : "", "CHANGE",
                settingsFromReader ? "BOOK" : "BACK");
}

void goDeveloper() {
  screen = S_SETTINGS;
  settingsFromReader = false;
  setGroup = SET_DEV;
  setSel = 0;
  drawSettings();
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
      case 0: statusSetAll(!statusAnyOn()); break;          // ON/OFF changes all items at once
      case 1: cfg.showPct = !cfg.showPct; break;
      case 2: cfg.showPage = !cfg.showPage; break;
      case 3: cfg.showLeft = !cfg.showLeft; break;
      case 4: cfg.showBar = !cfg.showBar; break;
      case 5: cfg.clockBook = !cfg.clockBook; break;
      default: if (batteryAvailable()) cfg.batMode = (cfg.batMode + 1) % 3; break;   // without a reading there is nothing to show
    }
  } else if (g == 2) {
    switch (i) {
      case 0: cfg.clockMode = (cfg.clockMode + 1) % 3; break;
      case 1:
        cfg.tz = (cfg.tz + 1) % TZ_COUNT;
        clockApplyTz(cfg.tz);
        break;
      default: break;                        // information: the time is set from the web page
    }
  } else if (g == 3) {
    switch (i) {
      case 0: cfg.sleepIdx = (cfg.sleepIdx + 1) % 5; break;
      case 1: cfg.sleepScreen = (cfg.sleepScreen + 1) % 3; break;
      case 2: cfg.fastFlip = !cfg.fastFlip; break;
      case 3: cfg.fastMenu = !cfg.fastMenu; break;
      case 4: cfg.uiFont = (cfg.uiFont == 2) ? 0 : 2; break;      // Helvetica 12 <-> Classic
      case 5: cfg.fullEvery = (cfg.fullEvery + 1) % 3; break;
      default: cfg.autoOpen = !cfg.autoOpen; break;
    }
  } else if (g == SET_DEV) {
    switch (i) {
      case 0: cfg.apPower = (cfg.apPower + 1) % 3; break;
      case 1:
        cfg.powerSave = (cfg.powerSave + 1) % 3;
        powerApply();
        break;
      case 2: apPasswordNew(); break;        // new hotspot password (phones must join the network again)
      default: break;                        // 3: pin test (opened in onKeySettings)
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
      default: break;                        // 2: voltage editor (opened in onKeySettings)
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
    if (setGroup < 0) {                      // enter a group
      setTopSel = setSel;
      setGroup = setSel;
      setSel = 0;
    } else if (setGroup == 2 && setSel == 2) {
      goSetTime();                           // manual date and time setting
      return;
    } else if (setGroup == 4 && setSel == 2) {
      goSetBatt();                           // manual battery voltage entry
      return;
    } else if (setGroup == SET_DEV && setSel == 3) {
      goPinScan();                           // battery pin search (after a board modification)
      return;
    } else {
      groupChange(setGroup, setSel);
      applyLayout();
      listMoveOnly = true;                   // value changed: fast refresh
    }
    drawSettings();
  } else if (e.key == KEY_HOME && isShort(e) && settingsFromReader && setGroup >= 0 && setGroup <= 1) {
    setGroup = 1 - setGroup;                 // shortcut from a book: HOME switches text <-> status line
    setSel = 0;
    drawSettings();
  } else if (e.key == KEY_BACK && isShort(e)) {
    saveSettings();
    if (settingsFromReader) {                // shortcut from a book: back to it (with the new layout)
      settingsFromReader = false;
      setGroup = -1;
      goReader(settingsResumeBook);
      return;
    }
    if (setGroup == SET_DEV) {               // Developer is opened from the main menu: return there
      setGroup = -1;
      goMenu();
    } else if (setGroup >= 0) {              // from a group back to the group list
      setGroup = -1;
      setSel = setTopSel;
      drawSettings();
    } else {
      goMenu();
    }
  }
}

// ---------- Manual clock setting ----------
// Fields: 0 year, 1 month, 2 day, 3 hour, 4 minute (local time of the chosen time zone).
// DOWN increases, UP decreases (as in "Go to page"); OK = next field; HOME = save; BACK = cancel.
int stY = 2026, stMo = 1, stD = 1, stH = 12, stMi = 0, stField = 0;

static int wrapRange(int v, int lo, int hi) {
  int n = hi - lo + 1;
  return lo + (((v - lo) % n) + n) % n;
}

// Initial values: the current time; when the clock is not set - the last known (saved) time or the build date
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
    const char* d = __DATE__;                // e.g. "Oct 14 2026"
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
  static const int fx[5] = { 0, 60, 96, 0, 36 };       // start of each field in px (12 px per character)
  static const int fw[5] = { 48, 24, 24, 24, 24 };
  int under = (stField < 3 ? 26 : 62) + 26;
  gfxHLine(UI_X + fx[stField], UI_X + fx[stField] + fw[stField] - 1, under);        // underline of the active field
  gfxHLine(UI_X + fx[stField], UI_X + fx[stField] + fw[stField] - 1, under + 1);
  char z[28];
  snprintf(z, sizeof(z), "Zone: %s", TZS[cfg.tz % TZ_COUNT].name);
  gfxText(UI_X, 98, z, FOOT_SIZE);
  drawLegend("SAVE", "NEXT", "CANCEL", true);
  gfxPresent();
}

void goSetTime() {
  stLoadDefaults();
  stField = 3;                               // the hour is the field corrected most often
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
    int step = (stField == 4) ? accelStep(e.repeat) : 1;   // minutes: faster while held
    if (step > 10) step = 10;
    switch (stField) {
      case 0: stY = wrapRange(stY + dir * step, 2024, 2099); break;
      case 1: stMo = wrapRange(stMo + dir * step, 1, 12); break;
      case 2: stD = wrapRange(stD + dir * step, 1, clockDaysInMonth(stY, stMo)); break;
      case 3: stH = wrapRange(stH + dir * step, 0, 23); break;
      default: stMi = wrapRange(stMi + dir * step, 0, 59); break;
    }
    int dm = clockDaysInMonth(stY, stMo);                 // changing the year/month must not leave a day that does not exist
    if (stD > dm) stD = dm;
    drawSetTime();
  } else if (e.key == KEY_OK && isShort(e)) {
    stField = (stField + 1) % 5;
    drawSetTime();
  } else if (e.key == KEY_HOME && isShort(e)) {          // save
    clockApplyTz(cfg.tz);
    if (clockSetLocal(stY, stMo, stD, stH, stMi)) prefs.putUInt("clk_last", (uint32_t)clockNow());
    stLeave();
  } else if (e.key == KEY_BACK && isShort(e)) {          // cancel
    stLeave();
  }
}

// ---------- Manual battery voltage entry ----------
// Voltage measured with a multimeter AT REST (battery without load, a few tens of minutes after
// charging/use) sets the starting point of the estimate from the Li-Po curve. DOWN increases, UP decreases,
// OK = step 0.01 / 0.05 V, HOME = save, BACK = cancel.
int sbCv = 390;                  // centivolts
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
    sbCv += dir * sbStep * (e.repeat > 8 ? 2 : 1);          // held: faster
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
  bool canRead = getLastBook(last, sizeof(last));      // BACK = back to the book
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
  if (devMode) {
    menuIds[menuCount] = M_DEV;
    menuLabels[menuCount++] = "Developer";
  }

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
  drawLegend("", "", "BACK", false);
  gfxPresent();
  screen = S_INFO;
}

// Pin test: shows the voltage on the free ADC pins. With the battery connected, the battery
// divider pin shows about half of its voltage, i.e. 1700-2150 mV. Enter it as PIN_BAT_ADC.
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

// ---------- Statistics ----------
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
    // about 7 bytes per word (UTF-8 with Polish characters), so this is only an approximation
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
//  Hotspot and web page: books, settings, sleep image
//  The reader creates its own WiFi network (fixed password, the phone remembers it) and serves
//  the page at 192.168.4.1. Only someone who knows the network password has access,
//  so no extra PIN is needed. The page is in English.
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

// Counts an HTTP request (diagnostics: shows whether the phone reaches the reader at all)
void countHit(const char* what) {
  httpHits++;
  statusDirty = true;
  Serial.printf("HTTP %s\n", what);
}

// Hotspot events: someone connected / disconnected / got an IP address
void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
    Serial.println("AP: phone connected");
    statusDirty = true;
  } else if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
    Serial.println("AP: phone disconnected");
    statusDirty = true;
  } else if (event == ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED) {
    Serial.println("AP: phone got an IP address");
    statusDirty = true;
  }
}

// ---------- Helpers: page sent in chunks (no large buffers in RAM) ----------
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

// Display name: without .txt, '_' -> space
void displayName(const char* fileName, char* out, int maxChars) {
  shortName(fileName, out, maxChars);
  for (char* p = out; *p; p++)
    if (*p == '_') *p = ' ';
}

// ---------- Main page: books ----------
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
    if (tp[0]) LittleFS.remove(tp);                // together with its table of contents
    removeAllIdx(name);
  }
  server.send(200, "text/plain", "OK");
}

// Browsers and systems check connectivity by requesting well-known addresses. Since DNS returns
// this device for everything, we answer the way the internet would - then the phone treats the network
// as working and does NOT move traffic to mobile data (the page would not open then).
// All other addresses are redirected to the main page.
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

// ---------- Settings ----------
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
  h += "</select><p class='note'>Uses the battery estimate (Battery card below).</p></div>";
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

  h = "<div class='card'><h2>Display</h2><p class='sub'>Fast page flip skips the black flash; a full refresh every few pages removes ghost images.</p>"
      "<label class='f'>Page flip</label><select name='flip'>";
  optTag(h, 0, "Full refresh (flashes, cleanest)", !cfg.fastFlip);
  optTag(h, 1, "Fast (no flash)", cfg.fastFlip);
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
  optTag(h, 0, "Classic (pixel, old)", cfg.uiFont == 0);
  h += "</select>";
  if (devMode) {                           // hidden options: only in developer mode
    h += "<label class='f'>Hotspot power (developer)</label><select name='appower'>";
    optTag(h, 0, "Low (best when the phone or PC is next to the reader)", cfg.apPower == 0);
    optTag(h, 1, "Medium (default)", cfg.apPower == 1);
    optTag(h, 2, "High (longer range, needs a strong power source)", cfg.apPower == 2);
    h += "</select><label class='f'>Power saving (developer)</label><select name='psave'>";
    optTag(h, 0, "Off (processor 240 MHz)", cfg.powerSave == 0);
    optTag(h, 1, "Processor 80 MHz", cfg.powerSave == 1);
    optTag(h, 2, "80 MHz + light sleep when idle (default)", cfg.powerSave == 2);
    h += "</select>";
  }
  h += "</div>";
  server.sendContent(h);

  h = "<div class='card'><h2>Power</h2>"
      "<label class='f'>Go to sleep after</label><select name='sleep'>";
  for (int i = 0; i < 5; i++) {
    char lab[24];
    if (sleepMinutes[i] == 0) snprintf(lab, sizeof(lab), "Never");
    else snprintf(lab, sizeof(lab), "%d min", sleepMinutes[i]);
    optTag(h, i, lab, cfg.sleepIdx == i);
  }
  h += "</select><label class='f'>Cover (sleep screen)</label><select name='sscr'>";
  optTag(h, 0, "Book info (title, progress)", cfg.sleepScreen == 0);
  optTag(h, 1, "Sleep image", cfg.sleepScreen == 1);
  optTag(h, 2, "Current page (leave the screen as it is)", cfg.sleepScreen == 2);
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
    if (uf == 1 || uf == 3) uf = 2;         // former Helvetica 14 and Unifont
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

// Battery estimate log (CSV) to download from the web page
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

// Clock setting: the browser sends the current time (seconds since 1970) every time the page opens
void handleTime() {
  long t = server.arg("t").toInt();
  if (t < 1700000000L) {
    server.send(400, "text/plain", "bad time");
    return;
  }
  clockSet((time_t)t);
  prefs.putUInt("clk_last", (uint32_t)t);                  // default value for manual setting after a power loss
  char b[32], tm[12], dt[12];
  if (clockFormat(tm, sizeof(tm), cfg.clockMode == 2 ? 2 : 1) && clockFormatDate(dt, sizeof(dt))) snprintf(b, sizeof(b), "%s, %s", tm, dt);
  else snprintf(b, sizeof(b), "not set");
  server.send(200, "text/plain", b);
}

// ---------- Sleep image ----------
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

// ---------- Book upload ----------
// The upload goes to a temporary file; only after it completes successfully
// does it replace the book. An interrupted upload leaves the old version alone.
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
        uploadIsToc = true;                // table of contents from an EPUB import
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
      if (!uploadIsToc) removeAllIdx(uploadName);   // the old index no longer fits
      LittleFS.remove(uploadPath);         // old version (if there was one)
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

// ---------- Upload mode screen: QR codes ----------
// Step 0: QR code with the network details (the phone camera joins the WiFi).
// Step 1: QR code with the page address (the camera opens the browser). OK switches the steps.
// The code has a wide empty margin around it (min. 4 modules), because the dark frame of the
// display right next to the code spoils reading it.
#define QR_SCALE 3                   // px per QR code module
#define QR_X 16                      // left edge of the code (px)
#define QR_Y(h) ((SCREEN_H - (h)) / 2) // top edge of the code: middle of the screen height

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

  // Code in the middle of the screen height, title above the text column: at least ~5 modules
  // of empty margin stay above and below the code (the title used to sit right above it).
  int tx = 104;                              // left edge of the text column
  if (qrEncode(payload, qr)) {
    int h = qr.size * QR_SCALE;
    uiQr(qr, QR_X, QR_Y(h), QR_SCALE);
    tx = QR_X + h + 12;                      // 12 px (4 modules) gap after the code
  }
  drawTitle(tx, uploadStep == 0 ? "Join WiFi" : "Open page");   // no WiFi icon: with it the title ran into the key legend
  const int maxCh = (SCREEN_W - TAG_COL_W - tx) / 6;   // how many 12 px characters fit before the key legend

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

  if (lastUploaded[0]) {                     // after a successful upload: the book name
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
  delay(100);                      // only after this can WiFi be turned on again safely
}

// Starts the hotspot. A clean start of the WiFi stack and up to 3 attempts lower the risk that the network "sometimes does not appear"
// (e.g. when entering Upload again quickly or during a brief voltage drop).
static bool apStart(const char* ssid, const char* pass) {
  WiFi.persistent(false);                     // do not store WiFi settings in flash
  for (int attempt = 1; attempt <= 3; attempt++) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(150);
    WiFi.mode(WIFI_AP);
    delay(50);
    WiFi.setSleep(false);
    if (WiFi.softAP(ssid, pass, AP_CHANNEL, 0, 4)) {                       // visible, up to 4 clients
      String ip = WiFi.softAPIP().toString();
      if (ip != "0.0.0.0") {
        // Lower transmit power: the device usually sits right next to the phone/computer, and full power at close range
        // overloads the receiver (the network may then show up as "Hidden network") and loads the power supply.
        static const wifi_power_t apPowerVals[3] = { WIFI_POWER_8_5dBm, WIFI_POWER_13dBm, WIFI_POWER_19_5dBm };
        WiFi.setTxPower(apPowerVals[cfg.apPower % 3]);
        Serial.printf("hotspot OK (attempt %d): channel %d, power %d, free heap %u B\n", attempt, AP_CHANNEL, cfg.apPower, (unsigned)ESP.getFreeHeap());
        wifi_config_t wc;
        if (esp_wifi_get_config(WIFI_IF_AP, &wc) == ESP_OK) {          // diagnostics: is the network really visible and encrypted
          Serial.printf("AP: ssid_len=%d hidden=%d authmode=%d channel=%d\n", (int)wc.ap.ssid_len, (int)wc.ap.ssid_hidden, (int)wc.ap.authmode, (int)wc.ap.channel);
        }
        return true;
      }
    }
    Serial.printf("hotspot: attempt %d failed\n", attempt);
  }
  WiFi.mode(WIFI_OFF);
  return false;
}

void goUpload() {
  batteryMv();                     // measure the battery BEFORE turning on WiFi (ADC2 does not work with WiFi)

  // The network starts AT ONCE. An e-paper refresh takes about 2 s, so drawing the screen
  // before starting the hotspot would delay the network appearing on the phone.
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
  dns.start(53, "*", WiFi.softAPIP());                  // every name -> the reader
#endif
  Serial.printf("Hotspot: %s, address %s\n", ssid, WiFi.softAPIP().toString().c_str());

  lastUploaded[0] = '\0';
  uploadPath[0] = '\0';
  uploadErr = "";
  uploadDone = false;
  uploadStep = 0;
  httpHits = 0;
  statusDirty = false;
  LittleFS.remove(TMP_PATH);       // leftover of an interrupted upload
  LittleFS.remove(WP_TMP);
  server.begin();
  screen = S_UPLOAD;
  powerApply();                    // hotspot: processor at least 160 MHz
  drawUpload();
}

// ============================================================
//  Button handling for every screen
// ============================================================
void onKeyMenu(const KeyEvent& e) {
  if (e.key == KEY_UP && isStep(e)) {
    menuSel = (menuSel + menuCount - 1) % menuCount;      // from the first item to the last
    listMoveOnly = true;
    drawMenu();
  } else if (e.key == KEY_DOWN && isStep(e)) {
    menuSel = (menuSel + 1) % menuCount;                  // from the last item to the first
    listMoveOnly = true;
    drawMenu();
  } else if (e.key == KEY_BACK && isShort(e)) {            // BACK: straight back to the book
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
      case M_DEV: goDeveloper(); break;
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
  } else if (e.key == KEY_BACK && isLong(e)) {           // long BACK: font, line spacing etc. without going through the menu
    saveBookmark();
    strncpy(settingsResumeBook, curBook, NAME_LEN - 1);
    settingsResumeBook[NAME_LEN - 1] = '\0';
    closeBook();
    screen = S_SETTINGS;
    settingsFromReader = true;
    setGroup = 0;                            // Reading group
    setSel = 0;
    drawSettings();
  } else if (e.key == KEY_HOME && isShort(e)) {
    saveBookmark();
    closeBook();
    goMenu();
  }
}

// ============================================================
//  Deep sleep
// ============================================================
unsigned long lastActivityMs = 0;

// Did the reader fall asleep in a book (RTC memory survives deep sleep). After waking: in a book
// back to the book, elsewhere to the main menu. After power-on "Open last book" as before.
RTC_DATA_ATTR uint8_t sleptInBook = 1;

void goSleep() {
  sleptInBook = (screen == S_READER || screen == S_GOTO) ? 1 : 0;
  if (clockValid()) prefs.putUInt("clk_last", (uint32_t)clockNow());   // default value for manual setting after a power loss
  Serial.println("sleep");
  statsFlush();              // add the reading time (only while reading) and save the statistics
  saveBookmark();            // safe outside the reader too (it has safeguards)
  if (fsOk && cfg.sleepScreen != 2) drawSleepScreen();   // book card or image (one refresh)
  if (fsOk) LittleFS.end();
  bmUpdate();                // add the awake time before sleeping
  prefs.putUInt("bm_slp", clockValid() ? (uint32_t)clockNow() : 0);   // after waking, add the sleep time
  bmSave();
  boardDeepSleep();          // does not return; after waking setup() starts from scratch
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

  // Formatting erases the books, so only with consent (OK button).
  // On the first start on a fresh partition this is expected.
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

  boardWaitRelease();        // the wake-up button must not act as a command

  buildMenu();
  screen = S_MENU;
  char last[NAME_LEN];
  bool resumed = esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED;   // woken from deep sleep
  if (cfg.autoOpen && (!resumed || sleptInBook) && getLastBook(last, sizeof(last))) {
    goReader(last);          // on an error it shows the menu by itself
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
      statusDirty = false;       // the counter is refreshed rarely and never during a file upload
      drawUpload();
    }
  }

  // bookmark: saved after 4 s of inactivity (saves flash)
  if (screen == S_READER && bmDirty && millis() - lastTurnMs > 4000) {
    saveBookmark();
  }

  // Main menu: MENU held on after a "long" press. Released before 10 s = sleep
  // (as everywhere), held for 10 s = developer mode on/off.
  if (devHoldMs) {
    if (!boardKeyDown(KEY_HOME)) {
      devHoldMs = 0;
      goSleep();
      return;
    }
    if (millis() - devHoldMs >= DEV_HOLD_MS) {
      devHoldMs = 0;
      devMode = !devMode;
      prefs.putBool("dev_on", devMode);
      Serial.printf("developer mode: %s\n", devMode ? "on" : "off");
      if (screen == S_MENU) {
        buildMenu();
        drawMenu();
      }
    }
    lastActivityMs = millis();
    delay(5);
    return;
  }

  KeyEvent e = inputPoll();
  if (e.type == EV_NONE) {
    unsigned long lim = (unsigned long)sleepMinutes[cfg.sleepIdx] * 60000UL;
    if (lim && screen != S_UPLOAD && millis() - lastActivityMs > lim) goSleep();
    idleWait();                 // delay(5) or light sleep (Power saving)
    return;
  }
  lastActivityMs = millis();

  // shortcut: long HOME = sleep now (except in upload mode); in the main menu we wait for the release
  if (e.key == KEY_HOME && isLong(e) && screen == S_MENU) {
    devHoldMs = millis() - LONG_PRESS_MS;
    return;
  }
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
      } else if (e.key == KEY_OK && isShort(e)) {      // switch the QR code: network / page
        uploadStep = 1 - uploadStep;
        drawUpload();
      }
      break;
    case S_INFO:
      if (e.key == KEY_BACK && isShort(e)) goMenu();
      break;
    case S_STATS:
      if (e.key == KEY_BACK && isShort(e)) goMenu();
      break;
    case S_PINSCAN:
      if (e.key == KEY_BACK && isShort(e)) goDeveloper();
      break;
    case S_GOTO: onKeyGoto(e); break;
    case S_SETTINGS: onKeySettings(e); break;
    case S_SETTIME: onKeySetTime(e); break;
    case S_SETBATT: onKeySetBatt(e); break;
  }
  lastActivityMs = millis();
}
