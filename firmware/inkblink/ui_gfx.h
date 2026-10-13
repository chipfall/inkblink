#pragma once
// ============================================================
//  Proste elementy graficzne interfejsu zbudowane na gfxInk / gfxWhite
//  (x od LEWEJ krawedzi ekranu, y od gory). Niezalezne od plytki.
//  Wymaga wczesniej: board_crowpanel.h, qr.h
// ============================================================
#include <Arduino.h>
#include <math.h>

// Wypelniony prostokat tuszem (granice wlacznie)
inline void uiFillRect(int x0, int y0, int x1, int y1) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) gfxInk(x, y);
}

// Prostokat "bialy" (kasuje tusz)
inline void uiClearRect(int x0, int y0, int x1, int y1) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) gfxWhite(x, y);
}

// Ramka 1 px
inline void uiRect(int x0, int y0, int x1, int y1) {
  for (int x = x0; x <= x1; x++) { gfxInk(x, y0); gfxInk(x, y1); }
  for (int y = y0; y <= y1; y++) { gfxInk(x0, y); gfxInk(x1, y); }
}

// Wypelnione kolo
inline void uiDisc(int cx, int cy, int r, bool ink) {
  for (int y = -r; y <= r; y++)
    for (int x = -r; x <= r; x++)
      if (x * x + y * y <= r * r) {
        if (ink) gfxInk(cx + x, cy + y);
        else gfxWhite(cx + x, cy + y);
      }
}

// Ikona WiFi: kropka i trzy luki, ramka 16 x 12 px (x, y = lewy gorny rog)
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
  uiFillRect(x + 6, y + 9, x + 8, y + 11);        // kropka
}

// Bateria: obrys 20 x 10 + wypustka, wypelnienie proporcjonalne do procentow
// (zajmuje 22 x 10 px; x, y = lewy gorny rog)
inline void uiBattery(int x, int y, int pct) {
  uiRect(x, y, x + 19, y + 9);
  uiFillRect(x + 20, y + 3, x + 21, y + 6);       // wypustka
  if (pct < 0) return;
  if (pct > 100) pct = 100;
  int w = pct * 16 / 100;
  if (pct > 0 && w < 1) w = 1;
  if (w > 0) uiFillRect(x + 2, y + 2, x + 1 + w, y + 7);
}

// Kod QR: kazdy modul to kwadrat scale x scale px; (x, y) = lewy gorny rog pierwszego modulu
inline void uiQr(const QrCode& q, int x, int y, int scale) {
  for (int my = 0; my < q.size; my++)
    for (int mx = 0; mx < q.size; mx++)
      if (qrDark(q, mx, my)) uiFillRect(x + mx * scale, y + my * scale, x + mx * scale + scale - 1, y + my * scale + scale - 1);
}
