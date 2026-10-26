#pragma once
// ============================================================
//  Simple interface graphics built on gfxInk / gfxWhite
//  (x from the LEFT edge of the screen, y from the top). Independent of the board.
//  Needs before it: board_crowpanel.h, qr.h
// ============================================================
#include <Arduino.h>
#include <math.h>

// Rectangle filled with ink (bounds inclusive)
inline void uiFillRect(int x0, int y0, int x1, int y1) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) gfxInk(x, y);
}

// "White" rectangle (erases ink)
inline void uiClearRect(int x0, int y0, int x1, int y1) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) gfxWhite(x, y);
}

// 1 px frame
inline void uiRect(int x0, int y0, int x1, int y1) {
  for (int x = x0; x <= x1; x++) { gfxInk(x, y0); gfxInk(x, y1); }
  for (int y = y0; y <= y1; y++) { gfxInk(x0, y); gfxInk(x1, y); }
}

// Filled circle
inline void uiDisc(int cx, int cy, int r, bool ink) {
  for (int y = -r; y <= r; y++)
    for (int x = -r; x <= r; x++)
      if (x * x + y * y <= r * r) {
        if (ink) gfxInk(cx + x, cy + y);
        else gfxWhite(cx + x, cy + y);
      }
}

// WiFi icon: dot and three arcs, 16 x 12 px box (x, y = top left corner)
inline void uiWifiIcon(int x, int y) {
  const float cx = x + 7.5f;
  const float cy = y + 10.0f;
  static const float R[3] = { 4.0f, 7.0f, 10.0f };
  for (int i = 0; i < 3; i++) {
    for (int a = 225; a <= 315; a++) {
      float rad = a * 0.01745329f;
      for (int t = 0; t < 2; t++) {
        float r = R[i] - t * 0.9f;
        gfxInk((int)lroundf(cx + r * cosf(rad)), (int)lroundf(cy + r * sinf(rad)));
      }
    }
  }
  uiFillRect(x + 6, y + 9, x + 8, y + 11);        // dot
}

// Battery: 20 x 10 outline + tip, fill proportional to the percentage
// (takes 22 x 10 px; x, y = top left corner)
inline void uiBattery(int x, int y, int pct) {
  uiRect(x, y, x + 19, y + 9);
  uiFillRect(x + 20, y + 3, x + 21, y + 6);       // tip
  if (pct < 0) return;
  if (pct > 100) pct = 100;
  int w = pct * 16 / 100;
  if (pct > 0 && w < 1) w = 1;
  if (w > 0) uiFillRect(x + 2, y + 2, x + 1 + w, y + 7);
}

// QR code: every module is a scale x scale px square; (x, y) = top left corner of the first module
inline void uiQr(const QrCode& q, int x, int y, int scale) {
  for (int my = 0; my < q.size; my++)
    for (int mx = 0; mx < q.size; mx++)
      if (qrDark(q, mx, my)) uiFillRect(x + mx * scale, y + my * scale, x + mx * scale + scale - 1, y + my * scale + scale - 1);
}
