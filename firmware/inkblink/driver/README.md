# E-paper driver

The firmware talks to the screen through a small driver API. For the CrowPanel V1.2 this is Elecrow's driver code, which lives next to `inkblink.ino`:

| File | Origin |
|---|---|
| `EPD.cpp`, `EPD.h`, `EPDfont.h` | Elecrow's repository, unchanged |
| `spi.cpp`, `spi.h` | Elecrow's repository, unchanged (pins SCK 12, MOSI 11, RES 10, DC 13, CS 14, BUSY 9) |
| `EPD_Init.cpp`, `EPD_Init.h` | Elecrow's files, **modified** for hardware V1.2 (JD79661 controller, inverted BUSY) plus `EPD_DisplayImageFast()` |

They are included with Elecrow's permission and are not covered by this project's MIT license; see `THIRD_PARTY_NOTICES.md`.

Elecrow's stock `EPD_Init.cpp` from the factory source does **not** initialise a V1.2 board (it waits forever on BUSY), so the modified version is required.

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

Pixel semantics are documented in `gfxInk()`, `gfxWhite()` and `gfxIsInk()` in `board_crowpanel.h`. A stand-in implementation of exactly this API for the host tests lives in `tests/stubs/` (`EPD_stub.cpp`); it is a good starting point for a different driver (for example GxEPD2 on another board).

## Tests

`make -C tests test` runs against the stand-in driver; `make -C tests test-driver` runs the tests that need the real driver (the fast-refresh register sequence).
