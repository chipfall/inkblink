extern bool g_apFail;
extern int g_apFailFirst;
extern int g_txPower;
extern bool g_slept;
#include "inkblink.ino"
#include <ctime>
#include <string>
static int passN=0, failN=0;
#define CHECK(c) do{ if(c){passN++;} else {failN++; printf("  FAIL line %d: %s\n",__LINE__,#c);} }while(0)
#define SECTION(n) printf("\n== %s ==\n",n)

static void dumpRaw(const char* path){ FILE* f=fopen(path,"wb"); for(int y=0;y<SCREEN_H;y++) for(int x=0;x<SCREEN_W;x++) fputc(gfxIsInk(x,y)?1:0,f); fclose(f); }
static void resetState(){ for(int i=0;i<64;i++) g_pin[i]=1; g_ms=1000; }
static void writeFile(const char* name,const std::string& data){ std::string p=std::string("build/run/fs/")+name; FILE* f=fopen(p.c_str(),"wb"); fwrite(data.data(),1,data.size(),f); fclose(f); }
static int countLinesOver(int& maxw){ // sprawdza szerokosc kazdej linii kazdej strony
  char buf[PAGE_BUF+1]; int bad=0; maxw=0;
  for(int p=0;p<totalPages;p++){
    book.seek(pageOff[p]); int n=book.read((uint8_t*)buf,PAGE_BUF); if(n<0)n=0; buf[n]=0; sanitizeBuf(buf,n);
    int pos=0;
    for(int r=0;r<linesPerPage;r++){ int adv; int len=lineLength(buf,pos,adv); int w=0;
      for(int i=0;i<len;){ uint32_t cp; int cb=charBytes((uint8_t)buf[pos+i]); gfxU8Decode(buf+pos+i,cb,cp); w+=fontCharW(cp); i+=cb; }
      if(w>maxw)maxw=w; if(w>maxTextW) bad++; pos+=adv; if(buf[pos]==0)break; }
  }
  return bad;
}

// ---------- symulacja przyciskow ----------
struct Hold { int pin; unsigned long from, to; };
struct Rec { unsigned long t; Key k; EvType type; int rep; };
static std::vector<Hold> g_holds; static unsigned long g_t0;
static int simRead(int pin){ for(auto&h:g_holds) if(h.pin==pin && g_ms>=g_t0+h.from && g_ms<g_t0+h.to) return 0; return 1; }
static std::vector<Rec> simulate(std::vector<Hold> holds, unsigned long until){
  std::vector<Rec> out; g_holds=holds; g_t0=g_ms; g_readHook=simRead;
  while(g_ms < g_t0+until){
    KeyEvent e=inputPoll(); if(e.type!=EV_NONE) out.push_back({g_ms-g_t0,e.key,e.type,e.repeat});
    g_ms+=5;
  }
  g_readHook=nullptr; for(int i=0;i<64;i++) g_pin[i]=1; g_ms+=200; inputPoll(); g_ms+=200;
  return out;
}
static int cnt(const std::vector<Rec>& v, EvType t){ int n=0; for(auto&r:v) if(r.type==t)n++; return n; }

int main(){
  resetState();
  system("rm -rf build/run/fs/* build/run/png; mkdir -p build/run/png");
  // dane testowe: polska ksiazka (BOM, CRLF), ~200 KB
  { srand(7); const char* w[]={"zażółć","gęślą","jaźń","Pchnąć","w","tę","łódź","jeża","lub","ośm","skrzyń","fig","dżdżownica","„cytat”","—","myślnik…","Źdźbło","chrząszcz","brzmi","w","trzcinie","i","a","na","się","nie","Wędrowiec","szedł","długo","przez","ciemny","las","internationalization","konstantynopolitańczykowianeczka"};
    std::string t="\xEF\xBB\xBF"; int nw=sizeof(w)/sizeof(w[0]);
    for(int p=0;p<500;p++){ int k=20+rand()%90; for(int i=0;i<k;i++){ t+=w[rand()%nw]; t+=(i+1<k)?" ":""; } t+="\r\n\r\n"; }
    writeFile("Polska_ksiazka.txt",t); printf("ksiazka: %zu bajtow\n",t.size());
    writeFile("maly.txt","Krotki tekst.\nDruga linia.\n\nAkapit po pustej linii, ktory jest wystarczajaco dlugi, zeby sie zawinal.");
    writeFile("notatki.pdf","x"); }
  fsOk=true; prefs.begin("reader",false); fontInit(); loadSettings();

  SECTION("T1 uklady czcionek");
  for(int fam=0;fam<fontFamilyCount();fam++) for(int si=0;si<fontSizeCount(fam);si++) for(int b=0;b<2;b++){
    cfg.famIdx=fam; cfg.fontIdx=si; cfg.bold=b; applyLayout();
    printf("  %-9s %2dpx bold=%d  lineH=%2d  lines/page=%d  maxW=%d x0=%d  sig=%08x\n",fontFamilyName(fam),fontSizeValue(fam,si),b,fontLineH(),linesPerPage,maxTextW,textX0,layoutSig());
    CHECK(linesPerPage>=1); CHECK(maxTextW<=SCREEN_W-2*MARGIN_X); CHECK(textX0>=MARGIN_X); CHECK(fontLineH()*linesPerPage+fontTopPad()<=STATUS_Y);
  }
  // podpisy ukladu rozne dla roznych ustawien
  { std::map<uint32_t,int> seen; int dup=0; for(int fam=0;fam<3;fam++) for(int si=0;si<fontSizeCount(fam);si++) for(int b=0;b<2;b++){ cfg.famIdx=fam;cfg.fontIdx=si;cfg.bold=b;applyLayout(); if(seen[layoutSig()]++) dup++; } CHECK(dup==0); }

  SECTION("T2 zawijanie: zadna linia nie przekracza szerokosci, indeks sie konczy");
  for(int fam=0;fam<3;fam++) for(int si=0;si<fontSizeCount(fam);si++){
    cfg.famIdx=fam; cfg.fontIdx=si; cfg.bold=false; applyLayout();
    clock_t c0=clock(); bool ok=openBook("Polska_ksiazka.txt"); double ms=1000.0*(clock()-c0)/CLOCKS_PER_SEC;
    int maxw; int bad=countLinesOver(maxw);
    bool mono=true; for(int i=1;i<totalPages;i++) if(pageOff[i]<=pageOff[i-1]) mono=false;
    printf("  %-9s %2dpx: stron=%d, najszersza linia=%d/%d px, za szerokich=%d, indeks %.0f ms (host)\n",fontFamilyName(fam),fontSizeValue(fam,si),totalPages,maxw,maxTextW,bad,ms);
    CHECK(ok); CHECK(bad==0); CHECK(mono); CHECK(totalPages>10);
    closeBook();
  }

  SECTION("T3 cache indeksu i zakladka");
  cfg.famIdx=0; cfg.fontIdx=1; cfg.bold=false; applyLayout();
  { char ip[28]; CHECK(openBook("Polska_ksiazka.txt")); int tp=totalPages; idxPath(curBook,layoutSig(),ip,sizeof(ip)); CHECK(LittleFS.exists(ip));
    page=tp/3; saveBookmark(); uint32_t off=pageOff[page]; closeBook();
    CHECK(openBook("Polska_ksiazka.txt")); CHECK(totalPages==tp); CHECK(page==tp/3); CHECK(pageOff[page]==off);   // z cache + wznowienie
    // inny uklad = inny plik indeksu
    cfg.fontIdx=0; applyLayout(); char ip2[28]; idxPath(curBook,layoutSig(),ip2,sizeof(ip2)); CHECK(strcmp(ip,ip2)!=0);
    closeBook(); }

  SECTION("T4 dlugie OK: zmiana rozmiaru zachowuje miejsce w tekscie");
  for(int fam=0;fam<3;fam++){
    cfg.famIdx=fam; cfg.fontIdx=0; cfg.bold=false; applyLayout(); openBook("Polska_ksiazka.txt");
    page=totalPages/2; uint32_t off=pageOff[page]; screen=S_READER;
    int cntSz=fontSizeCount(fam);
    cycleFontSize();
    if(cntSz>1){ CHECK(cfg.fontIdx==1); uint32_t nowOff=pageOff[page]; uint32_t nextOff=(page+1<totalPages)?pageOff[page+1]:bookSize;
      printf("  %-9s: bylo bajt %u -> strona zaczyna sie w %u, nastepna w %u\n",fontFamilyName(fam),off,nowOff,nextOff);
      CHECK(nowOff<=off && off<nextOff); }
    else { CHECK(cfg.fontIdx==0); }
    closeBook();
  }

  SECTION("T5 przyciski: zdarzenia");
  { auto v=simulate({{PIN_KEY_DOWN,100,220}},800); CHECK(v.size()==1 && v[0].type==EV_PRESS && v[0].k==KEY_DOWN); printf("  tap DOWN -> %zu zdarzenie\n",v.size()); }
  { auto v=simulate({{PIN_KEY_DOWN,100,2100}},2600); int r=cnt(v,EV_REPEAT); printf("  hold DOWN 2 s -> PRESS=%d REPEAT=%d, pierwszy REPEAT po %lu ms\n",cnt(v,EV_PRESS),r,v.size()>1?v[1].t-v[0].t:0);
    CHECK(cnt(v,EV_PRESS)==1); CHECK(r>=5 && r<=8); CHECK(v.size()>1 && v[1].t-v[0].t>=480 && v[1].t-v[0].t<=560); for(size_t i=2;i<v.size();i++) CHECK(v[i].t-v[i-1].t>=240&&v[i].t-v[i-1].t<=290); for(size_t i=1;i<v.size();i++) CHECK(v[i].rep==(int)i); }
  { auto v=simulate({{PIN_KEY_OK,100,260}},800); CHECK(v.size()==1 && v[0].type==EV_SHORT && v[0].k==KEY_OK); printf("  tap OK -> SHORT po puszczeniu (t=%lu)\n",v.empty()?0:v[0].t); CHECK(!v.empty()&&v[0].t>=250); }
  { auto v=simulate({{PIN_KEY_OK,100,1900}},2400); printf("  hold OK 1,8 s -> LONG=%d SHORT=%d (LONG po %lu ms)\n",cnt(v,EV_LONG),cnt(v,EV_SHORT),v.empty()?0:v[0].t-100); CHECK(cnt(v,EV_LONG)==1&&cnt(v,EV_SHORT)==0); CHECK(!v.empty()&&v[0].t-100>=790&&v[0].t-100<=900); }
  { auto v=simulate({{PIN_KEY_HOME,100,500}},900); CHECK(v.size()==1&&v[0].type==EV_SHORT&&v[0].k==KEY_HOME); printf("  tap HOME 0,4 s -> SHORT (nie LONG)\n"); }
  { auto v=simulate({{PIN_KEY_DOWN,100,108}},500); CHECK(v.empty()); printf("  drgania 8 ms -> %zu zdarzen\n",v.size()); }
  { auto v=simulate({{PIN_KEY_DOWN,100,400},{PIN_KEY_DOWN,406,900}},1300); printf("  trzymanie z krotka przerwa 6 ms -> PRESS=%d\n",cnt(v,EV_PRESS)); CHECK(cnt(v,EV_PRESS)==1); }
  { auto v=simulate({{PIN_KEY_UP,100,160},{PIN_KEY_DOWN,700,760}},1300); CHECK(cnt(v,EV_PRESS)==2 && v[0].k==KEY_UP && v[1].k==KEY_DOWN); printf("  dwa szybkie dotkniecia UP, DOWN -> %zu zdarzenia\n",v.size()); }
  CHECK(accelStep(0)==1&&accelStep(1)==1&&accelStep(2)==3&&accelStep(3)==10&&accelStep(4)==25&&accelStep(5)==50&&accelStep(200)==50);

  SECTION("T6 czytnik: przewracanie, krance, skok");
  cfg.famIdx=0; cfg.fontIdx=1; cfg.bold=false; applyLayout(); openBook("Polska_ksiazka.txt"); screen=S_READER; page=0;
  { int tp=totalPages; turnPages(-5); CHECK(page==0); turnPages(+3); CHECK(page==3); turnPages(+10); CHECK(page==13); turnPages(100000); CHECK(page==tp-1); turnPages(1); CHECK(page==tp-1); }
  closeBook();

  SECTION("T7 strona WWW: angielski, nowe pola, zapis");
  { auto ascii=[&](const std::string& s){ for(unsigned char c: s) if(c>127) return false; return true; };
    g_args.clear(); g_lastBody.clear(); handleConfigGet(); CHECK(g_lastCode==200);
    for(const char* w: {"Text margins","Reading","Status line","Display","Power","Sleep screen","Estimated time left","Battery in the book","Page flip","Save settings","name='bat'","name='flip'","name='fullev'","name='sscr'","Helvetica 12 px","Unifont 16 px","Classic 16 px"}) CHECK(g_lastBody.find(w)!=std::string::npos);
    CHECK(ascii(g_lastBody)); CHECK(g_lastBody.find("lang=\"en\"")!=std::string::npos); CHECK(g_lastBody.find("Settings saved")==std::string::npos);
    for(const char* pl: {"Ustawienia","Zapisz","Czcionka","Ksiazki","Wyslij"}) CHECK(g_lastBody.find(pl)==std::string::npos);
    g_args["ok"]="1"; g_lastBody.clear(); handleConfigGet(); CHECK(g_lastBody.find("Settings saved")!=std::string::npos); g_args.clear();
    printf("  strona ustawien: po angielsku, wszystkie sekcje, tylko ASCII\n");
    g_args["font"]="12"; g_args["st"]="on"; g_args["margin"]="2"; g_args["bold"]="on"; g_args["pct"]="on"; g_args["left"]="on"; g_args["bat"]="2"; g_args["flip"]="1"; g_args["fullev"]="2"; g_args["sleep"]="3"; g_args["sscr"]="1";
    handleConfigPost(); CHECK(g_lastCode==303 && g_lastLoc=="/config?ok=1");
    CHECK(cfg.famIdx==1 && cfg.fontIdx==2 && cfg.bold && cfg.showPct && !cfg.showPage && cfg.showLeft && !cfg.showBar && cfg.batMode==2 && cfg.fastFlip && cfg.fullEvery==2 && cfg.sleepIdx==3 && cfg.sleepScreen==1 && !cfg.autoOpen && cfg.margin==2);
    CHECK(prefs.u["cfg_mg"]==2); CHECK(prefs.u["cfg_fm"]==1 && prefs.u["cfg_fs"]==2 && prefs.u["cfg_tl"]==1 && prefs.u["cfg_bm"]==2 && prefs.u["cfg_ff"]==1 && prefs.u["cfg_fe"]==2 && prefs.u["cfg_ss"]==1);
    printf("  zapis: %s %dpx bold=%d left=%d bat=%d flip=%d every=%d sleep=%d min sscr=%d\n",fontFamilyName(cfg.famIdx),fontSizeValue(cfg.famIdx,cfg.fontIdx),cfg.bold,cfg.showLeft,cfg.batMode,cfg.fastFlip,fullEveryVals[cfg.fullEvery],sleepMinutes[cfg.sleepIdx],cfg.sleepScreen);
    g_args.clear(); g_args["font"]="99"; g_args["sleep"]="9"; g_args["bat"]="7"; g_args["fullev"]="5"; g_args["sscr"]="4"; handleConfigPost();
    CHECK(cfg.famIdx==1&&cfg.fontIdx==2&&cfg.sleepIdx==3&&cfg.batMode==2&&cfg.fullEvery==2&&cfg.sleepScreen==1); printf("  bledne wartosci ignorowane\n");
    // wczytywanie ustawien po restarcie
    Settings keep=cfg; cfg=Settings{FAM_CLASSIC,1,false,true,true,false,true,1,false,1,1,0,true,0}; loadSettings(); CHECK(cfg.famIdx==keep.famIdx&&cfg.showLeft==keep.showLeft&&cfg.batMode==keep.batMode&&cfg.fastFlip==keep.fastFlip&&cfg.sleepScreen==keep.sleepScreen); printf("  ustawienia odtwarzane z pamieci po restarcie\n");
    // strona glowna
    scanBooks(); g_lastBody.clear(); handleRoot(); CHECK(g_lastCode==200); for(const char* w: {"Add books","Library","Free","Books","Drag &amp; drop","data-f='"}) CHECK(g_lastBody.find(w)!=std::string::npos); CHECK(ascii(g_lastBody));
    CHECK(g_lastBody.find("Battery")==std::string::npos); printf("  strona glowna: karty Add books / Library, brak baterii gdy pin nieustawiony\n");
    g_lastBody.clear(); handleWallpaperPage(); for(const char* w: {"Sleep image","Send to reader","Use built-in image","id=\"pv\"","Dithered"}) CHECK(g_lastBody.find(w)!=std::string::npos); CHECK(ascii(g_lastBody)); printf("  strona obrazu uspienia OK\n"); }

  SECTION("T8 upload: bezpieczne nazwy, plik tymczasowy, brak kasowania starej ksiazki");
  { auto up=[&](HTTPUploadStatus st,const char* fn,const char* data){ g_up.status=st; g_up.filename=String(fn); g_up.buf=(uint8_t*)data; g_up.currentSize=data?strlen(data):0; handleUploadData(); };
    uploadDone=false; writeFile("A.txt","stara ksiazka A");
    up(UPLOAD_FILE_START,"Moja ksi\xC4\x85\xC5\xBCka (1).txt",nullptr); up(UPLOAD_FILE_WRITE,"czesc ","czesc "); up(UPLOAD_FILE_WRITE,"druga","druga"); up(UPLOAD_FILE_END,"",nullptr);
    CHECK(uploadDone); printf("  zapisano jako: %s\n",lastUploaded); CHECK(std::string(lastUploaded)=="Moja_ksiazka__1_.txt");
    { std::string p=std::string("/")+lastUploaded; CHECK(LittleFS.exists(p.c_str())); File f=LittleFS.open(p.c_str()); char b[32]={0}; f.read((uint8_t*)b,31); f.close(); CHECK(std::string(b)=="czesc druga"); }
    CHECK(!LittleFS.exists("/upload.tmp"));
    uploadErr=""; up(UPLOAD_FILE_START,"obraz.pdf",nullptr); up(UPLOAD_FILE_WRITE,"x","x"); up(UPLOAD_FILE_END,"",nullptr);
    CHECK(uploadErr.length()>0); CHECK(LittleFS.exists("/A.txt")); CHECK(LittleFS.exists((std::string("/")+lastUploaded).c_str())); printf("  .pdf odrzucony, poprzednie ksiazki nietkniete\n");
    handleUploadFinished(); CHECK(g_lastCode==400);
    up(UPLOAD_FILE_START,"A.txt",nullptr); up(UPLOAD_FILE_WRITE,"polowa","polowa"); up(UPLOAD_FILE_ABORTED,"",nullptr);
    { File f=LittleFS.open("/A.txt"); char b[32]={0}; f.read((uint8_t*)b,31); f.close(); CHECK(std::string(b)=="stara ksiazka A"); } CHECK(!LittleFS.exists("/upload.tmp")); printf("  przerwany upload nie rusza starej wersji\n");
    std::string longn(80,'x'); longn+=".txt"; uploadErr=""; up(UPLOAD_FILE_START,longn.c_str(),nullptr); up(UPLOAD_FILE_WRITE,"d","d"); up(UPLOAD_FILE_END,"",nullptr);
    CHECK(uploadErr.length()==0 && endsWithTxt(lastUploaded) && strlen(lastUploaded)<NAME_LEN); printf("  dluga nazwa przycieta do %zu znakow, nadal .txt\n",strlen(lastUploaded));
    cfg.famIdx=0; cfg.fontIdx=1; cfg.bold=false; applyLayout(); openBook("Polska_ksiazka.txt"); closeBook(); cfg.fontIdx=2; applyLayout(); openBook("Polska_ksiazka.txt"); closeBook();
    int idx=0; { File r=LittleFS.open("/"); File f=r.openNextFile(); while(f){ if(f.name()[0]=='i') idx++; f.close(); f=r.openNextFile(); } r.close(); } CHECK(idx>=2);
    g_args.clear(); g_args["f"]="Polska_ksiazka.txt"; handleDelete(); CHECK(!LittleFS.exists("/Polska_ksiazka.txt"));
    idx=0; { File r=LittleFS.open("/"); File f=r.openNextFile(); while(f){ if(f.name()[0]=='i') idx++; f.close(); f=r.openNextFile(); } r.close(); } CHECK(idx==0); printf("  usuniecie ksiazki usuwa tez jej indeksy\n");
    handleRoot(); CHECK(g_lastCode==200 && g_lastBody.find("id=\"file\"")!=std::string::npos && g_lastBody.find("A.txt")!=std::string::npos); printf("  strona glowna = karta uploadu + lista ksiazek\n"); }
  { char o[NAME_LEN]; CHECK(makeBookName("Wiedźmin - Ostatnie życzenie.txt",o)); printf("  transliteracja: %s\n",o); CHECK(std::string(o)=="Wiedzmin_-_Ostatnie_zyczenie.txt");
    CHECK(makeBookName("ą.TXT",o)&&std::string(o)=="a.txt"); CHECK(!makeBookName("a.pdf",o)); CHECK(!makeBookName(".txt",o)); CHECK(makeBookName("C:\\Users\\x\\Ł ó.txt",o)&&std::string(o)=="L_o.txt"); CHECK(makeBookName("../../etc/passwd.txt",o)&&std::string(o)=="passwd.txt"); }

  SECTION("T9 hotspot: siec, stale haslo, DNS, testy lacznosci");
  { prefs.s.erase("ap_pw"); g_apUp=false; g_dnsUp=false; screen=S_MENU;
    goUpload(); CHECK(screen==S_UPLOAD && g_apUp && g_dnsUp && g_wifiMode==WIFI_AP);
    char ssid[24]; apSsid(ssid,sizeof(ssid)); String pw=apPassword(); printf("  siec: %s, haslo: %s\n",ssid,pw.c_str());
    CHECK(strncmp(ssid,"InkBlink-",9)==0 && strlen(ssid)==13); CHECK(pw.length()==8); for(size_t i=0;i<pw.length();i++) CHECK(pw[i]>='0'&&pw[i]<='9');
    uploadStop(); CHECK(!g_apUp && !g_dnsUp && g_wifiMode==WIFI_OFF);
    goUpload(); CHECK(apPassword()==pw); uploadStop(); printf("  haslo to samo po ponownym wejsciu (telefon zapamieta siec)\n");
    String pw2=apPasswordNew(); CHECK(pw2.length()==8 && prefs.s["ap_pw"]==pw2.s); printf("  nowe haslo zapisane\n");
    g_uri="/generate_204"; handleNotFound(); CHECK(g_lastCode==204);
    g_uri="/hotspot-detect.html"; handleNotFound(); CHECK(g_lastCode==200 && g_lastBody.find("Success")!=std::string::npos);
    g_uri="/connecttest.txt"; handleNotFound(); CHECK(g_lastCode==200 && g_lastBody=="Microsoft Connect Test");
    g_uri="/ncsi.txt"; handleNotFound(); CHECK(g_lastCode==200 && g_lastBody=="Microsoft NCSI");
    g_uri="/cokolwiek"; handleNotFound(); CHECK(g_lastCode==302 && g_lastLoc=="http://192.168.4.1/"); printf("  testy lacznosci Android/iOS/Windows + przekierowanie reszty OK\n");
    // blad tworzenia hotspotu: haslo za krotkie (symulacja) -> komunikat i menu
    extern bool g_apFail; g_apFail=true; goUpload(); CHECK(screen==S_MENU && !g_apUp && g_wifiMode==WIFI_OFF); g_apFail=false; printf("  blad hotspotu -> wraca do menu\n");
    prefs.s["ap_pw"]="123"; CHECK(apPassword().length()==8); printf("  zbyt krotkie haslo w pamieci jest naprawiane\n"); prefs.s.erase("ap_pw"); }

  SECTION("T10 ustawienia: grupy, pozycje, zapis");
  { cfg=Settings{FAM_CLASSIC,1,false,true,true,false,true,1,false,1,1,0,true,0,0,false,0,false,0}; applyLayout(); goSettings(); clockNowHook=[]()->time_t{return 0;};
    KeyEvent ok{KEY_OK,EV_SHORT,0}, dn{KEY_DOWN,EV_REPEAT,3}, up{KEY_UP,EV_PRESS,0}, bk{KEY_BACK,EV_SHORT,0};
    CHECK(setGroup==-1 && settingsCount()==SET_GROUPS && screen==S_SETTINGS);
    // kazda pozycja w kazdej grupie miesci sie w wierszu
    int total=0; for(int g=0;g<SET_GROUPS;g++){ setGroup=g; for(int i=0;i<settingsCount();i++){ const char* a=settingsItem(i); total++; CHECK(strlen(a)<=(size_t)(UI_CHARS-2)); if(strlen(a)>(size_t)(UI_CHARS-2)) printf("    za dlugi: [%s]\n",a); } }
    printf("  %d pozycji w %d grupach, wszystkie mieszcza sie w wierszu\n",total,SET_GROUPS);
    setGroup=-1; setSel=0; onKeySettings(ok); CHECK(setGroup==0 && setSel==0);                       // Reading
    onKeySettings(ok); CHECK(cfg.famIdx==1); onKeySettings(ok); CHECK(cfg.famIdx==2 && cfg.fontIdx==0); onKeySettings(ok); CHECK(cfg.famIdx==3); onKeySettings(ok); CHECK(cfg.famIdx==0);
    CHECK(strstr(settingsItem(0),"Classic")!=nullptr); cfg.famIdx=3; cfg.fontIdx=0; CHECK(strstr(settingsItem(0),"Century")!=nullptr); cfg.famIdx=0; cfg.fontIdx=1;
    setSel=1; for(int i=0;i<5;i++) onKeySettings(ok); CHECK(cfg.fontIdx<4); setSel=2; onKeySettings(ok); CHECK(cfg.bold);
    setSel=3; onKeySettings(ok); CHECK(cfg.lineGap==1 && strstr(settingsItem(3),"Relaxed")!=nullptr); onKeySettings(ok); CHECK(cfg.lineGap==2); onKeySettings(ok); CHECK(cfg.lineGap==0);
    setSel=4; onKeySettings(ok); CHECK(cfg.margin==1); onKeySettings(ok); CHECK(cfg.margin==2); onKeySettings(ok); CHECK(cfg.margin==0); CHECK(strstr(settingsItem(4),"Normal")!=nullptr);
    setSel=5; onKeySettings(ok); CHECK(cfg.hyphen && strstr(settingsItem(5),"ON")!=nullptr); onKeySettings(ok); CHECK(!cfg.hyphen);
    printf("  grupa Reading: font (4 rodziny), rozmiar, bold, interlinia, marginesy, dzielenie wyrazow\n");
    onKeySettings(bk); CHECK(screen==S_SETTINGS && setGroup==-1 && setSel==0); CHECK(prefs.u["cfg_bd"]==1 && prefs.u["cfg_hy"]==0 && prefs.u["cfg_lg"]==0);   // powrot na liste grup, zapis
    setSel=1; onKeySettings(ok); CHECK(setGroup==1);                                                  // Status line
    setSel=3; onKeySettings(ok); CHECK(cfg.showLeft && strstr(settingsItem(3),"ON")!=nullptr);
    setSel=5; onKeySettings(ok); CHECK(cfg.clockBook); CHECK(statusVisible());
    setSel=0; int lpOn=linesPerPage; CHECK(strstr(settingsItem(0),"Status line: ON")!=nullptr && statusAnyOn());
    onKeySettings(ok); CHECK(!statusAnyOn() && !statusVisible() && !cfg.showPct && !cfg.showPage && !cfg.showLeft && !cfg.showBar && !cfg.clockBook && cfg.batMode==0 && linesPerPage>lpOn);
    CHECK(strstr(settingsItem(0),"OFF")!=nullptr); for(int i=1;i<=5;i++) CHECK(strstr(settingsItem(i),"OFF")!=nullptr);      // wszystkie podopcje pokazuja OFF
    printf("  Status line OFF: wszystkie podopcje OFF, %d -> %d wierszy na stronie\n",lpOn,linesPerPage);
    onKeySettings(ok); CHECK(statusAnyOn() && statusVisible() && cfg.showPct && cfg.showPage && cfg.showLeft && cfg.showBar && cfg.clockBook && cfg.batMode==1 && linesPerPage==lpOn);
    for(int i=1;i<=5;i++) CHECK(strstr(settingsItem(i),"ON")!=nullptr); printf("  Status line ON: wszystkie podopcje ON\n");
    setSel=2; onKeySettings(ok); CHECK(!cfg.showPage && statusAnyOn() && strstr(settingsItem(0),"ON")!=nullptr); onKeySettings(ok);             // pojedyncza podopcja nie wylacza glownego
    CHECK(strstr(settingsItem(6),"pin not set")!=nullptr); setSel=6; onKeySettings(ok); CHECK(cfg.batMode==2); onKeySettings(ok); CHECK(cfg.batMode==0); onKeySettings(ok); CHECK(cfg.batMode==1);
    onKeySettings(bk); CHECK(setGroup==-1 && setSel==1);                                              // wraca na te sama grupe
    setSel=2; onKeySettings(ok); CHECK(setGroup==2);                                                  // Clock
    setSel=0; onKeySettings(ok); CHECK(cfg.clockMode==1 && strstr(settingsItem(0),"24h")!=nullptr); onKeySettings(ok); CHECK(cfg.clockMode==2 && strstr(settingsItem(0),"12h")!=nullptr); onKeySettings(ok); CHECK(cfg.clockMode==0);
    setSel=1; onKeySettings(ok); CHECK(cfg.tz==1 && strstr(settingsItem(1),"UK")!=nullptr); for(int i=0;i<TZ_COUNT-1;i++) onKeySettings(ok); CHECK(cfg.tz==0); CHECK(strstr(settingsItem(1),"Central Europe")!=nullptr);
    CHECK(strstr(settingsItem(2),"not set")!=nullptr); printf("  grupy Status line i Clock: czas jeszcze nie ustawiony -> 'not set'\n");
    onKeySettings(bk); setSel=3; onKeySettings(ok); CHECK(setGroup==3);                              // Power & system
    setSel=0; onKeySettings(ok); CHECK(cfg.sleepIdx==2); setSel=1; onKeySettings(ok); CHECK(cfg.sleepScreen==1); onKeySettings(ok); onKeySettings(ok); CHECK(cfg.sleepScreen==0);
    setSel=2; onKeySettings(ok); CHECK(cfg.fastFlip && strstr(settingsItem(2),"Fast")!=nullptr); onKeySettings(ok); CHECK(!cfg.fastFlip);
    setSel=3; onKeySettings(ok); CHECK(cfg.fastMenu && strstr(settingsItem(3),"Fast")!=nullptr); onKeySettings(ok); CHECK(!cfg.fastMenu && strstr(settingsItem(3),"Full")!=nullptr); onKeySettings(ok); CHECK(cfg.fastMenu);
    setSel=4; onKeySettings(ok); CHECK(cfg.uiFont==2 && strstr(settingsItem(4),"Helv 12")!=nullptr); onKeySettings(ok); CHECK(cfg.uiFont==3 && strstr(settingsItem(4),"Unifont")!=nullptr); onKeySettings(ok); CHECK(cfg.uiFont==0 && strstr(settingsItem(4),"Classic")!=nullptr);
    setSel=5; onKeySettings(ok); CHECK(cfg.fullEvery==2); onKeySettings(ok); CHECK(cfg.fullEvery==0);
    setSel=7; onKeySettings(ok); CHECK(cfg.apPower==1 && strstr(settingsItem(7),"Medium")!=nullptr); onKeySettings(ok); CHECK(cfg.apPower==2 && strstr(settingsItem(7),"High")!=nullptr); onKeySettings(ok); CHECK(cfg.apPower==0 && strstr(settingsItem(7),"Low")!=nullptr);
    setSel=8; onKeySettings(ok); CHECK(strncmp(settingsItem(8),"Hotspot: ",9)==0);
    setSel=settingsCount()-1; onKeySettings(dn); CHECK(setSel==0); onKeySettings(up); CHECK(setSel==settingsCount()-1);
    KeyEvent hm{KEY_HOME,EV_SHORT,0}; onKeySettings(hm); CHECK(screen==S_SETTINGS && setGroup==3); printf("  HOME w ustawieniach nic nie robi\n");
    onKeySettings(bk); CHECK(setGroup==-1); onKeySettings(bk); CHECK(screen==S_MENU); CHECK(prefs.u["cfg_bd"]==1 && prefs.u["cfg_cb"]==1); printf("  BACK: z grupy na liste, z listy do menu; ustawienia zapisane\n");
    screen=S_SETTINGS; setGroup=0; setSel=2; KeyEvent lng{KEY_OK,EV_LONG,0}; bool b0=cfg.bold; onKeySettings(lng); CHECK(cfg.bold==b0); setGroup=-1; }

  SECTION("T13 legenda klawiszy: render ekranow i geometria");
  { const int midTop=(SCREEN_H-TAG_H)/2; const int cx0=SCREEN_W-TAG_COL_W;
    struct R{ const char* name; };
    auto inkIn=[&](int x0,int y0,int x1,int y1){ int n=0; for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++) if(gfxIsInk(x,y)) n++; return n; };
    auto contentOverflow=[&](){ // tusz w kolumnie legendy poza rzedami etykiet/strzalek?
      int bad=0; for(int y=0;y<SCREEN_H;y++){ bool legendRow=(y<TAG_H)||(y>=midTop-9&&y<midTop+TAG_H+3+6)||(y>=SCREEN_H-TAG_H);
        if(legendRow) continue; for(int x=cx0-2;x<SCREEN_W;x++) if(gfxIsInk(x,y)) bad++; } return bad; };
    // reset ustawien do domyslnych i dane testowe
    cfg=Settings{FAM_CLASSIC,1,false,true,true,true,1,true}; applyLayout(); prefs.s.erase("ap_pw");
    writeFile("Wiedzmin_-_Ostatnie_zyczenie.txt","x"); writeFile("Bardzo_dluga_nazwa_ksiazki_ktora_sie_nie_miesci_w_wierszu.txt","x"); writeFile("Pan_Tadeusz.txt","x"); writeFile("Lalka.txt","x"); writeFile("Ksiegi_Jakubowe.txt","x"); writeFile("Solaris.txt","x"); writeFile("Dziady.txt","x");
    writeFile("Polska_ksiazka.txt","\xEF\xBB\xBFTekst do skoku. Zażółć gęślą jaźń.\r\n");
    struct S{ const char* name; std::function<void()> f; bool top,mid,bot; } ss[]={
      {"menu",[&]{ prefs.s.erase("last"); menuSel=1; goMenu(); },false,true,false},
      {"library",[&]{ libSel=1; goLibrary(); },false,true,true},
      {"settings",[&]{ setSel=2; goSettings(); },false,true,true},
      {"goto",[&]{ cfg=Settings{FAM_CLASSIC,1,false,true,true,true,1,true}; applyLayout(); openBook("Polska_ksiazka.txt"); gotoVal=1; gotoStepIdx=1; goGoto(); },true,true,true},
      {"info",[&]{ goInfo(); },false,true,true},
      {"stats",[&]{ goStats(); },false,false,true},
      {"pinscan",[&]{ goPinScan(); },false,false,true},
      {"upload",[&]{ lastUploaded[0]=0; strcpy(lastUploaded,"Wiedzmin_-_Ostatnie_zyczenie.txt"); goUpload(); },false,true,true},
    };
    for(auto&x:ss){ g_apFail=false; x.f(); char path[64]; snprintf(path,sizeof(path),"build/run/png/ui_%s.raw",x.name); dumpRaw(path);
      int top=inkIn(cx0,0,SCREEN_W-1,TAG_H-1), mid=inkIn(cx0,midTop,SCREEN_W-1,midTop+TAG_H-1), bot=inkIn(cx0,SCREEN_H-TAG_H,SCREEN_W-1,SCREEN_H-1);
      printf("  %-9s top=%s mid=%s bot=%s  zawartosc w kolumnie legendy=%d px\n",x.name,top?"tak":"nie",mid?"tak":"nie",bot?"tak":"nie",contentOverflow());
      CHECK((top>0)==x.top); CHECK((mid>0)==x.mid); CHECK((bot>0)==x.bot); CHECK(contentOverflow()==0);
      if(screen==S_UPLOAD) uploadStop(); closeBook(); }
  }

  SECTION("T14 klawisze bez podpisu sa nieaktywne (przez prawdziwe loop())");
  { auto press=[&](int pin,unsigned long ms){ g_holds={{pin,0,ms}}; g_t0=g_ms; g_readHook=simRead; unsigned long end=g_t0+ms+500; while(g_ms<end){ loop(); g_ms+=5; } g_readHook=nullptr; for(int i=0;i<64;i++) g_pin[i]=1; g_ms+=300; };
    cfg=Settings{FAM_CLASSIC,1,false,true,true,true,1,true}; applyLayout();
    goInfo(); press(PIN_KEY_HOME,150); CHECK(screen==S_INFO); press(PIN_KEY_OK,150); CHECK(screen==S_PINSCAN); press(PIN_KEY_BACK,150); CHECK(screen==S_INFO); press(PIN_KEY_BACK,150); CHECK(screen==S_MENU); printf("  Info: HOME nic, OK = test pinow, EXIT wraca\n");
    goStats(); press(PIN_KEY_HOME,150); CHECK(screen==S_STATS); press(PIN_KEY_BACK,150); CHECK(screen==S_MENU); printf("  Stats: HOME nic, EXIT wraca\n");
    goLibrary(); press(PIN_KEY_HOME,150); CHECK(screen==S_LIBRARY); press(PIN_KEY_BACK,150); CHECK(screen==S_MENU); printf("  Biblioteka: HOME nic, EXIT wraca\n");
    goSettings(); press(PIN_KEY_HOME,150); CHECK(screen==S_SETTINGS); press(PIN_KEY_BACK,150); CHECK(screen==S_MENU); printf("  Ustawienia: HOME nic, EXIT wraca\n");
    goUpload(); CHECK(screen==S_UPLOAD); press(PIN_KEY_HOME,150); CHECK(screen==S_UPLOAD && g_apUp); press(PIN_KEY_BACK,150); CHECK(screen==S_MENU && !g_apUp); printf("  Upload: HOME nic, EXIT zatrzymuje hotspot\n");
    // w czytniku HOME i EXIT dzialaja (brak legendy na ekranie z tekstem)
    openBook("Polska_ksiazka.txt"); screen=S_READER; showPage(false); press(PIN_KEY_HOME,150); CHECK(screen==S_MENU); openBook("Polska_ksiazka.txt"); screen=S_READER; showPage(false); press(PIN_KEY_BACK,150); CHECK(screen==S_LIBRARY); printf("  Czytnik: HOME -> menu, EXIT -> biblioteka\n"); closeBook(); }

  SECTION("T15 hotspot: licznik telefonow i zadan, rzadkie odswiezanie");
  { g_apFail=false; g_stations=0; g_onEventCalls=0; setup(); CHECK(g_onEventCalls==1); closeBook(); goUpload(); CHECK(screen==S_UPLOAD && httpHits==0);
    unsigned long d0=lastUploadDraw; g_stations=1; WiFiEventInfo_t inf; statusDirty=false; onWifiEvent(ARDUINO_EVENT_WIFI_AP_STACONNECTED,inf); CHECK(statusDirty);
    g_ms+=1000; loop(); CHECK(lastUploadDraw==d0); printf("  zmiana tuz po rysowaniu: bez odswiezenia (limit 4 s)\n");
    g_ms+=4500; loop(); CHECK(lastUploadDraw!=d0 && !statusDirty); printf("  po 4 s licznik odswiezony (telefony=%d)\n",(int)WiFi.softAPgetStationNum());
    g_args.clear(); handleRoot(); handleRoot(); CHECK(httpHits==2 && statusDirty); g_uri="/foo"; handleNotFound(); CHECK(httpHits==3);
    // w trakcie wysylania pliku licznika nie odswiezamy (blokowaloby transfer)
    { g_up.status=UPLOAD_FILE_START; g_up.filename=String("x.txt"); g_up.buf=nullptr; g_up.currentSize=0; handleUploadData(); }
    CHECK(uploadFile); unsigned long d1=lastUploadDraw; g_ms+=9000; statusDirty=true; loop(); CHECK(lastUploadDraw==d1); printf("  w trakcie uploadu brak odswiezania ekranu\n");
    g_up.status=UPLOAD_FILE_ABORTED; handleUploadData(); CHECK(!uploadFile);
    g_ms+=9000; statusDirty=true; loop(); CHECK(lastUploadDraw!=d1);
    uploadStop(); }

  SECTION("T12 uspienie");
  { g_slept=false; cfg.sleepIdx=0; goSleep(); CHECK(g_slept); }


  SECTION("T44 hotspot: czysty start i ponawianie");
  { cfg=Settings{FAM_CLASSIC,1,false,true,true,false,true,1,false,1,1,0,true,0,0,false,0,false,0,true,0}; applyLayout(); g_apFail=false;
    g_apFailFirst=0; screen=S_MENU; goUpload(); CHECK(screen==S_UPLOAD && g_apUp && g_wifiMode==WIFI_AP); uploadStop(); CHECK(!g_apUp && g_wifiMode==WIFI_OFF);
    g_apFailFirst=2; screen=S_MENU; goUpload(); CHECK(screen==S_UPLOAD && g_apUp && g_apFailFirst==0); printf("  dwie nieudane proby, trzecia dziala: hotspot wstaje\n"); uploadStop();
    g_apFailFirst=3; screen=S_UPLOAD; goUpload(); CHECK(screen==S_MENU && !g_apUp && g_wifiMode==WIFI_OFF); printf("  trzy nieudane proby: komunikat i powrot do menu, WiFi wylaczone\n"); g_apFailFirst=0;
    { int want[3]={34,52,78}; for(int pw=0;pw<3;pw++){ cfg.apPower=pw; g_txPower=0; screen=S_MENU; goUpload(); CHECK(screen==S_UPLOAD && g_txPower==want[pw]); uploadStop(); } printf("  moc hotspotu Low/Medium/High: ustawiana po starcie (%d/%d/%d)\n",want[0],want[1],want[2]); cfg.apPower=1; }
    for(int i=0;i<5;i++){ screen=S_MENU; goUpload(); CHECK(screen==S_UPLOAD && g_apUp); uploadStop(); } printf("  5 szybkich wejsc i wyjsc z Upload pod rzad: za kazdym razem hotspot dziala\n"); }

  printf("\n==== WYNIK: %d testow OK, %d bledow ====\n",passN,failN);
  return failN?1:0;
}
