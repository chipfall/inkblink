# InkBlink

Open-source e-book reader firmware for the **Elecrow CrowPanel ESP32 2.13" e-paper display (DIE01021S, hardware V1.2)**.

It turns the board into a small pocket reader: plain-text books and EPUB files (converted in your browser) are uploaded over the reader's own WiFi hotspot, no SD card or internet needed. Full Polish character support, a built-in web interface, reading statistics and a lot of reading-comfort options.

> **Status: beta.** Developed and tested on one CrowPanel V1.2 board. Much of the logic is covered by host tests (see [Testing](#testing)), but on-device behaviour (screen refresh, fonts as rendered, battery, clock accuracy) is checked by hand. Expect rough edges. Issues and photos of your setup are welcome.

## Features

**Reading**
- `.txt` books (UTF-8, BOM/CRLF OK) and **EPUB** (converted to text in the browser, with a chapter list)
- Fonts: Helvetica and Century (10 / 12 / 14 / 18 px, real bold), Unifont 16 px (synthetic bold), built-in Classic 12 / 16 px; Polish letters everywhere
- Line spacing, margins, **automatic Polish hyphenation** (rule-based, can be switched off)
- Bookmarks per book (survive font changes), jump to a page or to a chapter
- Optional status line: percent, page n/N, time left, progress bar, clock, battery (one switch turns all of it off for more text per page)
- Reading statistics: time, pages, speed, finished books, estimated time left
- Page turns: full refresh, or fast partial refresh (beta) with a full refresh every N pages

**Device**
- Own WiFi hotspot + captive portal; QR codes on screen to join the network and open the page
- Web interface: upload many books at once (drag & drop), delete books, change every setting, upload a custom sleep image (converted to 1-bit in the browser)
- Sleep screen: book card / your own image / keep current screen; auto sleep after 1–30 min; wake with the encoder button
- Clock set from your phone each time you open the web page (time zones with daylight saving)
- Battery estimate (the board has no battery sensor): based on time awake / asleep and screen refreshes, started from a voltage you enter, with a downloadable log; experimental power saving (80 MHz, light sleep)
- Fast, flash-free refresh when moving through menus (can be switched to full refresh)

## Quick start

1. Install **Arduino IDE 2.x** with the **ESP32 Arduino core 3.x** and these libraries: `Adafruit GFX Library`, `U8g2_for_Adafruit_GFX`.
2. Open `firmware/inkblink/inkblink.ino` and select **ESP32S3 Dev Module** with:
   - Flash size **8 MB**, PSRAM **OPI PSRAM**, **USB CDC On Boot: Disabled**
   - the included `partitions.csv` (2 MB app + ~5.9 MB LittleFS for books; Arduino uses it automatically when it sits next to the sketch)
3. Connect the board over USB-C and upload. If the board boot-loops after the first upload on top of the factory firmware, enable **Erase All Flash Before Sketch Upload** once, then disable it again.
4. Add books: **Menu → Upload**, join the WiFi `InkBlink-XXXX` (scan the QR code or type the password shown), open `http://192.168.4.1`, drop `.txt` or `.epub` files.

Button reference and every screen: [docs/USER_GUIDE.md](docs/USER_GUIDE.md).

## Hardware

Only the CrowPanel 2.13" V1.2 is supported now. The e-paper driver for it (Elecrow's code, included with their permission) is in `firmware/inkblink/`. The code is split so another board (planned: LilyGo T5 2.13") needs a new `board_*.h` and driver glue. Pins, buttons, battery, and enclosure-relevant dimensions: [docs/HARDWARE.md](docs/HARDWARE.md).

## Testing

The firmware logic is compiled with g++ on a PC against stand-ins for the Arduino/ESP32 libraries and for the e-paper driver:

```sh
make -C tests test        # C++ host tests + JavaScript tests (needs g++, python3, node >= 18)
```

These tests check layout, hyphenation, settings, navigation, EPUB conversion, web pages and more, but **not** the real display, SPI timing, WiFi radio or battery. Details in [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md).

## Repository layout

| Path | Contents |
|---|---|
| `firmware/inkblink/` | Arduino sketch (`inkblink.ino` + headers), `partitions.csv` |
| `tests/` | host tests, stand-in libraries, Makefile |
| `tools/epub/` | EPUB → text converter (also embedded in the web page), samples generator, CLI |
| `docs/` | user guide, hardware notes, development notes, roadmap |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Note: most code comments are currently in Polish; translations and English-only contributions are welcome.

## License

Original code: MIT, see [LICENSE](LICENSE). Third-party components - including Elecrow's driver files, which are *not* MIT-licensed - keep their own terms: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
