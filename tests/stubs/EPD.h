#pragma once
#include "EPD_Init.h"
void EPD_DrawPoint(uint16_t x, uint16_t y, uint8_t color);
void EPD_DrawLine(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint8_t color);
void EPD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint8_t color, uint8_t sizey);
