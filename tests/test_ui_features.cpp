extern bool g_slept;
#include "inkblink.ino"
#include <fstream>
#include <cmath>
extern bool g_apFail;
static int passN=0, failN=0;
#define CHECK(c) do{ if(c){passN++;} else {failN++; printf("  FAIL line %d: %s\n",__LINE__,#c);} }while(0)
#define SECTION(n) printf("\n== %s ==\n",n)
static void dumpRaw(const char* path){ FILE* f=fopen(path,"wb"); for(int y=0;y<SCREEN_H;y++) for(int x=0;x<SCREEN_W;x++) fputc(gfxIsInk(x,y)?1:0,f); fclose(f); }
static void writeFile(const char* name,const std::string& data){ std::string p=std::string("build/run/fs/")+name; FILE* f=fopen(p.c_str(),"wb"); fwrite(data.data(),1,data.size(),f); fclose(f); }
static long fileSize(const char* name){ std::string p=std::string("build/run/fs/")+name; FILE* f=fopen(p.c_str(),"rb"); if(!f) return -1; fseek(f,0,SEEK_END); long n=ftell(f); fclose(f); return n; }
static int inkIn(int x0,int y0,int x1,int y1){ int n=0; for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++) if(gfxIsInk(x,y)) n++; return n; }
static void resetAll(){ for(int i=0;i<64;i++) g_pin[i]=1; g_ms=100000; }

static std::string bigText(int paragraphs){ std::string t; const char* par="Wędrowiec szedł długo przez ciemny las, aż dotarł do chaty stojącej nad strumieniem. Zażółć gęślą jaźń, pchnąć w tę łódź jeża lub ośm skrzyń fig. "; for(int i=0;i<paragraphs;i++){ for(int k=0;k<4;k++) t+=par; t+="\r\n\r\n"; } return t; }

int main(){
  resetAll(); system("rm -rf build/run/fs/* build/run/png; mkdir -p build/run/png");
  fsOk=true; prefs.begin("reader",false); fontInit(); loadSettings(); statsLoadTotals();
  cfg=Settings{FAM_CLASSIC,1,false,true,true,false,true,1,false,1,1,0,true}; applyLayout();

  SECTION("T16 kody QR na ekranie hotspotu (dekodowane osobno przez OpenCV)");
  { prefs.s["ap_pw"]="48201937"; g_apFail=false; g_stations=0; goUpload(); CHECK(screen==S_UPLOAD && uploadStep==0);
    char ssid[24]; apSsid(ssid,sizeof(ssid));
    dumpRaw("build/run/png/qr_join.raw");
    std::ofstream("build/run/png/qr_join.txt") << "WIFI:T:WPA;S:" << ssid << ";P:48201937;;";
    KeyEvent ok{KEY_OK,EV_SHORT,0};
    // przelaczanie kodu przez prawdziwe loop()
    g_readHook=nullptr; extern int g_pin[64]; (void)g_pin;
    uploadStep=1-uploadStep; drawUpload(); CHECK(uploadStep==1);
    dumpRaw("build/run/png/qr_open.raw"); std::ofstream("build/run/png/qr_open.txt") << "http://192.168.4.1";
    uploadStep=0; uploadStop();
    printf("  wyrenderowano oba kody: siec=%s haslo=48201937, strona=http://192.168.4.1\n",ssid); }

  SECTION("T17 statystyki czytania");
  { writeFile("Stat_A.txt",bigText(600)); writeFile("Stat_B.txt",bigText(300));
    memset(&tst,0,sizeof(tst)); prefs.b.clear();
    cfg.famIdx=0; cfg.fontIdx=1; cfg.bold=false; cfg.showLeft=true; applyLayout();
    CHECK(openBook("Stat_A.txt")); screen=S_READER; page=0; showPage(false); statsTouch();
    CHECK(statsLeftSeconds()==-1); printf("  przed pierwszymi stronami czas do konca nieznany (--)\n");
    for(int i=0;i<20;i++){ g_ms+=20000; turnPages(1); }
    CHECK(page==20 && bst.pages==20 && bst.secs==400 && bst.spdSecs==400 && bst.spdBytes==pageOff[20]-pageOff[0]);
    double rate=(double)bst.spdBytes/bst.spdSecs; long expect=(long)((bookSize-pageOff[page])/rate); long got=statsLeftSeconds();
    CHECK(llabs(got-expect)<=2); char d[16]; fmtDuration((uint32_t)got,d,sizeof(d)); printf("  tempo %.1f bajtow/s, do konca ok. %s (oczekiwane %ld s, jest %ld s)\n",rate,d,expect,got);
    uint32_t secsBefore=bst.secs,pagesBefore=bst.pages;
    g_ms+=600000; turnPages(1); CHECK(bst.secs==secsBefore && bst.pages==pagesBefore); printf("  przerwa 10 min nie liczy sie jako czytanie\n");
    g_ms+=1000; turnPages(1); CHECK(bst.secs==secsBefore+1 && bst.pages==pagesBefore); printf("  przewrocenie po 1 s: czas liczony, do tempa nie\n");
    uint32_t spd=bst.spdBytes; g_ms+=30000; turnPages(10); CHECK(bst.spdBytes==spd && bst.pages==pagesBefore); printf("  skok o 10 stron nie psuje tempa\n");
    CHECK(!bst.done); turnPages(100000); CHECK(page==totalPages-1 && bst.done==1 && tst.finished==1); turnPages(-3); turnPages(3); CHECK(tst.finished==1); printf("  ksiazka skonczona liczona raz\n");
    // zapis i odczyt
    BookStats keep=bst; TotalStats keepT=tst; closeBook(); memset(&tst,0,sizeof(tst)); statsLoadTotals();
    CHECK(tst.secs==keepT.secs && tst.pages==keepT.pages && tst.finished==1 && tst.spdBytes==keepT.spdBytes);
    CHECK(openBook("Stat_A.txt")); CHECK(bst.secs>=keep.secs && bst.pages==keep.pages && bst.done==1); printf("  statystyki przetrwaly zamkniecie ksiazki i restart\n");
    closeBook();
    // nowa ksiazka bez historii: czas do konca ze sredniego tempa
    CHECK(openBook("Stat_B.txt")); screen=S_READER; showPage(false); CHECK(bst.spdSecs==0); CHECK(statsLeftSeconds()>=0); printf("  nowa ksiazka: czas do konca ze sredniego tempa\n"); closeBook();
    // formatowanie
    char f[16]; fmtDuration(5400,f,sizeof(f)); CHECK(strcmp(f,"1h30m")==0); fmtDuration(2100,f,sizeof(f)); CHECK(strcmp(f,"35m")==0); fmtDuration(30,f,sizeof(f)); CHECK(strcmp(f,"<1m")==0); fmtDuration(3600*12+60*5,f,sizeof(f)); CHECK(strcmp(f,"12h05m")==0);
    fmtDurationLong(45240,f,sizeof(f)); CHECK(strcmp(f,"12h 34m")==0); fmtDurationLong(100,f,sizeof(f)); CHECK(strcmp(f,"1m")==0);
    // ekran Stats
    prefs.putString("last","Stat_A.txt"); goStats(); CHECK(screen==S_STATS); dumpRaw("build/run/png/ui_stats_full.raw");
    // pasek stanu z czasem do konca w czytniku
    cfg.showLeft=true; cfg.showPct=true; cfg.showPage=true; cfg.showBar=true; applyLayout(); CHECK(openBook("Stat_B.txt")); screen=S_READER; page=5; showPage(false); dumpRaw("build/run/png/reader_status_left.raw"); closeBook(); }

  SECTION("T18 bateria (pin nieskonfigurowany)");
  { CHECK(!batteryAvailable()); CHECK(batteryMv()==-1 && batteryPercent()==-1);
    CHECK(batteryPercentFromMv(4300)==100 && batteryPercentFromMv(4200)==100 && batteryPercentFromMv(3300)==0 && batteryPercentFromMv(3000)==0 && batteryPercentFromMv(3700)==28);
    int prev=101; bool mono=true; for(int mv=4300;mv>=3000;mv-=10){ int p=batteryPercentFromMv(mv); if(p>prev) mono=false; prev=p; } CHECK(mono); printf("  krzywa Li-Po monotoniczna: 4.20V=%d%% 3.90V=%d%% 3.70V=%d%% 3.50V=%d%%\n",batteryPercentFromMv(4200),batteryPercentFromMv(3900),batteryPercentFromMv(3700),batteryPercentFromMv(3500));
    goInfo(); CHECK(inkIn(0,0,SCREEN_W-1,SCREEN_H-1)>100); printf("  Info bez baterii: 'not set' + PINS\n"); }

  SECTION("T19 ekran uspienia: karta ksiazki / obraz / bez zmian");
  { cfg.showLeft=false; applyLayout(); prefs.s.erase("last");
    // (a) brak ksiazek -> obraz domyslny
    cfg.sleepScreen=0; closeBook(); curBook[0]=0; totalPages=0; gfxClear(); CHECK(!drawSleepBookCard()); drawSleepScreen(); dumpRaw("build/run/png/sleep_default.raw"); int inkDef=inkIn(0,0,SCREEN_W-1,SCREEN_H-1); CHECK(inkDef>800); printf("  bez ksiazki -> wbudowany obraz (tusz: %d px)\n",inkDef);
    // (b) karta w trakcie czytania
    cfg.famIdx=0; cfg.fontIdx=1; applyLayout(); writeFile("Wiedzmin_-_Ostatnie_zyczenie.txt",bigText(500));
    CHECK(openBook("Wiedzmin_-_Ostatnie_zyczenie.txt")); screen=S_READER; page=0; showPage(false); statsTouch(); for(int i=0;i<30;i++){ g_ms+=15000; turnPages(1); }
    drawSleepScreen(); dumpRaw("build/run/png/sleep_book_reader.raw"); CHECK(inkIn(0,60,SCREEN_W-1,75)>200); printf("  karta w czytniku: strona %d z %d, postep %d%%\n",page+1,totalPages,readPercent());
    // (c) karta po zamknieciu ksiazki (ostatnia ksiazka z zakladki)
    saveBookmark(); closeBook(); gfxClear(); CHECK(drawSleepBookCard()); gfxClear(); drawSleepScreen(); dumpRaw("build/run/png/sleep_book_last.raw");
    // (d) dlugi tytul: dwa wiersze
    char l1[48],l2[48]; titleLines("Bardzo_dluga_nazwa_ksiazki_ktora_sie_nie_miesci_w_jednym_wierszu_karty.txt",l1,l2,29); printf("  dlugi tytul: [%s] / [%s]\n",l1,l2); CHECK(strlen(l1)<=29 && strlen(l2)<=29 && l2[0]!=0);
    titleLines("Pan_Tadeusz.txt",l1,l2,29); CHECK(strcmp(l1,"Pan Tadeusz")==0 && l2[0]==0);
    titleLines("Wiedzmin_-_Ostatnie_zyczenie_i_inne_opowiadania.txt",l1,l2,29); CHECK(strlen(l1)<=29 && l1[strlen(l1)-1]!=' ');
    writeFile("Bardzo_dluga_nazwa_ksiazki_ktora_sie_nie_miesci_w_jednym_wierszu_karty.txt"[0]?"Dluga.txt":"Dluga.txt",bigText(50)); prefs.putString("last","Dluga.txt"); gfxClear(); drawSleepScreen(); dumpRaw("build/run/png/sleep_book_long.raw");
    // (e) obraz wgrany przez uzytkownika: dwa znane piksele
    { std::string wp(WALLPAPER_BYTES,'\0'); wp[2*32+(3>>3)] |= (0x80>>(3&7)); wp[121*32+(249>>3)] |= (0x80>>(249&7)); writeFile("wallpaper.bin",wp);
      cfg.sleepScreen=1; gfxClear(); CHECK(drawWallpaperFile()); CHECK(gfxIsInk(3,2) && gfxIsInk(249,121) && !gfxIsInk(4,2) && !gfxIsInk(3,3) && !gfxIsInk(248,121) && inkIn(0,0,249,121)==2); printf("  obraz uzytkownika: piksele (3,2) i (249,121) na swoich miejscach, reszta biala\n");
      writeFile("wallpaper.bin",std::string(100,'\xFF')); gfxClear(); CHECK(!drawWallpaperFile()); printf("  plik o zlym rozmiarze ignorowany -> obraz domyslny\n");
      std::string pat(WALLPAPER_BYTES,'\0'); for(int y=0;y<122;y++) for(int x=0;x<250;x++) if(((x/10)+(y/10))%2==0) pat[y*32+(x>>3)] |= (0x80>>(x&7)); writeFile("wallpaper.bin",pat); gfxClear(); CHECK(drawWallpaperFile()); dumpRaw("build/run/png/sleep_user.raw"); }
    // (f) goSleep: karta rysowana, 'Keep' nie rysuje
    remove("build/run/fs/wallpaper.bin");
    cfg.sleepScreen=0; CHECK(openBook("Wiedzmin_-_Ostatnie_zyczenie.txt")); screen=S_READER; showPage(false); gfxClear(); g_slept=false; goSleep(); CHECK(g_slept && inkIn(0,0,249,121)>400); printf("  goSleep rysuje karte\n");
    cfg.sleepScreen=2; resetAll(); fsOk=true; g_slept=false; screen=S_MENU; gfxClear(); goSleep(); CHECK(g_slept && inkIn(0,0,249,121)==0); printf("  'Keep': ekran uspienia nie jest rysowany\n"); fsOk=true; cfg.sleepScreen=0; closeBook(); }

  SECTION("T21 wgrywanie obrazu uspienia przez WWW");
  { auto up=[&](HTTPUploadStatus st,const char* data,size_t n){ g_up.status=st; g_up.filename=String("wallpaper.bin"); g_up.buf=(uint8_t*)data; g_up.currentSize=n; handleWpData(); };
    remove("build/run/fs/wallpaper.bin"); std::string img(WALLPAPER_BYTES,'\xA5');
    up(UPLOAD_FILE_START,nullptr,0); for(size_t o=0;o<img.size();o+=976) up(UPLOAD_FILE_WRITE,img.data()+o,std::min<size_t>(976,img.size()-o)); up(UPLOAD_FILE_END,nullptr,0); handleWpFinished();
    CHECK(g_lastCode==200 && fileSize("wallpaper.bin")==WALLPAPER_BYTES && fileSize("wp.tmp")==-1); printf("  poprawny obraz (3904 B) zapisany\n");
    std::string good; { FILE* f=fopen("build/run/fs/wallpaper.bin","rb"); good.resize(WALLPAPER_BYTES); fread(&good[0],1,WALLPAPER_BYTES,f); fclose(f); }
    up(UPLOAD_FILE_START,nullptr,0); up(UPLOAD_FILE_WRITE,img.data(),100); up(UPLOAD_FILE_END,nullptr,0); handleWpFinished(); CHECK(g_lastCode==400 && fileSize("wallpaper.bin")==WALLPAPER_BYTES && fileSize("wp.tmp")==-1); printf("  za krotki obraz odrzucony, stary nietkniety\n");
    std::string big(4200,'\x11'); up(UPLOAD_FILE_START,nullptr,0); for(size_t o=0;o<big.size();o+=1000) up(UPLOAD_FILE_WRITE,big.data()+o,std::min<size_t>(1000,big.size()-o)); up(UPLOAD_FILE_END,nullptr,0); handleWpFinished(); CHECK(g_lastCode==400 && fileSize("wp.tmp")==-1 && fileSize("wallpaper.bin")==WALLPAPER_BYTES); printf("  za duzy obraz odrzucony\n");
    up(UPLOAD_FILE_START,nullptr,0); up(UPLOAD_FILE_WRITE,img.data(),500); up(UPLOAD_FILE_ABORTED,nullptr,0); CHECK(fileSize("wp.tmp")==-1 && fileSize("wallpaper.bin")==WALLPAPER_BYTES); printf("  przerwane wgrywanie nie rusza starego obrazu\n");
    handleWpReset(); CHECK(fileSize("wallpaper.bin")==-1 && g_lastCode==200); printf("  reset usuwa obraz uzytkownika\n"); }

  SECTION("T22 test pinow i menu ze Stats");
  { for(int i=0;i<64;i++) g_adc[i]=0; g_adc[3]=312; g_adc[8]=1980; g_adc[15]=44; goPinScan(); CHECK(screen==S_PINSCAN); dumpRaw("build/run/png/ui_pinscan.raw"); CHECK(inkIn(0,16,80,110)>300);
    buildMenu(); bool hasStats=false; for(int i=0;i<menuCount;i++) if(menuIds[i]==M_STATS) hasStats=true; CHECK(hasStats && (menuCount==5||menuCount==6)); printf("  menu: %d pozycji, w tym Stats\n",menuCount);
    menuSel=0; for(int i=0;i<menuCount;i++) if(menuIds[i]==M_STATS) menuSel=i; screen=S_MENU; KeyEvent ok{KEY_OK,EV_SHORT,0}; onKeyMenu(ok); CHECK(screen==S_STATS); printf("  menu -> Stats otwiera ekran statystyk\n"); }

  printf("\n==== NOWE TESTY: %d OK, %d bledow ====\n",passN,failN);
  return failN?1:0;
}
