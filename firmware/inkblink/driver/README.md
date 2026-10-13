# E-paper driver files (not in this repository)

The firmware talks to the screen through a small driver API. For the CrowPanel V1.2 this is Elecrow's driver code, which is **not redistributed here** until Elecrow has granted permission (see `THIRD_PARTY_NOTICES.md`).

## What you need locally

Put these seven files **next to `inkblink.ino`** (they are listed in `.gitignore`, so they will never be committed by accident):

| File | Origin |
|---|---|
| `EPD.cpp`, `EPD.h`, `EPDfont.h` | Elecrow's repository (example / factory source) |
| `spi.cpp`, `spi.h` | Elecrow's repository (pins SCK 12, MOSI 11, RES 10, DC 13, CS 14, BUSY 9) |
| `EPD_Init.cpp`, `EPD_Init.h` | Elecrow's files **modified** for hardware V1.2 (JD79661 controller, inverted BUSY) plus `EPD_DisplayImageFast()` |

Elecrow's stock `EPD_Init.cpp` from the factory source does **not** initialise a V1.2 board (it waits forever on BUSY), so the modified version is required. If you are the project owner, your local sketch folder already has all seven files.

## API the firmware expects (`board_crowpanel.h`)

```c
#define EPD_W / EPD_H / ALLSCREEN_BYTES / USE_HORIZONTIAL 2 / WHITE / BLACK
extern uint8_t ImageBW[ALLSCREEN_BYTES];            // 1 bit per pixel frame buffer
void EPD_Init(void);
void EPD_Update(void);                              // run the refresh programmed by the last call below
void EPD_Sleep(void);
void EPD_DisplayImage(const uint8_t* img);          // full-refresh image upload
void EPD_DisplayImageFast(const uint8_t* newImg, const uint8_t* oldImg);  // partial refresh (DU waveform)
void EPD_DrawPoint(uint16_t x, uint16_t y, uint8_t color);
void EPD_DrawLine(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint8_t color);
void EPD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint8_t color, uint8_t sizey);  // "Classic" font
```

Pixel semantics are documented in `gfxInk()`, `gfxWhite()` and `gfxIsInk()` in `board_crowpanel.h`. A stand-in implementation of exactly this API for tests lives in `tests/stubs/` (`EPD_stub.cpp`); it is a good starting point for a different driver (for example GxEPD2 on another board).

## Running the driver-dependent tests

With the seven files in place:

```sh
make -C tests test-driver
```

This also runs the tests that check the real register sequence of the fast refresh.
