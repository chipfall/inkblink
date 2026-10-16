# Changelog

Versions are `MAJOR.MINOR.PATCH`. A `-dev` suffix marks work in progress; it is dropped when a release is tagged (see `docs/DEVELOPMENT.md`). The newest section must match `FW_VERSION` in `firmware/inkblink/inkblink.ino` (checked by `make -C tests test`).

## 0.9.1-dev (unreleased)

- Menu and screen titles in a clearer font (setting *Menu font*: Helvetica 12 / Unifont / Classic; default Helvetica 12): a distinct "l", fixes the broken "M" and "W" in bold Classic titles
- Elecrow's driver files are now part of the repository (with Elecrow's permission); CI also runs the driver-dependent tests
- Hotspot start hardened: clean WiFi restart before every start, up to 3 attempts, selectable transmit power (setting *Hotspot power*, default Medium) and AP configuration details on the serial monitor
- Manual date and time setting (Settings -> Clock -> Set time); date shown on the Info screen and in the web page; clearer format examples in the web page

## 0.9.0 (2026-10-14, initial import)

First tracked version of InkBlink (before: private prototype versions 0.1 - 0.8).

- Reader: UTF-8 text books, EPUB import in the browser with chapter list, bookmarks, goto page / chapter
- Fonts: Helvetica and Century (10/12/14/18 px), Unifont 16 px, built-in Classic; real glyph measurement for correct wrapping, line spacing, margins
- Automatic Polish hyphenation (rule-based)
- Status line with percent, page, time left, progress bar, clock and battery; one master switch
- Statistics (time, pages, speed, estimated time left)
- Hotspot upload with captive portal, QR codes for WiFi and web page, web interface (books, sleep image, settings)
- Sleep screen (book card / custom image / keep), auto sleep, wake with the encoder button
- Clock set from the phone via the web page, time zones with daylight saving
- Battery indicator (pin setup required), pin finder on the Info screen
- Fast partial refresh for page flips (beta) and flash-free menu navigation
- Shortcuts: long BACK in the book opens the Reading settings, BACK in the menu returns to the book
- Host test suite (C++ and JavaScript), EPUB tools
