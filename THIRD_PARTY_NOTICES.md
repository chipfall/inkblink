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

- **Permission:** the repository carries no LICENSE file and the files have no license headers. We asked Elecrow in writing whether the driver files may be included in this public repository and received the answer (e-mail from Elecrow support, October 2026): *"Yes, you can include in your public repository the driver files."* We asked which license applies to the files and whether a LICENSE file could be added; the reply only said the feedback would be passed on to the relevant departments, so **no license text exists**.
- **What that means:** these files stay the property of Elecrow and are **not** covered by this project's MIT license. They are published here with Elecrow's permission, for use with the CrowPanel hardware. The permission as we received it mentions the repository only; it does not explicitly mention compiled binaries, pre-flashed devices or other commercial use. If you intend those, ask Elecrow for explicit permission first.
- **Modifications:** `EPD_Init.cpp` / `EPD_Init.h` were changed for hardware V1.2 (JD79661 initialisation, `EPD_DisplayImageFast()` for partial refresh). They carry a header comment saying so.
- The original e-mail is kept by the project owner.

## Manufacturer's CAD data

The STEP model of the board (`3D file` folder in Elecrow's repository) is not included. Dimensions quoted in `docs/HARDWARE.md` were read from it for personal enclosure design and should be verified with calipers.

## Test code

Everything under `tests/` (including the stand-in libraries) was written for this project and contains no third-party code.
