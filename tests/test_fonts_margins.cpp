#include "inkblink.ino"
extern bool g_apFail;
static int passN=0, failN=0;
#define CHECK(c) do{ if(c){passN++;} else {failN++; printf("  FAIL line %d: %s\n",__LINE__,#c);} }while(0)
#define SECTION(n) printf("\n== %s ==\n",n)
static void dumpRaw(const char* path){ FILE* f=fopen(path,"wb"); for(int y=0;y<SCREEN_H;y++) for(int x=0;x<SCREEN_W;x++) fputc(gfxIsInk(x,y)?1:0,f); fclose(f); }
static void writeFile(const char* name,const std::string& d){ std::string p=std::string("build/run/fs/")+name; FILE* f=fopen(p.c_str(),"wb"); fwrite(d.data(),1,d.size(),f); fclose(f); }
static int inkIn(int x0,int y0,int x1,int y1){ int n=0; for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++) if(gfxIsInk(x,y)) n++; return n; }
int main(){
  for(int i=0;i<64;i++) g_pin[i]=1; g_ms=100000; system("rm -rf build/run/fs/*; mkdir -p build/run/png");
  fsOk=true; prefs.begin("reader",false); fontInit(); loadSettings(); statsLoadTotals();
  std::string t; for(int p=0;p<300;p++){ t+="Wędrowiec szedł długo przez ciemny las, aż dotarł do chaty stojącej nad strumieniem. Zażółć gęślą jaźń, pchnąć w tę łódź jeża lub ośm skrzyń fig.\r\n\r\n"; } writeFile("M.txt",t);

  SECTION("T24 rozmiary Classic, pogrubienie Unifont, marginesy");
  { CHECK(fontSizeCount(FAM_CLASSIC)==2 && fontSizeValue(FAM_CLASSIC,0)==12 && fontSizeValue(FAM_CLASSIC,1)==16 && fontSizeCount(FAM_HELV)==4 && fontSizeCount(FAM_CENTURY)==4 && fontSizeCount(FAM_UNIFONT)==1); printf("  Classic: 12 i 16 px; Helvetica i Century 10/12/14/18; Unifont 16\n");
    // zapisane dawne indeksy (24 px = 2, 32 px = 3) -> najwiekszy dostepny
    cfg.famIdx=0; cfg.fontIdx=3; applyLayout(); CHECK(cfg.fontIdx==1 && fontLineH()==16); cfg.fontIdx=2; applyLayout(); CHECK(cfg.fontIdx==1); printf("  stary zapis 24/32 px przycinany do 16 px\n");
    // Unifont: pogrubienie dziala (wiecej tuszu, szersza tabela)
    auto inkOf=[&](bool bold){ cfg=Settings{FAM_UNIFONT,0,bold,false,false,false,false,1,false,1,1,0,true,0}; applyLayout(); openBook("M.txt"); page=3; screen=S_READER; showPage(false); int n=inkIn(0,0,249,121); closeBook(); return n; };
    int regular=inkOf(false), bold=inkOf(true); printf("  Unifont: zwykly %d px tuszu, pogrubiony %d px\n",regular,bold); CHECK(bold>regular*12/10);
    cfg=Settings{FAM_UNIFONT,0,true,false,false,false,false,1,false,1,1,0,true,0}; applyLayout(); CHECK(fSynthBold && fontCharW('a')==fontCharW('i')+0 && fontCharW('a')==9);   // 8 + 1 px
    cfg=Settings{FAM_UNIFONT,0,false,false,false,false,false,1,false,1,1,0,true,0}; applyLayout(); CHECK(!fSynthBold && fontCharW('a')==8); printf("  szerokosc znaku Unifont: 8 px, pogrubiony 9 px\n");
    // marginesy
    int w[3], x0[3], pages[3];
    for(int m=0;m<3;m++){ cfg=Settings{FAM_HELV,1,false,true,true,false,true,1,false,1,1,0,true,(uint8_t)m}; applyLayout(); w[m]=maxTextW; x0[m]=textX0; openBook("M.txt"); pages[m]=totalPages; closeBook(); }
    printf("  marginesy Normal/Narrow/Wide: szerokosc tekstu %d/%d/%d px, x0 %d/%d/%d, stron %d/%d/%d\n",w[0],w[1],w[2],x0[0],x0[1],x0[2],pages[0],pages[1],pages[2]);
    CHECK(w[0]==234 && w[1]==242 && w[2]==222 && x0[0]==8 && x0[1]==4 && x0[2]==14); CHECK(pages[1]<=pages[0] && pages[0]<=pages[2]);
    cfg=Settings{FAM_CLASSIC,1,false,true,true,false,true,1,false,1,1,0,true,0}; applyLayout(); CHECK(maxTextW==232 && textX0==9); printf("  Classic 16 px: %d znakow w wierszu, centrowanie x0=%d\n",maxTextW/fontFixedW(),textX0);
    // zmiana marginesu daje inny indeks (inny podpis ukladu)
    uint32_t s0=layoutSig(); cfg.margin=2; applyLayout(); CHECK(layoutSig()!=s0); }

  SECTION("T25 czas do konca: placeholder '~--' i nizsze progi");
  { cfg=Settings{FAM_CLASSIC,1,false,true,true,true,false,1,false,1,1,0,true,0}; applyLayout(); memset(&tst,0,sizeof(tst)); prefs.b.clear();
    CHECK(openBook("M.txt")); screen=S_READER; page=0; showPage(false); statsTouch();
    CHECK(statsLeftSeconds()==-1); int inkOff,inkOn;
    cfg.showLeft=false; applyLayout(); page=1; showPage(false); inkOff=inkIn(0,STATUS_Y,200,121);
    cfg.showLeft=true; applyLayout(); showPage(false); inkOn=inkIn(0,STATUS_Y,200,121); printf("  pasek stanu: bez 'time left' %d px, z '~--' %d px\n",inkOff,inkOn); CHECK(inkOn>inkOff);
    dumpRaw("build/run/png/status_placeholder.raw");
    // po kilku stronach (ok. 50 s) jest szacunek
    page=0; showPage(false); statsTouch(); for(int i=0;i<5;i++){ g_ms+=10000; turnPages(1); }
    CHECK(bst.spdSecs==50 && statsLeftSeconds()>=0); printf("  po 5 stronach i 50 s: szacunek jest (%ld s)\n",statsLeftSeconds());
    showPage(false); dumpRaw("build/run/png/status_estimate.raw"); closeBook(); }

  SECTION("T26 hotspot: odstep kodu QR od tekstu i ramki");
  { prefs.s["ap_pw"]="48201937"; g_apFail=false; goUpload();
    auto check=[&](int step){ uploadStep=step; drawUpload(); static QrCode q; char ssid[24]; apSsid(ssid,sizeof(ssid)); char pl[80]; if(step==0) snprintf(pl,sizeof(pl),"WIFI:T:WPA;S:%s;P:48201937;;",ssid); else snprintf(pl,sizeof(pl),"http://192.168.4.1"); qrEncode(pl,q); int h=q.size*QR_SCALE; int y0=QR_Y(h), x1=QR_X+h-1, y1=y0+h-1;
      // bounding box kodu rzeczywiscie ma tusz na krawedziach (modul znacznika) i nic wokol w odstepie 4 modulow (12 px) poza tytulem
      CHECK(inkIn(QR_X,y0,QR_X+8,y0+8)>20);
      int left=inkIn(QR_X-12,y0-12,QR_X-1,y1+12), right=inkIn(x1+1,y0-12,x1+12,y1+12), bottom=inkIn(QR_X-12,y1+1,x1+12,y1+12), top=inkIn(QR_X-12,y0-12,x1+12,y0-1);
      printf("  krok %d: kod %dx%d px w (%d,%d); tusz w marginesach (12 px = 4 moduly): lewy=%d prawy=%d gorny=%d dolny=%d\n",step,h,h,QR_X,y0,left,right,top,bottom);
      CHECK(left==0 && right==0 && top==0 && bottom==0);
      // bity formatu (oba egzemplarze) musza byc poprawnym slowem BCH dla poziomu M - wczesniej makro bit() z Arduino psulo je na urzadzeniu
      int n=q.size, fa=0, fb=0;
      for(int i=0;i<=5;i++) fa|=qrDark(q,8,i)<<i; fa|=qrDark(q,8,7)<<6; fa|=qrDark(q,8,8)<<7; fa|=qrDark(q,7,8)<<8; for(int i=9;i<15;i++) fa|=qrDark(q,14-i,8)<<i;
      for(int i=0;i<8;i++) fb|=qrDark(q,n-1-i,8)<<i; for(int i=8;i<15;i++) fb|=qrDark(q,8,n-15+i)<<i;
      int raw=fa^0x5412, rem=raw>>10; for(int i=0;i<10;i++) rem=(rem<<1)^((rem>>9)*0x537);
      CHECK(fa==fb && ((raw>>10)<<10|rem)==raw && (raw>>13)==0); printf("  krok %d: bity formatu poprawne (poziom M, maska %d)\n",step,(raw>>10)&7); };
    check(0); check(1); uploadStep=0; uploadStop(); }

  SECTION("T27 test pinow: gwiazdka przy pinie w zakresie baterii");
  { for(int i=0;i<64;i++) g_adc[i]=0; goPinScan(); int a=inkIn(0,16,84,115); g_adc[8]=1980; goPinScan(); int b=inkIn(0,16,84,115); printf("  tusz bez/z pinem 1980 mV: %d / %d px (jest '*')\n",a,b); CHECK(b>a+5); }


  SECTION("T28 hotspot: linijka 'Got:' nie nachodzi na haslo");
  { prefs.s["ap_pw"]="48201937"; g_apFail=false; goUpload();
    for(int step=0;step<2;step++){ uploadStep=step; strcpy(lastUploaded,"Wiedzmin_-_Ostatnie_zyczenie.txt"); drawUpload(); char fn[40]; snprintf(fn,sizeof(fn),"build/run/png/upload_got%d.raw",step); dumpRaw(fn);
      int tx=(step==0)?117:105; int gap=inkIn(tx,95,200,96), got=inkIn(tx,97,200,108), stat=inkIn(tx,109,200,121), over=inkIn(204,16,249,107);
      printf("  krok %d: odstep nad 'Got' %d px tuszu, 'Got' %d px, status %d px, tusz w kolumnie legendy (poza etykietami) %d\n",step,gap,got,stat,over-inkIn(204,54,249,67));
      CHECK(gap==0 && got>30 && stat>30); }
    lastUploaded[0]=0; uploadStep=0; uploadStop(); }

  printf("\n==== TESTY POPRAWEK: %d OK, %d bledow ====\n",passN,failN); return failN?1:0; }
