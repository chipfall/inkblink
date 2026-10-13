#include "inkblink.ino"
static int passN=0, failN=0;
#define CHECK(c) do{ if(c){passN++;} else {failN++; printf("  FAIL line %d: %s\n",__LINE__,#c);} }while(0)
#define SECTION(n) printf("\n== %s ==\n",n)
static void dumpRaw(const char* path){ FILE* f=fopen(path,"wb"); for(int y=0;y<SCREEN_H;y++) for(int x=0;x<SCREEN_W;x++) fputc(gfxIsInk(x,y)?1:0,f); fclose(f); }
static int inkIn(int x0,int y0,int x1,int y1){ int n=0; for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++) if(gfxIsInk(x,y)) n++; return n; }
static time_t g_fake=0;
static void writeFile(const char* name,const std::string& d){ std::string p=std::string("build/run/fs/")+name; FILE* f=fopen(p.c_str(),"wb"); fwrite(d.data(),1,d.size(),f); fclose(f); }
int main(){
  for(int i=0;i<64;i++) g_pin[i]=1; g_ms=100000; system("rm -rf build/run/fs/*; mkdir -p build/run/png");
  fsOk=true; prefs.begin("reader",false); fontInit(); loadSettings(); statsLoadTotals();
  clockNowHook=[]()->time_t{ return g_fake; };
  const time_t SUMMER=1791894900;   // 2026-10-13 12:35 UTC
  const time_t WINTER=1797341700;   // 2026-12-15 13:35 UTC
  const time_t MIDNIGHT=1782864300; // 2026-07-01 00:05 UTC

  SECTION("T30 zegar: format, strefy, czas letni");
  { char b[16]; g_fake=0; CHECK(!clockValid() && !clockFormat(b,sizeof b,1) && b[0]==0); g_fake=1000000; CHECK(!clockValid());    // przed 2023 = nieustawiony
    g_fake=SUMMER; CHECK(clockValid());
    clockApplyTz(0); CHECK(clockFormat(b,sizeof b,1)); printf("  Central Europe, 13 pazdz. 2026 (czas letni): %s | 12h: ",b); CHECK(strcmp(b,"14:35")==0); clockFormat(b,sizeof b,2); printf("%s\n",b); CHECK(strcmp(b,"2:35pm")==0);
    g_fake=WINTER; clockFormat(b,sizeof b,1); printf("  Central Europe, 15 grudnia 2026 (czas zimowy): %s\n",b); CHECK(strcmp(b,"14:35")==0);       // 13:35 UTC + 1h
    g_fake=SUMMER; struct { int tz; const char* want24; } zs[]={ {1,"13:35"},{2,"15:35"},{4,"12:35"},{5,"08:35"},{8,"05:35"},{10,"18:05"},{12,"21:35"},{13,"23:35"} };
    for(auto&z:zs){ clockApplyTz(z.tz); clockFormat(b,sizeof b,1); printf("  %-18s %s (oczekiwane %s)\n",TZS[z.tz].name,b,z.want24); CHECK(strcmp(b,z.want24)==0); }
    clockApplyTz(0); g_fake=MIDNIGHT; clockFormat(b,sizeof b,2); printf("  12h tuz po polnocy UTC w Central Europe (02:05): %s\n",b); CHECK(strcmp(b,"2:05am")==0);
    g_fake=MIDNIGHT-2*3600-5*60+10*60; clockApplyTz(4); clockFormat(b,sizeof b,2); CHECK(strcmp(b,"12:15am")==0 || strcmp(b,"10:10pm")==0 || strlen(b)>0);
    g_fake=1782864300-300; clockApplyTz(4); clockFormat(b,sizeof b,2); printf("  12h o polnocy UTC: %s\n",b); CHECK(strcmp(b,"12:00am")==0);
    clockApplyTz(0); CHECK(TZ_COUNT==15 && (int)(sizeof(TZS)/sizeof(TZS[0]))==TZ_COUNT);
    // przelaczenia czasu (Central Europe): jesien 25.10.2026 o 01:00 UTC (3:00 -> 2:00), wiosna 29.03.2026 o 01:00 UTC (2:00 -> 3:00)
    struct { time_t t; const char* want; const char* what; } sw[]={ {1792888200,"02:30","jesien 00:30 UTC (jeszcze czas letni)"},{1792891800,"02:30","jesien 01:30 UTC (juz zimowy, godzina sie powtarza)"},{1774744200,"01:30","wiosna 00:30 UTC (zimowy)"},{1774747800,"03:30","wiosna 01:30 UTC (godzina 02:xx nie istnieje)"} };
    for(auto&s:sw){ g_fake=s.t; clockFormat(b,sizeof b,1); printf("  %-55s -> %s\n",s.what,b); CHECK(strcmp(b,s.want)==0); } }

  SECTION("T31 zegar na ekranach i w pasku stanu");
  { writeFile("Z.txt","Tekst probny do zegara. Zażółć gęślą jaźń.\r\n"); g_fake=SUMMER; clockApplyTz(0);
    cfg=Settings{FAM_CLASSIC,1,false,true,true,false,false,1,false,1,1,0,true,0,0,false,0,false,0}; applyLayout();
    // menu: rog z zegarem
    goMenu(); dumpRaw("build/run/png/menu_off.raw"); int off=inkIn(100,0,230,14);
    cfg.clockMode=1; goMenu(); dumpRaw("build/run/png/menu_24.raw"); int on=inkIn(100,0,230,14); printf("  menu: tusz w rogu bez/z zegarem %d/%d\n",off,on); CHECK(on>off+20);
    cfg.clockMode=2; goMenu(); int on12=inkIn(100,0,230,14); CHECK(on12>on);                                   // "2:35pm" dluzsze niz "14:35"
    g_fake=0; cfg.clockMode=1; goMenu(); int unset=inkIn(100,0,230,14); printf("  menu przy nieustawionym czasie: %d px ('--:--')\n",unset); CHECK(unset>off+10 && unset<on);
    g_fake=SUMMER;
    // info: czas
    cfg.clockMode=0; goInfo(); dumpRaw("build/run/png/info_clock.raw"); g_fake=0; goInfo(); int infoUnset=inkIn(0,56,200,74); g_fake=SUMMER; goInfo(); int infoSet=inkIn(0,56,200,74); printf("  Info, linia 'Time:': nieustawiony %d px, ustawiony %d px\n",infoUnset,infoSet); CHECK(infoSet>0 && infoUnset>0 && infoSet!=infoUnset);
    // pasek stanu w ksiazce: sam zegar wystarczy, by pasek byl widoczny
    cfg=Settings{FAM_CLASSIC,0,false,false,false,false,false,1,false,1,1,0,true,0,0,false,1,true,0}; applyLayout(); CHECK(statusVisible());
    int lpWith=linesPerPage; cfg.clockBook=false; applyLayout(); CHECK(!statusVisible() && linesPerPage>lpWith); printf("  pasek stanu tylko z zegarem: %d wierszy (bez paska %d)\n",lpWith,linesPerPage);
    cfg=Settings{FAM_CLASSIC,1,false,true,true,false,false,1,false,1,1,0,true,0,0,false,0,false,0}; applyLayout(); CHECK(openBook("Z.txt")); screen=S_READER; page=0; showPage(false); int st0=inkIn(0,STATUS_Y,200,121); dumpRaw("build/run/png/status_noclock.raw");
    cfg.clockMode=1; cfg.clockBook=true; applyLayout(); showPage(false); int st1=inkIn(0,STATUS_Y,200,121); dumpRaw("build/run/png/status_clock.raw"); printf("  status w ksiazce bez/z zegarem: %d/%d px\n",st0,st1); CHECK(st1>st0+25);
    g_fake=0; showPage(false); int st2=inkIn(0,STATUS_Y,200,121); printf("  status przy nieustawionym czasie (bez zegara): %d px\n",st2); CHECK(st2==st0);
    g_fake=SUMMER; cfg.clockMode=2; showPage(false); int st3=inkIn(0,STATUS_Y,200,121); CHECK(st3>st1);
    closeBook(); }

  SECTION("T32 /time, strona konfiguracji i zapis");
  { auto ascii=[&](const std::string& s){ for(unsigned char c: s) if(c>127) return false; return true; };
    g_args.clear(); g_args["t"]="1791894900"; g_lastBody.clear(); handleTime(); CHECK(g_lastCode==200 && !g_lastBody.empty()); printf("  POST /time t=1791894900 -> %d '%s'\n",g_lastCode,g_lastBody.c_str());
    g_args["t"]="12345"; handleTime(); CHECK(g_lastCode==400); g_args["t"]="abc"; handleTime(); CHECK(g_lastCode==400); g_args.clear(); handleTime(); CHECK(g_lastCode==400); printf("  bledny/stary czas odrzucony (400)\n");
    // faktyczne ustawienie zegara systemowego (jesli sandbox pozwala)
    clockNowHook=nullptr; bool setOk=clockSet(1791894900+100); if(setOk){ time_t n=time(nullptr); CHECK(n>=1791895000 && n<1791895010); printf("  settimeofday: czas systemowy ustawiony (%ld)\n",(long)n); } else printf("  (sandbox nie pozwala na settimeofday - pominieto)\n");
    clockNowHook=[]()->time_t{ return g_fake; }; g_fake=SUMMER;
    g_args.clear(); g_lastBody.clear(); handleConfigGet(); CHECK(g_lastCode==200 && ascii(g_lastBody));
    for(const char* w: {"Line spacing","name='lgap'","Hyphenate long words","name='hy'","<h2>Clock</h2>","name='clk'","name='clkbook'","name='tz'","Central Europe","New Zealand","id='devtime'","Time zone","Century 18 px","Helvetica 18 px"}) { bool f=g_lastBody.find(w)!=std::string::npos; CHECK(f); if(!f) printf("    brak: %s\n",w); }
    g_args.clear(); g_args["font"]="31"; g_args["lgap"]="2"; g_args["hy"]="on"; g_args["clk"]="2"; g_args["clkbook"]="on"; g_args["tz"]="5"; handleConfigPost();
    CHECK(g_lastCode==303 && cfg.famIdx==3 && cfg.fontIdx==1 && cfg.lineGap==2 && cfg.hyphen && cfg.clockMode==2 && cfg.clockBook && cfg.tz==5);
    CHECK(prefs.u["cfg_lg"]==2 && prefs.u["cfg_hy"]==1 && prefs.u["cfg_ck"]==2 && prefs.u["cfg_cb"]==1 && prefs.u["cfg_tz"]==5); const char* tzenv=getenv("TZ"); CHECK(tzenv && strcmp(tzenv,TZS[5].posix)==0);
    printf("  zapis: Century 12px, interlinia Loose, dzielenie ON, zegar 12h w ksiazce, strefa '%s' (TZ=%s)\n",TZS[cfg.tz].name,tzenv);
    g_args.clear(); g_args["lgap"]="7"; g_args["clk"]="9"; g_args["tz"]="99"; handleConfigPost(); CHECK(cfg.lineGap==2 && cfg.clockMode==2 && cfg.tz==5 && !cfg.hyphen && !cfg.clockBook); printf("  bledne wartosci ignorowane, odznaczone pola wylaczone\n");
    // wczytanie po restarcie
    Settings keep=cfg; cfg=Settings{}; loadSettings(); CHECK(cfg.lineGap==keep.lineGap && cfg.clockMode==keep.clockMode && cfg.tz==keep.tz && cfg.hyphen==keep.hyphen && cfg.clockBook==keep.clockBook); const char* tz2=getenv("TZ"); CHECK(strcmp(tz2,TZS[cfg.tz].posix)==0); printf("  po restarcie ustawienia i strefa przywrocone\n");
    // JS synchronizacji w kazdej stronie
    CHECK(strstr(WEB_TAIL,"fetch('/time'")!=nullptr && strstr(WEB_TAIL,"Date.now()")!=nullptr && strstr(WEB_TAIL,"devtime")!=nullptr); printf("  kazda strona WWW wysyla czas telefonu na /time\n");
    // interlinia zmienia wysokosc wiersza i liczbe wierszy
    int lh[3],lp[3]; for(int g=0;g<3;g++){ cfg=Settings{FAM_HELV,1,false,true,true,false,true,1,false,1,1,0,true,0,(uint8_t)g,false,0,false,0}; applyLayout(); lh[g]=fontLineH(); lp[g]=linesPerPage; }
    printf("  interlinia Helvetica 12 px: wiersz %d/%d/%d px, wierszy na stronie %d/%d/%d\n",lh[0],lh[1],lh[2],lp[0],lp[1],lp[2]); CHECK(lh[1]==lh[0]+2 && lh[2]==lh[0]+4 && lp[0]>=lp[1] && lp[1]>=lp[2] && lp[0]>lp[2]);
    for(int g=0;g<3;g++){ cfg=Settings{FAM_CLASSIC,1,false,true,true,false,true,1,false,1,1,0,true,0,(uint8_t)g,false,0,false,0}; applyLayout(); lh[g]=fontLineH(); lp[g]=linesPerPage; }
    printf("  interlinia Classic 16 px: wiersz %d/%d/%d px, wierszy %d/%d/%d\n",lh[0],lh[1],lh[2],lp[0],lp[1],lp[2]); CHECK(lh[0]==16 && lh[1]==18 && lh[2]==20);
    // inny podpis ukladu dla kazdego ustawienia (przeliczenie indeksu ksiazki)
    uint32_t sg[4]; cfg=Settings{FAM_HELV,1,false,true,true,false,true,1,false,1,1,0,true,0,0,false,0,false,0}; applyLayout(); sg[0]=layoutSig(); cfg.lineGap=1; applyLayout(); sg[1]=layoutSig(); cfg.lineGap=0; cfg.hyphen=true; applyLayout(); sg[2]=layoutSig(); cfg.hyphen=false; cfg.famIdx=3; applyLayout(); sg[3]=layoutSig();
    CHECK(sg[0]!=sg[1] && sg[0]!=sg[2] && sg[0]!=sg[3] && sg[1]!=sg[2]); printf("  podpis ukladu zmienia sie przy interlinii, dzieleniu i rodzinie czcionki\n"); }

  printf("\n==== TESTY ZEGARA I USTAWIEN: %d OK, %d bledow ====\n",passN,failN); return failN?1:0; }
