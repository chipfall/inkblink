# Third-party notices

InkBlink's own code is MIT-licensed (see `LICENSE`). It builds on, and is used together with, the components below. This file records what was checked and what is still open. It is not legal advice.

## Libraries (not included in this repository; installed through the Arduino Library Manager)

| Component | License | Notes |
|---|---|---|
| **U8g2_for_Adafruit_GFX** (olikraus) | BSD 2-Clause | Checked in the project's LICENSE file. |
| **U8g2** library (olikraus), pulled in by the above | BSD 2-Clause for the library code | The fonts have *separate* licenses, see below. |
| **Adafruit GFX Library** (Adafruit Industries) | BSD (per Adafruit) | Not re-verified here: please check the license file of the version you install. |
| ESP32 Arduino core | LGPL 2.1 and others | Used as the build platform; not distributed here. |

## Fonts (compiled into the firmware through U8g2)

- **Helvetica** (`u8g2_font_helvR*_te`, `u8g2_font_helvB*_te`) and **New Century Schoolbook** (`u8g2_font_ncenR*_te`, `u8g2_font_ncenB*_te`) are U8g2's conversions of the Adobe X11 BDF fonts. U8g2's LICENSE file reproduces this notice for them:

  > Copyright 1984-1989, 1994 Adobe Systems Incorporated. Adobe is a trademark of Adobe Systems Incorporated which may be registered in certain jurisdictions. Permission to use, copy, modify, distribute and sell this software and its documentation for any purpose and without fee is hereby granted, provided that the above copyright notices appear in all copies and that both those copyright notices and this permission notice appear in supporting documentation, and that the names of Adobe Systems and Digital Equipment Corporation not be used in advertising or publicity pertaining to distribution of the software without specific, written prior permission. Adobe Systems and Digital Equipment Corporation make no representations about the suitability of this software for any purpose.

  The complete, authoritative text is in the LICENSE file of the U8g2 repository (https://github.com/olikraus/u8g2). When you distribute firmware binaries, keep this notice with them.

- **GNU Unifont** (`u8g2_font_unifont_t_polish`): dual-licensed under the **GNU GPL 2 or later with the GNU Font Embedding Exception** and the **SIL Open Font License 1.1** (since Unifont 13.0.04). The Unifont project states that the exception and the OFL allow use with non-GPL software. Source: https://unifoundry.com/unifont/

- **"Classic" font family** (Classic 12 / 16 px in the font menu) uses the bitmap font data of the manufacturer's driver (`EPDfont.h`) - see the next section (included with Elecrow's permission).

## Manufacturer's driver code (Elecrow) - included with permission

`firmware/inkblink/` contains the e-paper driver of the CrowPanel board: `EPD.cpp`, `EPD.h`, `EPDfont.h`, `spi.cpp`, `spi.h` (unchanged copies) and `EPD_Init.cpp`, `EPD_Init.h` (modified). The origin is Elecrow's public repository for this board (https://github.com/Elecrow-RD/CrowPanel-ESP32-2.13-E-paper-HMI-Display-with-122-250). `EPDfont.h` is also the source of the bitmap data of the "Classic" font family.

- **Permission:** the manufacturer's repository carries no LICENSE file and the files have no license headers. Elecrow support confirmed the following by e-mail (October 2026); the e-mails are kept by the project owner:
  - the driver files may be included in this public repository;
  - ready-to-flash firmware binaries containing them may be published free of charge (for example as GitHub releases);
  - Elecrow devices may be resold flashed with this firmware. The seller flashes the firmware and is responsible for all software-related support, returns and exchanges; Elecrow's warranty covers hardware faults only, under Elecrow's own policies;
  - modified versions of the files may be published and distributed. Elecrow asked that modified versions **not** carry Elecrow's branding, so `EPD_Init.cpp` / `EPD_Init.h` only say that they are derived from the manufacturer's sample driver and are not supported by the manufacturer;
  - products may be described as compatible with the Elecrow CrowPanel ESP32 2.13" E-paper display.

  Elecrow could not provide a public license text or statement, so **no license text exists** for these files.
- **What that means:** the files stay the property of Elecrow and are **not** covered by this project's MIT license. They are published here for use with the CrowPanel hardware. If you reuse them outside this project, ask Elecrow yourself; the permission above was given to this project.
- **Modifications:** `EPD_Init.cpp` / `EPD_Init.h` were changed for hardware V1.2 (JD79661 initialisation, `EPD_DisplayImageFast()` for partial refresh). The other driver files are unchanged copies.
- InkBlink is an independent project. It is not made, endorsed or supported by Elecrow.

## Manufacturer's CAD data

The STEP model of the board (`3D file` folder in Elecrow's repository) is not included. Dimensions quoted in `docs/HARDWARE.md` were read from it for personal enclosure design and should be verified with calipers.

## Test code

Everything under `tests/` (including the stand-in libraries) was written for this project and contains no third-party code.
