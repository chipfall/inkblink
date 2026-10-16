#pragma once
// Test stand-in for the e-paper driver API that the firmware expects (board_crowpanel.h).
// The real driver (Elecrow's code, not part of this repository) provides the same names.
#include <Arduino.h>

#define USE_HORIZONTIAL 2
#define EPD_W 250
#define EPD_H 128        // RAM width of the panel (visible rows: 122), as in the real driver for orientation 2
#define ALLSCREEN_BYTES (((EPD_H % 8) ? (EPD_H / 8 + 1) : (EPD_H / 8)) * EPD_W)
#define WHITE 0xFF
#define BLACK 0x00

extern uint8_t ImageBW[ALLSCREEN_BYTES];

void EPD_Init(void);
void EPD_Update(void);
void EPD_Sleep(void);
void EPD_DisplayImage(const uint8_t* img);
void EPD_DisplayImageFast(const uint8_t* newImg, const uint8_t* oldImg);
