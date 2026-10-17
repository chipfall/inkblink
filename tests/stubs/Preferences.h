#pragma once
#include "Arduino.h"
class Preferences { public: std::map<std::string,std::string> s; std::map<std::string,uint32_t> u;
  bool begin(const char*,bool){return true;}
  std::map<std::string,std::string> b;
  std::map<std::string,float> f;
  bool isKey(const char* k){ return s.count(k)||u.count(k)||b.count(k); }
  size_t getBytes(const char* k,void* buf,size_t n){ if(!b.count(k)) return 0; size_t m=std::min(n,b[k].size()); memcpy(buf,b[k].data(),m); return m; }
  size_t putBytes(const char* k,const void* buf,size_t n){ b[k]=std::string((const char*)buf,n); return n; }
  String getString(const char* k,const String& d=String()){ return s.count(k)? String(s[k].c_str()):d; } void putString(const char* k,const String& v){ s[k]=v.s; }
  uint32_t getUInt(const char* k,uint32_t d){ return u.count(k)?u[k]:d; } void putUInt(const char* k,uint32_t v){ u[k]=v; }
  uint8_t getUChar(const char* k,uint8_t d){ return u.count(k)?(uint8_t)u[k]:d; } void putUChar(const char* k,uint8_t v){ u[k]=v; }
  float getFloat(const char* k,float d){ auto it=f.find(k); return it==f.end()?d:it->second; } void putFloat(const char* k,float v){ f[k]=v; }
  bool getBool(const char* k,bool d){ return u.count(k)?u[k]!=0:d; } void putBool(const char* k,bool v){ u[k]=v; } };
