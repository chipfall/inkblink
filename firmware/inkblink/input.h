#pragma once
// ============================================================
//  Zdarzenia przyciskow: krotkie / dlugie wcisniecie i powtarzanie
//  przy przytrzymaniu. Zalezy tylko od boardKeyDown(Key) z pliku plytki,
//  wiec dziala tak samo na kazdej plytce.
//
//  UP / DOWN (nawigacja):  EV_PRESS od razu po wcisnieciu,
//                          potem EV_REPEAT co 250 ms po 500 ms trzymania
//                          (repeat = numer powtorzenia 1, 2, 3...).
//  OK / BACK / HOME:       EV_SHORT po puszczeniu (krotkie wcisniecie)
//                          albo EV_LONG po 800 ms trzymania (raz).
// ============================================================
#include <Arduino.h>

enum EvType : uint8_t { EV_NONE, EV_PRESS, EV_REPEAT, EV_SHORT, EV_LONG };

struct KeyEvent {
  Key key;
  EvType type;
  uint8_t repeat;
};

#define LONG_PRESS_MS 800
#define REPEAT_DELAY_MS 500
#define REPEAT_EVERY_MS 250
#define DEBOUNCE_MS 20
#define LOCKOUT_MS 60

inline bool keyIsNav(Key k) {
  return k == KEY_UP || k == KEY_DOWN;
}

inline bool isStep(const KeyEvent& e) {
  return e.type == EV_PRESS || e.type == EV_REPEAT;
}
inline bool isShort(const KeyEvent& e) {
  return e.type == EV_SHORT;
}
inline bool isLong(const KeyEvent& e) {
  return e.type == EV_LONG;
}

// Wolac w kazdym obiegu petli. Nie blokuje (poza kilkunastoma ms na debounce).
inline KeyEvent inputPoll() {
  static Key active = KEY_NONE;
  static unsigned long tDown = 0, lastFire = 0, tUp = 0;
  static bool longFired = false;
  static uint8_t nRep = 0;

  KeyEvent ev = { KEY_NONE, EV_NONE, 0 };
  unsigned long now = millis();

  if (active == KEY_NONE) {
    if (now - tUp < LOCKOUT_MS) return ev;       // drgania po puszczeniu
    static const Key order[] = { KEY_UP, KEY_DOWN, KEY_OK, KEY_BACK, KEY_HOME };
    for (Key k : order) {
      if (boardKeyDown(k)) {
        delay(DEBOUNCE_MS);
        if (!boardKeyDown(k)) return ev;          // to byly drgania
        active = k;
        tDown = lastFire = millis();
        longFired = false;
        nRep = 0;
        if (keyIsNav(k)) ev = KeyEvent{ k, EV_PRESS, 0 };
        return ev;
      }
    }
    return ev;
  }

  if (!boardKeyDown(active)) {
    delay(10);                                    // drgania podczas trzymania?
    if (boardKeyDown(active)) return ev;
    Key k = active;
    active = KEY_NONE;
    tUp = millis();
    if (!keyIsNav(k) && !longFired) ev = KeyEvent{ k, EV_SHORT, 0 };
    return ev;
  }

  if (keyIsNav(active)) {
    if (now - tDown >= REPEAT_DELAY_MS && now - lastFire >= REPEAT_EVERY_MS) {
      lastFire = now;
      if (nRep < 255) nRep++;
      ev = KeyEvent{ active, EV_REPEAT, nRep };
    }
  } else if (!longFired && now - tDown >= LONG_PRESS_MS) {
    longFired = true;
    ev = KeyEvent{ active, EV_LONG, 0 };
  }
  return ev;
}
