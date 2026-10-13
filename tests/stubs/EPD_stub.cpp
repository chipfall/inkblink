// Software model of the e-paper frame buffer used by the host tests.
// It reproduces only what the firmware relies on: the pixel mapping of ImageBW for
// orientation 2 (documented in board_crowpanel.h: gfxInk/gfxIsInk), point/line/char drawing,
// and a record of full vs. fast updates. Glyphs are simple procedural shapes (not a real font).
#include "EPD.h"
#include "trace.h"
#include <string.h>

uint8_t ImageBW[ALLSCREEN_BYTES];
std::vector<TraceCmd> g_trace;
bool g_epdLastFast = false;
int g_epdFullCount = 0, g_epdFastCount = 0;

void EPD_Init(void) {}
void EPD_Update(void) {}
void EPD_Sleep(void) {}
void EPD_DisplayImage(const uint8_t*) { g_epdLastFast = false; g_epdFullCount++; }
void EPD_DisplayImageFast(const uint8_t*, const uint8_t*) { g_epdLastFast = true; g_epdFastCount++; }

// color WHITE lights an ink pixel (clears the bit), BLACK clears ink - as the firmware expects.
void EPD_DrawPoint(uint16_t x, uint16_t y, uint8_t color) {
  if (x >= EPD_W || y >= EPD_H) return;
  int xpoint = y;
  int ypoint = EPD_W - x - 1;
  int bytesPerRow = (EPD_H % 8 == 0) ? (EPD_H / 8) : (EPD_H / 8 + 1);
  int addr = xpoint / 8 + ypoint * bytesPerRow;
  uint8_t mask = 0x80 >> (xpoint % 8);
  if (color == WHITE) ImageBW[addr] &= ~mask;
  else ImageBW[addr] |= mask;
}

void EPD_DrawLine(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint8_t color) {
  int dx = (int)xe - xs, dy = (int)ye - ys;
  int n = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
  if (n == 0) { EPD_DrawPoint(xs, ys, color); return; }
  for (int i = 0; i <= n; i++) EPD_DrawPoint(xs + dx * i / n, ys + dy * i / n, color);
}

// Draws a stand-in glyph (box outline plus a stroke that depends on the character) into a
// cell of size/2 x size. BLACK = ink glyph, WHITE = erase (white letter on a black tag).
// x is the driver-space coordinate of the cell's first column; columns run toward lower x,
// matching how the firmware mirrors coordinates for orientation 2.
void EPD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint8_t color, uint8_t sizey) {
  if (num == ' ') return;
  int w = sizey / 2, h = sizey;
  uint8_t pix = (color == BLACK) ? WHITE : BLACK;
  for (int dy = 1; dy < h - 1; dy++) {
    for (int dx = 1; dx < w - 1; dx++) {
      bool edge = (dx == 1 || dx == w - 2 || dy == 1 || dy == h - 2);
      bool stroke = ((dx + dy * (1 + num % 3)) % (2 + num % 4)) == 0;
      if (edge || stroke) EPD_DrawPoint(x - dx, y + dy, pix);
    }
  }
}
