#pragma once
#include "Arduino.h"
enum wl_status_t { WL_IDLE_STATUS=0, WL_CONNECTED=3, WL_CONNECT_FAILED=4, WL_DISCONNECTED=6 };
enum WiFiEvent_t { ARDUINO_EVENT_WIFI_AP_STACONNECTED=1, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED=2, ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED=3 };
union WiFiEventInfo_t { int dummy; };
extern int g_stations; extern int g_onEventCalls;
#define WIFI_OFF 0
#define WIFI_STA 1
#define WIFI_AP 2
extern int g_wifiStatus; extern int g_wifiMode; extern bool g_apUp;
struct WiFiClass { void onEvent(void(*)(WiFiEvent_t,WiFiEventInfo_t)){ g_onEventCalls++; } int softAPgetStationNum(){ return g_stations; } void mode(int m){g_wifiMode=m;} void begin(const char*,const char*){} wl_status_t status(){return (wl_status_t)g_wifiStatus;}
  void disconnect(bool=false){} IPAddress localIP(){return IPAddress();} bool softAP(const char*,const char* pw,int=1,int=0,int=4){ extern bool g_apFail; g_apUp = !g_apFail && strlen(pw)>=8; return g_apUp; }
  IPAddress softAPIP(){ IPAddress i; i.v="192.168.4.1"; return i;} bool softAPdisconnect(bool=false){ g_apUp=false; return true;}
  int scanNetworks(){ return 4; } String SSID(int i){ static const char* n[4]={"Dom","Dom","Sasiad","Siec z \"cudzyslowem\""}; return String(n[i]); } };
extern WiFiClass WiFi;
