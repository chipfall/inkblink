# Changelog

Versions are `MAJOR.MINOR.PATCH`. During 0.9.x every update package bumps the patch number (`0.9.8` = update 8), so the Info screen always shows which package is installed. A version that has been tested on the device gets a git tag (see `docs/DEVELOPMENT.md`). The newest section must match `FW_VERSION` in `firmware/inkblink/inkblink.ino` (checked by `make -C tests test`).

## 0.9.10

- Fix: the QR codes on the Upload screen could not be scanned on the device. The Arduino `bit()` macro replaced a helper of the same name in the QR encoder, so all format bits were drawn dark (the code was fine in the PC tests, which now define the same macro and check the format bits)
- Upload screen: QR code centred vertically with an empty margin of at least 4 modules on every side; the title moved above the text column
- Battery estimate: deep-sleep current set to the measured 0.33 mA (4-day voltage-drop test)

## 0.9.9

- *Power saving* = Light: while reading, the device goes to light sleep right after a page flip (the 1.5 s delay now applies only to menus and settings), which roughly halves the energy per page

## 0.9.8

- Fix: with *Power saving* = Light the device did not stay asleep (sleep screen appeared, then it restarted into the book). The light-sleep timer was still armed when entering deep sleep; all wake sources are now cleared, only the wheel button wakes the device
- Light sleep starts 1.5 s after the last button press, so menu navigation has no wake-up lag
- Battery estimate uses the measured currents: 24 mA idle at 80 MHz, below 1 mA in light sleep (to be refined)
- Version numbering: the patch number follows the update package (this is update 8)

## 0.9.7 (updates 1-7, earlier shown as 0.9.1-dev)

- Menu and screen titles in a clearer font (setting *Menu font*: Helvetica 12 / Unifont / Classic; default Helvetica 12): a distinct "l", fixes the broken "M" and "W" in bold Classic titles
- Battery estimate without a sensor (Settings -> Battery): consumption model from measured currents, manual voltage entry (device and web page), "battery full", CSV log; battery indicator shows `~NN%`
- Experimental *Power saving* setting: processor 80 MHz and light sleep in idle (to be measured)
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
