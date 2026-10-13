#include "Arduino.h"
#include "WiFi.h"
#include "WebServer.h"
#include "LittleFS.h"
int g_pin[64]; int (*g_readHook)(int)=nullptr; unsigned long g_ms=0; SerialT Serial; WiFiClass WiFi; FSC LittleFS;
int g_wifiStatus=3; int g_wifiMode=0; bool g_apUp=false; bool g_slept=false;
bool g_authOk=true; int g_lastCode=0; std::string g_lastBody,g_lastLoc; int g_authRequested=0; std::map<std::string,std::string> g_args; HTTPUpload g_up;
bool g_dnsUp=false; std::string g_uri;

bool g_apFail=false;

int g_stations=0; int g_onEventCalls=0;

int g_adc[64];


extern const uint8_t u8g2_font_helvR10_te[1]; const uint8_t u8g2_font_helvR10_te[1]={0};
extern const uint8_t u8g2_font_helvR12_te[1]; const uint8_t u8g2_font_helvR12_te[1]={0};
extern const uint8_t u8g2_font_helvR14_te[1]; const uint8_t u8g2_font_helvR14_te[1]={0};
extern const uint8_t u8g2_font_helvR18_te[1]; const uint8_t u8g2_font_helvR18_te[1]={0};
extern const uint8_t u8g2_font_helvB10_te[1]; const uint8_t u8g2_font_helvB10_te[1]={0};
extern const uint8_t u8g2_font_helvB12_te[1]; const uint8_t u8g2_font_helvB12_te[1]={0};
extern const uint8_t u8g2_font_helvB14_te[1]; const uint8_t u8g2_font_helvB14_te[1]={0};
extern const uint8_t u8g2_font_helvB18_te[1]; const uint8_t u8g2_font_helvB18_te[1]={0};
extern const uint8_t u8g2_font_ncenR10_te[1]; const uint8_t u8g2_font_ncenR10_te[1]={0};
extern const uint8_t u8g2_font_ncenR12_te[1]; const uint8_t u8g2_font_ncenR12_te[1]={0};
extern const uint8_t u8g2_font_ncenR14_te[1]; const uint8_t u8g2_font_ncenR14_te[1]={0};
extern const uint8_t u8g2_font_ncenR18_te[1]; const uint8_t u8g2_font_ncenR18_te[1]={0};
extern const uint8_t u8g2_font_ncenB10_te[1]; const uint8_t u8g2_font_ncenB10_te[1]={0};
extern const uint8_t u8g2_font_ncenB12_te[1]; const uint8_t u8g2_font_ncenB12_te[1]={0};
extern const uint8_t u8g2_font_ncenB14_te[1]; const uint8_t u8g2_font_ncenB14_te[1]={0};
extern const uint8_t u8g2_font_ncenB18_te[1]; const uint8_t u8g2_font_ncenB18_te[1]={0};
extern const uint8_t u8g2_font_unifont_t_polish[1]; const uint8_t u8g2_font_unifont_t_polish[1]={0};
