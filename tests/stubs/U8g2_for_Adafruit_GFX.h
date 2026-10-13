#pragma once
#include "Adafruit_GFX.h"
extern const uint8_t u8g2_font_helvR10_te[]; extern const uint8_t u8g2_font_helvR12_te[]; extern const uint8_t u8g2_font_helvR14_te[]; extern const uint8_t u8g2_font_helvR18_te[]; 
extern const uint8_t u8g2_font_helvB10_te[]; extern const uint8_t u8g2_font_helvB12_te[]; extern const uint8_t u8g2_font_helvB14_te[]; extern const uint8_t u8g2_font_helvB18_te[]; 
extern const uint8_t u8g2_font_ncenR10_te[]; extern const uint8_t u8g2_font_ncenR12_te[]; extern const uint8_t u8g2_font_ncenR14_te[]; extern const uint8_t u8g2_font_ncenR18_te[]; 
extern const uint8_t u8g2_font_ncenB10_te[]; extern const uint8_t u8g2_font_ncenB12_te[]; extern const uint8_t u8g2_font_ncenB14_te[]; extern const uint8_t u8g2_font_ncenB18_te[]; 
extern const uint8_t u8g2_font_unifont_t_polish[]; 
struct StubFont{const uint8_t* p;int size;double wf;};
static inline StubFont stubFontOf(const uint8_t* f){ static const StubFont t[]={
  {u8g2_font_helvR10_te,10,0.52},
  {u8g2_font_helvR12_te,12,0.52},
  {u8g2_font_helvR14_te,14,0.52},
  {u8g2_font_helvR18_te,18,0.52},
  {u8g2_font_helvB10_te,10,0.52},
  {u8g2_font_helvB12_te,12,0.52},
  {u8g2_font_helvB14_te,14,0.52},
  {u8g2_font_helvB18_te,18,0.52},
  {u8g2_font_ncenR10_te,10,0.50},
  {u8g2_font_ncenR12_te,12,0.50},
  {u8g2_font_ncenR14_te,14,0.50},
  {u8g2_font_ncenR18_te,18,0.50},
  {u8g2_font_ncenB10_te,10,0.50},
  {u8g2_font_ncenB12_te,12,0.50},
  {u8g2_font_ncenB14_te,14,0.50},
  {u8g2_font_ncenB18_te,18,0.50},
  {nullptr,0,0}}; for(int i=0;t[i].p;i++) if(t[i].p==f) return t[i]; return {nullptr,12,0.5}; }
// atrapa: szerokosci "jak w proporcjonalnym foncie", rysuje ramki zamiast liter
class U8G2_FOR_ADAFRUIT_GFX { Adafruit_GFX* g=nullptr; const uint8_t* f=nullptr; uint16_t fg=1;
 int cpw(unsigned cp){ if(f==u8g2_font_unifont_t_polish) return 8; StubFont sf=stubFontOf(f); int base=(int)(sf.size*sf.wf+0.5)+(f&&sf.p&&sf.p==f&&false?0:0);
   if(cp=='i'||cp=='l'||cp=='j'||cp=='.'||cp==','||cp==' '||cp=='\''||cp=='!'||cp==0x142) return base/2+1; if(cp=='m'||cp=='w'||cp=='M'||cp=='W') return base+base/2+1; return base; }
 public:
 void begin(Adafruit_GFX& x){g=&x;} void setFont(const uint8_t* x){f=x;} void setFontMode(uint8_t){} void setFontDirection(uint8_t){} void setForegroundColor(uint16_t c){fg=c;}
 int16_t getFontAscent(){ if(f==u8g2_font_unifont_t_polish) return 13; return (int)(stubFontOf(f).size*0.8); }
 int16_t getFontDescent(){ if(f==u8g2_font_unifont_t_polish) return -3; return -(int)(stubFontOf(f).size*0.22+0.5); }
 int adv(unsigned cp){ return cpw(cp); }
 int vis(unsigned cp){ return cp==' ' ? 0 : std::max(1, cpw(cp)-1); }
 int16_t getUTF8Width(const char* s){ int w=0; unsigned last=0; bool any=false; for(int i=0;s[i];){ unsigned c=(unsigned char)s[i]; unsigned cp=c; int n=1; if((c&0xE0)==0xC0){cp=((c&0x1F)<<6)|(s[i+1]&0x3F);n=2;} if(any) w+=adv(last); last=cp; any=true; i+=n;} if(any) w+=vis(last); return w; }
 uint16_t drawUTF8(int16_t x,int16_t y,const char* s){ int asc=getFontAscent(); int px=x; for(int i=0;s[i];){ unsigned c=(unsigned char)s[i]; unsigned cp=c; int n=1; if((c&0xE0)==0xC0){cp=((c&0x1F)<<6)|(s[i+1]&0x3F);n=2;}
   int w=cpw(cp); bool capAcc=(cp==0x104||cp==0x106||cp==0x118||cp==0x141||cp==0x143||cp==0xD3||cp==0x15A||cp==0x179||cp==0x17B);
   bool lowAcc=(cp==0x105||cp==0x107||cp==0x119||cp==0x142||cp==0x144||cp==0xF3||cp==0x15B||cp==0x17A||cp==0x17C);
   bool tail=(cp=='g'||cp=='j'||cp=='p'||cp=='q'||cp=='y'||cp==0x105||cp==0x119);
   if(capAcc) g->drawPixel(px+w/2,y-asc-3,fg); if(lowAcc) g->drawPixel(px+w/2,y-asc-1,fg); if(tail) g->drawPixel(px+w/2,y+4,fg);
   if(cp!=' '){ for(int dx=0;dx<w-1;dx++){ g->drawPixel(px+dx,y-asc+2,fg); g->drawPixel(px+dx,y,fg);} for(int dy=2;dy<=asc;dy++){ g->drawPixel(px,y-asc+dy,fg); g->drawPixel(px+w-2,y-asc+dy,fg);} }
   px+=w; i+=n;} return px-x; }
};
