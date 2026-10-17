// Battery estimate without a sensor (battery_model.h + integration), manual voltage entry, log, power saving.
#include "inkblink.ino"
#include <fstream>
#include <sstream>
static int passN=0, failN=0;
#define CHECK(c) do{ if(c){passN++;} else {failN++; printf("  FAIL line %d: %s\n",__LINE__,#c);} }while(0)
#define SECTION(n) printf("\n== %s ==\n",n)
static std::string fsRead(const char* name){ std::ifstream f(std::string("build/run/fs/")+name,std::ios::binary); std::stringstream ss; ss<<f.rdbuf(); return ss.str(); }
static bool near(float a,float b,float tol=0.02f){ return fabsf(a-b)<=tol; }
static time_t g_fake=0;
static const time_t SUMMER=1791894900;   // 2026-10-13 12:35 UTC
extern bool g_slept;
static void writeFile(const char* name,const std::string& d){ std::string p=std::string("build/run/fs/")+name; FILE* f=fopen(p.c_str(),"wb"); fwrite(d.data(),1,d.size(),f); fclose(f); }
static int inkIn(int x0,int y0,int x1,int y1){ int n=0; for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++) if(gfxIsInk(x,y)) n++; return n; }
static int lines(const std::string& s){ int n=0; for(char c:s) if(c=='\n') n++; return n; }
int main(){
  for(int i=0;i<64;i++) g_pin[i]=1; g_ms=100000; system("rm -rf build/run/fs/*; mkdir -p build/run/png");
  fsOk=true; prefs.begin("reader",false); fontInit(); loadSettings(); statsLoadTotals();
  clockNowHook=[]()->time_t{ return g_fake; }; clockSetHook=[](time_t t){ g_fake=t; }; clockApplyTz(0);
  cfg=Settings{FAM_CLASSIC,1,false,true,true,false,true,1,false,1,1,0,true,0,0,false,0,false,0,true,0,1,0,true,2}; applyLayout();

  SECTION("T50 model: krzywa napiecia, zuzycie, granice");
  { CHECK(near(bmPctFromVolt(4.20f),100)&&near(bmPctFromVolt(3.27f),0)&&near(bmPctFromVolt(3.85f),55)&&near(bmPctFromVolt(5.0f),100)&&near(bmPctFromVolt(2.0f),0));
    float last=-1; bool mono=true; for(int i=0;i<=100;i++){ float p=bmPctFromVolt(3.2f+i*0.01f); if(p<last-0.001f) mono=false; last=p; } CHECK(mono);
    bool rt=true; for(int p=0;p<=100;p+=5){ if(!near(bmPctFromVolt(bmVoltFromPct((float)p)),(float)p,0.3f)) rt=false; } CHECK(rt); printf("  krzywa: 4,20 V = %.0f%%, 3,85 V = %.0f%%, 3,70 V = %.0f%% (monotoniczna, odwracalna)\n",bmPctFromVolt(4.2f),bmPctFromVolt(3.85f),bmPctFromVolt(3.70f));
    BatModel m; bmReset(m,1200); CHECK(!m.valid && bmPercent(m)==0); bmSetPercent(m,50); CHECK(m.valid && near(m.remMah,600,0.01f) && near(bmPercent(m),50));
    bmAddRun(m,false,0,3600); CHECK(near(m.remMah,600-BM_AWAKE_MA_OFF,0.01f) && near(m.awakeS,3600,0.1f));
    bmAddRun(m,true,0,3600); CHECK(near(m.remMah,600-BM_AWAKE_MA_OFF-BM_HOTSPOT_MA,0.01f) && near(m.hotspotS,3600,0.1f));
    float r=m.remMah; bmAddFlips(m,10,100); CHECK(near(m.remMah,r-10*BM_FLIP_FULL_MAH-100*BM_FLIP_FAST_MAH,0.001f) && m.flipsFull==10 && m.flipsFast==100);
    r=m.remMah; bmAddSleep(m,36000); CHECK(near(m.remMah,r-BM_SLEEP_MA*10,0.001f) && near(m.sleepS,36000,0.1f));
    bmAddRun(m,true,0,1e7f); CHECK(m.remMah==0 && near(bmPercent(m),0)); bmSetPercent(m,80); CHECK(m.awakeS==0&&m.hotspotS==0&&m.sleepS==0&&m.flipsFast==0&&m.flipsFull==0); printf("  zuzycie: czuwanie %.1f mA, hotspot %.0f mA, odswiezenia, sen; dol zero; ustawienie zeruje liczniki\n",BM_AWAKE_MA_OFF,BM_HOTSPOT_MA);
    CHECK(near(bmAwakeMa(0),BM_AWAKE_MA_OFF)&&near(bmAwakeMa(1),BM_AWAKE_MA_CPU80)&&near(bmAwakeMa(2),BM_AWAKE_MA_LIGHT)); }

  SECTION("T51 integracja: wpisane napiecie, czas, odswiezenia, zapis");
  { bmReset(bm,1200); bmLastMs=millis(); CHECK(PIN_BAT_ADC<0 && !batteryIsEstimate() && !batteryAvailable() && batteryPercent()==-1 && batteryMv()==-1);
    screen=S_MENU; g_fake=1791894900; bmSetVoltage(3.85f); CHECK(bm.valid && batteryIsEstimate() && batteryAvailable() && batteryPercent()==55 && batteryMv()==-1); printf("  3,85 V -> %d%% (szacunek, bez napiecia z czujnika)\n",batteryPercent());
    g_ms+=3600000UL; bmUpdate(); CHECK(near(bm.remMah,660-BM_AWAKE_MA_OFF,0.1f)&&batteryPercent()==52); printf("  po godzinie czuwania: %d%% (%.1f mAh)\n",batteryPercent(),bm.remMah);
    screen=S_UPLOAD; g_ms+=3600000UL; bmUpdate(); screen=S_MENU; CHECK(near(bm.remMah,660-BM_AWAKE_MA_OFF-BM_HOTSPOT_MA,0.2f)); printf("  po godzinie hotspotu: %.1f mAh\n",bm.remMah);
    float r=bm.remMah; uint32_t f0=bm.flipsFull,f1=bm.flipsFast; for(int i=0;i<5;i++) gfxPresent(); for(int i=0;i<20;i++) gfxPresentFast(); bmUpdate(); CHECK(bm.flipsFull==f0+5 && bm.flipsFast==f1+20 && near(bm.remMah,r-5*BM_FLIP_FULL_MAH-20*BM_FLIP_FAST_MAH,0.01f)); printf("  5 pelnych i 20 szybkich odswiezen policzone\n");
    // zapis i odczyt po restarcie
    bmSave(); BatModel keep=bm; bmReset(bm,1200); CHECK(!bm.valid); bmLoad(); CHECK(bm.valid && near(bm.remMah,keep.remMah,0.001f) && bm.flipsFull==keep.flipsFull && near(bm.hotspotS,keep.hotspotS,0.5f));
    // zmiana pojemnosci zachowuje procent
    int pct=batteryPercent(); cfg.battCap=3; bmCapSync(); CHECK(near(bm.capMah,1500) && batteryPercent()==pct); cfg.battCap=2; bmCapSync();
    // wylaczenie szacunku ukrywa wskaznik
    cfg.battEst=false; CHECK(!batteryIsEstimate() && batteryPercent()==-1); cfg.battEst=true;
    // ekrany: tylda przy procentach (rog menu), Info
    goMenu(); int withEst=0; for(int y=0;y<14;y++) for(int x=SCREEN_W-60;x<SCREEN_W-24;x++) if(gfxIsInk(x,y)) withEst++; cfg.battEst=false; goMenu(); int noEst=0; for(int y=0;y<14;y++) for(int x=SCREEN_W-60;x<SCREEN_W-24;x++) if(gfxIsInk(x,y)) noEst++; cfg.battEst=true;
    printf("  rog menu: z szacunkiem %d px, bez %d px\n",withEst,noEst); CHECK(withEst>noEst+20);
    goInfo(); int ov=0; for(int y=34;y<54;y++) for(int x=SCREEN_W-46-1;x<=SCREEN_W-46+3;x++) if(gfxIsInk(x,y)) ov++; CHECK(ov==0); int infoInk=0; for(int y=34;y<54;y++) for(int x=0;x<200;x++) if(gfxIsInk(x,y)) infoInk++; CHECK(infoInk>60); }

  SECTION("T52 czas snu po wybudzeniu, goSleep");
  { cfg.powerSave=0; bmReset(bm,1200); bmSetPercent(bm,60); g_fake=SUMMER; g_slept=false; goSleep(); CHECK(g_slept && prefs.u["bm_slp"]==(uint32_t)SUMMER && prefs.u["bm_ok"]==1);
    float r=bm.remMah; g_fake=SUMMER+7200; g_wakeCause=1; bmBootSleepAccount(); CHECK(near(bm.sleepS,7200,1) && near(bm.remMah,r-BM_SLEEP_MA*2,0.01f) && prefs.u["bm_slp"]==0); printf("  2 h snu doliczone: -%.2f mAh\n",BM_SLEEP_MA*2);
    r=bm.remMah; prefs.u["bm_slp"]=(uint32_t)SUMMER; g_wakeCause=0; bmBootSleepAccount(); CHECK(near(bm.remMah,r,0.0001f) && prefs.u["bm_slp"]==0);        // zimny start: czas snu nieznany
    prefs.u["bm_slp"]=(uint32_t)SUMMER; g_fake=0; g_wakeCause=1; bmBootSleepAccount(); CHECK(near(bm.remMah,r,0.0001f)); printf("  zimny start albo brak zegara: sen nie jest doliczany\n"); g_wakeCause=0; g_fake=SUMMER; }

  SECTION("T53 ustawienia Battery i edytor napiecia");
  { bmReset(bm,1200); cfg.battEst=true; cfg.battCap=2; g_fake=SUMMER; fsOk=true; system("rm -f build/run/fs/battlog.csv");
    CHECK(SET_GROUPS==5 && strcmp(SET_GROUP_NAMES[4],"Battery")==0 && SET_GROUP_ITEMS[4]==5);
    for(int i=0;i<5;i++){ setGroup=4; setSel=i; CHECK(strlen(settingsItem(i))<=(size_t)(UI_CHARS-2)); }
    setTopSel=4; goSettings(); setSel=4; onKeySettings(KeyEvent{KEY_OK,EV_SHORT,0}); CHECK(setGroup==4); setSel=0; CHECK(strstr(settingsItem(0),"Estimate: ON")!=nullptr); setSel=1; CHECK(strstr(settingsItem(1),"1200 mAh")!=nullptr);
    onKeySettings(KeyEvent{KEY_OK,EV_SHORT,0}); CHECK(cfg.battCap==3 && strstr(settingsItem(1),"1500 mAh")!=nullptr && near(bm.capMah,1500)); for(int i=0;i<5;i++) onKeySettings(KeyEvent{KEY_OK,EV_SHORT,0}); CHECK(cfg.battCap==2);
    setSel=2; CHECK(strstr(settingsItem(2),"---")!=nullptr);
    KeyEvent ok{KEY_OK,EV_SHORT,0}, dn{KEY_DOWN,EV_PRESS,0}, up{KEY_UP,EV_PRESS,0}, save{KEY_HOME,EV_SHORT,0}, cancel{KEY_BACK,EV_SHORT,0};
    onKeySettings(ok); CHECK(screen==S_SETBATT && sbCv==390); printf("  edytor otwarty od 3,90 V (brak szacunku)\n");
    onKeySetBatt(dn); CHECK(sbCv==391); onKeySetBatt(up); onKeySetBatt(up); CHECK(sbCv==389); onKeySetBatt(ok); CHECK(sbStep==5); onKeySetBatt(dn); CHECK(sbCv==394); onKeySetBatt(ok); CHECK(sbStep==1);
    for(int i=0;i<200;i++) onKeySetBatt(up); CHECK(sbCv==300); for(int i=0;i<200;i++) onKeySetBatt(dn); CHECK(sbCv==425); printf("  granice 3,00 - 4,25 V\n");
    sbCv=385; drawSetBatt(); CHECK(inkIn(UI_X,24,UI_X+90,48)>150 && inkIn(SCREEN_W-46-1,16,SCREEN_W-46+3,106)==0 && inkIn(SCREEN_W-46,0,SCREEN_W-1,14)>40);   // duze napiecie, nic pod legenda, etykieta SAVE
    onKeySetBatt(save); CHECK(screen==S_SETTINGS && setGroup==4 && setSel==2 && bm.valid && batteryPercent()==55); printf("  zapisane 3,85 V -> %d%%\n",batteryPercent());
    std::string lg=fsRead("battlog.csv"); CHECK(lines(lg)==2 && lg.find("time,volt,pct_from_volt")==0 && lg.find(",voltage\n")!=std::string::npos && lg.find("3.85,55.0,-1.0,55.0")!=std::string::npos); printf("  log: %d wiersze, naglowek + wpis\n",lines(lg));
    // anulowanie nie zmienia szacunku
    int before=batteryPercent(); setGroup=4; setSel=2; onKeySettings(ok); onKeySetBatt(dn); onKeySetBatt(cancel); CHECK(batteryPercent()==before && lines(fsRead("battlog.csv"))==2);
    // drugi wpis: liczniki od poprzedniego ustawienia trafiaja do logu
    g_ms+=1800000UL; bmUpdate(); float est=bmPercent(bm); setGroup=4; setSel=2; onKeySettings(ok); sbCv=380; onKeySetBatt(save); lg=fsRead("battlog.csv"); CHECK(lines(lg)==3 && lg.find("0.500")!=std::string::npos); printf("  drugi wpis: szacunek przed %.1f%%, po %d%%, 0,5 h pracy w logu\n",est,batteryPercent());
    // pelna i wyczysc
    setGroup=4; setSel=3; onKeySettings(ok); CHECK(batteryPercent()==100 && lg.size()<fsRead("battlog.csv").size() && fsRead("battlog.csv").find(",full\n")!=std::string::npos);
    setSel=4; onKeySettings(ok); CHECK(!bm.valid && batteryPercent()==-1 && strstr(settingsItem(2),"---")!=nullptr);
    // rotacja logu: nie rosnie bez konca
    for(int i=0;i<200;i++) bmSetVoltage(3.8f+0.001f*(i%10)); size_t sz=fsRead("battlog.csv").size(); CHECK(sz<14000 && sz>0); printf("  200 wpisow: log ma %zu B (limit ok. 12 KB)\n",sz); }

  SECTION("T54 strona WWW: karta Battery, log CSV, ustawianie napiecia");
  { bmReset(bm,1200); cfg.battEst=true; cfg.battCap=2; g_lastBody.clear(); g_args.clear(); handleConfigGet(); CHECK(g_lastBody.find("<h2>Battery</h2>")!=std::string::npos && g_lastBody.find("name='battest'")!=std::string::npos && g_lastBody.find("name='battcap'")!=std::string::npos && g_lastBody.find("name='bv'")!=std::string::npos && g_lastBody.find("name='bfull'")!=std::string::npos && g_lastBody.find("/battlog.csv")!=std::string::npos && g_lastBody.find("3000 mAh")!=std::string::npos);
    CHECK(g_lastBody.find("not set")!=std::string::npos && g_lastBody.find("name='psave'")!=std::string::npos && g_lastBody.find("80 MHz + light sleep when idle")!=std::string::npos);
    g_args.clear(); g_args["battest"]="on"; g_args["bv"]="3.92"; handleConfigPost(); CHECK(bm.valid && batteryPercent()==(int)(bmPctFromVolt(3.92f)+0.5f) && cfg.battEst); printf("  POST bv=3.92 -> %d%%\n",batteryPercent());
    g_args.clear(); g_args["battest"]="on"; g_args["bv"]="5.5"; int p0=batteryPercent(); handleConfigPost(); CHECK(batteryPercent()==p0); g_args["bv"]="abc"; handleConfigPost(); CHECK(batteryPercent()==p0); g_args["bv"]="2.1"; handleConfigPost(); CHECK(batteryPercent()==p0); printf("  napiecie poza 3,0-4,25 V albo tekst: ignorowane\n");
    g_args.clear(); g_args["battest"]="on"; g_args["bfull"]="on"; handleConfigPost(); CHECK(batteryPercent()==100);
    g_args.clear(); g_args["battest"]="on"; g_args["battcap"]="4"; handleConfigPost(); CHECK(cfg.battCap==4 && near(bm.capMah,2000) && batteryPercent()==100); g_args["battcap"]="9"; handleConfigPost(); CHECK(cfg.battCap==4);
    g_args.clear(); handleConfigPost(); CHECK(!cfg.battEst); cfg.battEst=true;
    g_args.clear(); g_args["battest"]="on"; g_args["psave"]="2"; handleConfigPost(); CHECK(cfg.powerSave==2 && cpuMhzCur==80); g_args["psave"]="7"; handleConfigPost(); CHECK(cfg.powerSave==2); g_args["psave"]="0"; handleConfigPost(); CHECK(cfg.powerSave==0 && cpuMhzCur==240); printf("  Power saving z WWW zmienia taktowanie\n");
    g_lastBody.clear(); g_args.clear(); handleBattLog(); CHECK(g_lastCode==200 && g_lastBody.find("time,volt,pct_from_volt")==0 && lines(g_lastBody)>=2); printf("  /battlog.csv: %d wierszy\n",lines(g_lastBody));
    system("rm -f build/run/fs/battlog.csv"); g_lastBody.clear(); handleBattLog(); CHECK(g_lastBody.find("time,volt")==0 && lines(g_lastBody)==1); cfg.battCap=2; bmCapSync(); }

  SECTION("T55 oszczedzanie energii: taktowanie i lekkie uspienie");
  { std::string bk; for(int i=0;i<200;i++) bk+="Wedrowiec szedl dlugo przez ciemny las. "; writeFile("P.txt",bk);
    cfg=Settings{FAM_CLASSIC,1,false,true,true,false,true,1,false,1,1,0,true,0,0,false,0,false,0,true,0,1,0,true,2}; applyLayout();
    cpuMhzCur=240; g_cpuMhz=240; g_cpuHistoryN=0; screen=S_MENU;
    cfg.powerSave=0; powerApply(); CHECK(cpuMhzCur==240 && g_cpuHistoryN==0);
    cfg.powerSave=1; powerApply(); CHECK(cpuMhzCur==80 && g_cpuMhz==80); screen=S_UPLOAD; powerApply(); CHECK(cpuMhzCur==160); screen=S_MENU; powerApply(); CHECK(cpuMhzCur==80);
    cfg.powerSave=2; goMenu(); CHECK(cpuMhzCur==80); printf("  procesor: 240 MHz (Off), 80 MHz w menu/czytniku, 160 MHz w hotspocie\n");
    g_cpuHistoryN=0; CHECK(openBook("P.txt")); CHECK(g_cpuHistoryN>=2 && g_cpuHistory[0]==240 && g_cpuHistory[g_cpuHistoryN-1]==80 && cpuMhzCur==80); printf("  otwarcie ksiazki: indeksowanie na 240 MHz, potem powrot do 80 MHz\n"); closeBook();
    cfg.powerSave=0; powerApply(); g_cpuHistoryN=0; CHECK(openBook("P.txt") && g_cpuHistoryN==0 && cpuMhzCur==240); closeBook();
    // lekkie uspienie tylko w trybie 2, poza hotspotem i bez wcisnietego przycisku
    g_lightSleeps=0; screen=S_MENU;
    cfg.powerSave=2; idleWait(); CHECK(g_lightSleeps==1); cfg.powerSave=1; idleWait(); CHECK(g_lightSleeps==1); cfg.powerSave=0; idleWait(); CHECK(g_lightSleeps==1);
    cfg.powerSave=2; screen=S_UPLOAD; idleWait(); CHECK(g_lightSleeps==1); printf("  lekkie uspienie: tylko tryb 2; nie w hotspocie\n");
    screen=S_MENU; g_pin[PIN_KEY_OK]=0; idleWait(); CHECK(g_lightSleeps==1); g_pin[PIN_KEY_OK]=1; printf("  wcisniety przycisk blokuje usypianie (brak petli natychmiastowych wybudzen)\n");
    idleWait(); CHECK(g_lightSleeps==2); loop(); CHECK(g_lightSleeps==3); cfg.powerSave=0; loop(); CHECK(g_lightSleeps==3); printf("  petla glowna w bezczynnosci usypia lekko tylko w trybie 2\n");
    // zgodnosc z modelem: stala dla trybu zdefiniowana, TODO w naglowku do uzupelnienia po pomiarach
    CHECK(BM_AWAKE_MA_OFF>20 && BM_HOTSPOT_MA>BM_AWAKE_MA_OFF); screen=S_MENU; cfg.powerSave=0; powerApply(); }
  printf("\n==== TESTY SZACOWANIA BATERII I OSZCZEDZANIA ENERGII: %d OK, %d bledow ====\n",passN,failN); return failN?1:0; }
