# Development notes

## Source map (`firmware/inkblink/`)

| File | Role |
|---|---|
| `inkblink.ino` | main program: screens, settings, library, reader, statistics, sleep, hotspot and web server |
| `board_crowpanel.h` | the board layer: pins, key reading, battery ADC, deep sleep, drawing primitives (`gfx*`), full/fast presenting |
| `input.h` | key events: press, repeat with acceleration, short / long press |
| `reader_fonts.h` | font families, size tables, glyph measurement, line height, drawing of a text line |
| `hyphen.h` | Polish hyphenation rules |
| `clock.h` | time zones (POSIX TZ), time formatting, setting the system time |
| `qr.h` | self-contained QR code generator (byte mode, ECC M, versions 1-5) |
| `ui_gfx.h` | UI shapes: icons, QR drawing, battery |
| `webui_assets.h` | HTML/CSS/JS of the web interface (PROGMEM) |
| `partitions.csv` | 2 MB app + about 5.9 MB LittleFS |

The e-paper driver files (Elecrow's code, one part modified) sit in the same folder; see `driver/README.md` and `THIRD_PARTY_NOTICES.md`.

## Concepts worth knowing

- **Pagination and the page index.** For every book and layout the firmware builds an index of page start offsets and caches it in `/i<hash>_<layout>.idx`. The layout signature (`layoutSig()`) hashes font, size, bold, status line visibility, text width, lines per page, line height and the hyphenation setting. If you change the wrapping or hyphenation algorithm, bump `WRAP_VERSION` / `HYPHEN_VERSION` so old indexes are discarded. Bookmarks are stored as byte offsets, so they survive layout changes.
- **Line wrapping** (`layoutLine()`): counts pixels, not characters. A word that does not fit is hyphenated if allowed (`hyphen.h`, at most two hyphenated lines in a row per page), otherwise moved to the next line. The hyphen is drawn at display time; the text file is never modified.
- **Glyph measurement** (`reader_fonts.h`): U8g2's `getUTF8Width()` returns the visible width for the last glyph, so advances are measured as `width("cc") - width("c")` and cross-checked against a probe string. Line height comes from drawing a probe string with accents and descenders into the frame buffer and measuring the real ink extent. Synthetic bold (Unifont) draws each glyph twice with the same advance the wrapper uses.
- **Chapter list**: EPUB import writes `<name>.toc` next to `<name>.txt`: one line per chapter, `byte-offset<TAB>title`, offsets ascending, UTF-8, at most 300 chapters, file at most 24 KB. The firmware cuts titles to 31 bytes.
- **Fast refresh**: list screens and (optionally) page flips send the previous and the new frame to the controller and run a partial waveform; a full refresh is forced periodically to clear ghosts.
- **Web interface**: served from PROGMEM, no external assets. The Books page script is `tools/epub/epub.js` + `tools/epub/books_page.js`, embedded into `webui_assets.h` by `tools/sync_webui_epub.py`.
- **Arduino IDE prototype pitfall**: the IDE inserts function prototypes at the top of the sketch. Avoid default arguments, and define structs/enums before functions that use them. `tests/test_arduino_prototypes.cpp` emulates this step.

## Adding another board

Create a `board_<name>.h` that provides the same interface as `board_crowpanel.h` (`boardInit`, `boardReadKey`, `boardKeyDown`, `boardWaitRelease`, `boardBatteryMv`, `boardPinMv`, `boardDeepSleep`, `gfxClear`, `gfxInk`, `gfxWhite`, `gfxIsInk`, `gfxText*`, `gfxHLine`, `gfxPresent`, `gfxPresentFast`, `gfxFastReady`, the pin/key constants and screen size) and select it in `inkblink.ino`. The LilyGo T5 2.13" (GxEPD2) is the planned second target. The stand-in driver in `tests/stubs/` shows the minimum API.

## Tests

```sh
make -C tests test          # C++ host tests + JavaScript tests
make -C tests test-driver   # also the tests that need the real driver files (part of the repository)
```

- The firmware sources are compiled unchanged with g++ against stand-ins in `tests/stubs/` (Arduino, WiFi, WebServer, LittleFS, Preferences, DNS, U8g2, e-paper driver). `tests/common.cpp` holds the shared mock state; each `test_*.cpp` includes `inkblink.ino` and drives the firmware functions directly.
- Frame-buffer assertions use `gfxIsInk()`; the stand-in "Classic" font draws simple shapes, so ink counts are meaningful but glyph shapes are not. The stand-in U8g2 mimics real metrics (visible-width quirk, accents above the cap height, descender tails), which is what the wrapping and line-spacing tests rely on.
- What the tests do **not** cover: real display output, SPI timing, WiFi/captive portal behaviour on phones, deep sleep and wake-up, battery ADC, clock drift, flash wear, and the real U8g2 glyph bitmaps. Check those on the device.
- EPUB tests build small synthetic EPUB files (`tools/epub/make_samples.py`), not commercial books.
- Output images of some tests are written to `tests/build/run/png/` as raw 250 x 122 bitmaps for inspection.

## Battery model

`firmware/inkblink/battery_model.h` holds the whole model: the measured current constants (`BM_*`, two of them still marked TODO), the voltage-to-percent curve and the accounting functions. `inkblink.ino` calls `bmUpdate()` every loop iteration (time awake or in hotspot mode plus the refresh counters `g_presFull`/`g_presFast` from `board_crowpanel.h`), accounts sleep time at boot from the system clock and writes `/battlog.csv`. Update the constants after new measurements; the log keeps the raw counters so old entries can be recomputed. Tests: `tests/test_battery_estimate.cpp`.

## Versioning and releases

- Versions are `MAJOR.MINOR.PATCH` (minor = new features, `1.0.0` = considered stable).
- During 0.9.x **every update package bumps the patch number**: update 8 = `0.9.8`, update 9 = `0.9.9`, then `0.9.10` and so on. The on-device Info screen shows `FW_VERSION`, so you always see which package is flashed and a bug report always names its build.
- With each package: set `FW_VERSION` in `inkblink.ino` and add a heading for the new version at the top of `CHANGELOG.md`; commit message e.g. "0.9.8: light sleep fix".
- When a version has been tested on the device and is worth keeping, tag it: add the date to its changelog heading, commit, then `git tag v0.9.8` and `git push --tags`. Only tagged versions are releases (and, later, get `.bin` files).
- `tests/check_version.py` (part of `make test`) verifies that `FW_VERSION` is well formed and equals the newest `CHANGELOG.md` heading.
- Release binaries contain the manufacturer's driver code. Elecrow's permission as received covers including the files in this repository; it does not explicitly mention compiled binaries or commercial use (see `THIRD_PARTY_NOTICES.md`), so ask Elecrow before selling pre-flashed devices.

## Code language

Firmware code comments, on-device texts and the web UI are in English. The PC test programs in `tests/` still print some of their messages in Polish; translating them is a welcome contribution. New code and comments should be in English.
