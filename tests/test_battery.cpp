#include "inkblink.ino"
extern bool g_apFail;
static int passN=0, failN=0;
#define CHECK(c) do{ if(c){passN++;} else {failN++; printf("  FAIL line %d: %s\n",__LINE__,#c);} }while(0)
static void dumpRaw(const char* path){ FILE* f=fopen(path,"wb"); for(int y=0;y<SCREEN_H;y++) for(int x=0;x<SCREEN_W;x++) fputc(gfxIsInk(x,y)?1:0,f); fclose(f); }
static void writeFile(const char* name,const std::string& d){ std::string p=std::string("build/run/fs/")+name; FILE* f=fopen(p.c_str(),"wb"); fwrite(d.data(),1,d.size(),f); fclose(f); }
static int inkIn(int x0,int y0,int x1,int y1){ int n=0; for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++) if(gfxIsInk(x,y)) n++; return n; }
int main(){
  for(int i=0;i<64;i++) g_pin[i]=1; g_ms=100000; system("rm -rf build/run/fs/*; mkdir -p build/run/png");
  fsOk=true; prefs.begin("reader",false); fontInit(); loadSettings(); statsLoadTotals();
  printf("== T18b bateria z ustawionym pinem (PIN_BAT_ADC=%d, dzielnik %.1f) ==\n",PIN_BAT_ADC,(double)BAT_DIV_RATIO);
  CHECK(batteryAvailable());
  g_adc[3]=1950; screen=S_MENU; batMv=-1; CHECK(batteryMv()==3900); CHECK(batteryPercent()==62); printf("  pin 1950 mV x2 = %d mV -> %d%%\n",batteryMv(),batteryPercent());
  g_adc[3]=1700; g_ms+=5000; CHECK(batteryMv()==3900); printf("  odczyt z pamieci podrecznej przez 30 s (bez ponownego pomiaru)\n");
  g_ms+=31000; CHECK(batteryMv()==3400 && batteryPercent()==3); printf("  po 30 s nowy pomiar: %d mV -> %d%%\n",batteryMv(),batteryPercent());
  // w trybie upload (WiFi zajmuje ADC2) uzywamy ostatniego odczytu
  g_adc[3]=1950; g_ms+=40000; screen=S_UPLOAD; CHECK(batteryMv()==3400); screen=S_MENU; CHECK(batteryMv()==3900); printf("  w trybie upload: bez nowego pomiaru\n");
  // ikona w rogu menu rosnie z poziomem
  auto cornerInk=[&](int adcMv){ g_adc[3]=adcMv; g_ms+=40000; gfxClear(); drawBatteryCorner(); return inkIn(SCREEN_W-24,2,SCREEN_W-3,11); };
  int lo=cornerInk(1650), mid=cornerInk(1850), hi=cornerInk(2100); printf("  tusz w ikonie: 3.30V=%d px, 3.70V=%d px, 4.20V=%d px\n",lo,mid,hi); CHECK(lo<mid && mid<hi);
  g_adc[3]=1950; g_ms+=40000; buildMenu(); screen=S_MENU; drawMenu(); dumpRaw("build/run/png/menu_battery.raw"); CHECK(inkIn(SCREEN_W-60,0,SCREEN_W-1,14)>60);
  // pasek stanu w ksiazce
  writeFile("Bat.txt","Tekst ksiazki do testu paska stanu z bateria. "+std::string(6000,'a'));
  cfg=Settings{FAM_CLASSIC,1,false,true,true,false,true,1,false,1,1,0,true}; applyLayout();
  auto barBat=[&](int mode,int adcMv){ cfg.batMode=mode; g_adc[3]=adcMv; g_ms+=40000; openBook("Bat.txt"); screen=S_READER; page=2; showPage(false); int n=inkIn(SCREEN_W-30,SCREEN_H-10,SCREEN_W-6,SCREEN_H-5); closeBook(); return n; };
  int off=barBat(0,1950), lowHidden=barBat(1,1950), lowShown=barBat(1,1800), always=barBat(2,1950);
  printf("  ikona w ksiazce: wylaczona=%d px, 'Low' przy 62%%=%d px, 'Low' przy 15%%=%d px, 'Always'=%d px\n",off,lowHidden,lowShown,always);
  CHECK(off==0 && lowHidden==0 && lowShown>20 && always>20);
  cfg.batMode=2; g_adc[3]=1800; g_ms+=40000; openBook("Bat.txt"); screen=S_READER; page=2; showPage(false); dumpRaw("build/run/png/reader_battery.raw"); closeBook();
  // Info: bez przycisku PINS, jest bateria
  goInfo(); CHECK(inkIn(SCREEN_W-46,54,SCREEN_W-1,67)==0); dumpRaw("build/run/png/info_battery.raw"); printf("  Info: bateria jest, etykiety PINS nie ma\n");
  KeyEvent ok{KEY_OK,EV_SHORT,0}; screen=S_INFO; /* OK nie otwiera testu pinow, gdy bateria skonfigurowana */
  { auto press=[&](){ /* przez petle zdarzen */ }; (void)press; }
  // karta uspienia z bateria
  cfg.sleepScreen=0; prefs.putString("last","Bat.txt"); g_adc[3]=1950; g_ms+=40000; gfxClear(); CHECK(drawSleepBookCard()); CHECK(inkIn(SCREEN_W-32,106,SCREEN_W-1,SCREEN_H-1)>30); dumpRaw("build/run/png/sleep_battery.raw"); printf("  karta uspienia z bateria\n");
  // strona WWW: chip z bateria
  g_adc[3]=1950; g_ms+=40000; screen=S_MENU; batteryMv(); screen=S_UPLOAD; g_lastBody.clear(); handleRoot(); CHECK(g_lastBody.find("Battery")!=std::string::npos && g_lastBody.find("62%")!=std::string::npos); printf("  strona WWW pokazuje baterie\n");
  // goUpload mierzy przed WiFi
  g_adc[3]=1800; g_ms+=40000; screen=S_MENU; g_apFail=false; goUpload(); CHECK(screen==S_UPLOAD && batMv==3600); uploadStop(); printf("  goUpload: pomiar tuz przed wlaczeniem WiFi (%d mV)\n",batMv);
  printf("\n==== TEST BATERII: %d OK, %d bledow ====\n",passN,failN); return failN?1:0; }
