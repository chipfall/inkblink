#pragma once
#include <cstdint>
#include <algorithm>
using std::max; using std::min;
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#define HIGH 1
#define LOW 0
#define INPUT 1
#define OUTPUT 3
#define F(x) x
#define RTC_DATA_ATTR         // ESP32: zmienna w pamieci RTC (przetrwa gleboki sen)
#define bit(b) (1UL << (b))   // jak w rdzeniu Arduino (koliduje z nazwami w kodzie, patrz qr.h)
#ifndef PROGMEM
#define PROGMEM
#endif
extern int g_pin[64];
extern unsigned long g_ms;
inline void pinMode(int,int){}
inline void digitalWrite(int p,int v){ g_pin[p]=v; }
extern int (*g_readHook)(int);
inline int digitalRead(int p){ if(g_readHook) return g_readHook(p); return g_pin[p]; }
inline unsigned long millis(){ return g_ms; }
inline void delay(unsigned long d){ g_ms+=d; }
inline void delayMicroseconds(unsigned int){}
inline bool psramFound(){ return false; }
inline void* ps_malloc(size_t n){ return malloc(n); }
inline long random(long m){ return 123456789L % m; }
inline uint32_t esp_random(){ return 123456789u; }
class String { public:
  std::string s;
  String(){} String(const char* c):s(c?c:""){} String(int v):s(std::to_string(v)){} String(unsigned v):s(std::to_string(v)){}
  const char* c_str() const {return s.c_str();} size_t length() const {return s.size();}
  void reserve(size_t){}
  String& operator+=(const char* c){s+=c;return *this;} String& operator+=(const String& o){s+=o.s;return *this;} String& operator+=(char c){s+=c;return *this;}
  char operator[](size_t i) const {return s[i];}
  bool operator==(const String& o) const {return s==o.s;} bool operator!=(const String& o) const {return s!=o.s;}
  bool operator==(const char* c) const {return s==c;} bool operator!=(const char* c) const {return s!=c;}
  void trim(){ size_t a=s.find_first_not_of(" \t\r\n"); if(a==std::string::npos){s="";return;} size_t b=s.find_last_not_of(" \t\r\n"); s=s.substr(a,b-a+1);} 
  int toInt() const {return atoi(s.c_str());}
  float toFloat() const {return (float)atof(s.c_str());}
};
struct IPAddress { std::string v="192.168.0.152"; String toString() const {return String(v.c_str());} };
struct SerialT { void begin(int){} void println(const char* x){ if(getenv("VERBOSE")) printf("[serial] %s\n",x);} void println(const IPAddress& i){ if(getenv("VERBOSE")) printf("[serial] ip %s\n", i.v.c_str());}
  int printf(const char* f,...) __attribute__((format(printf,2,3))) { if(!getenv("VERBOSE")) return 0; va_list a; va_start(a,f); printf("[serial] "); int r=vprintf(f,a); va_end(a); return r; } };
extern SerialT Serial;

struct EspClass { uint64_t getEfuseMac(){ return 0x0000A1B2C3D4E5F6ULL & 0xFFFFFFFFFFFFULL; } uint32_t getFreeHeap(){ return 200000; } };
static EspClass ESP;

#define ADC_11db 3
extern int g_adc[64];
inline void analogSetPinAttenuation(int,int){}
inline int analogReadMilliVolts(int pin){ return g_adc[pin]; }
