#pragma once
#include "Arduino.h"
class Adafruit_GFX { public: Adafruit_GFX(int16_t w,int16_t h):W(w),H(h){} virtual ~Adafruit_GFX(){} virtual void drawPixel(int16_t x,int16_t y,uint16_t c)=0; int16_t W,H; };
