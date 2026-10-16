#include "trace.h"
#include "inkblink.ino"
static int passN=0, failN=0;
#define CHECK(c) do{ if(c){passN++;} else {failN++; printf("  FAIL line %d: %s\n",__LINE__,#c);} }while(0)
#define SECTION(n) printf("\n== %s ==\n",n)
static void writeFile(const char* name,const std::string& d){ std::string p=std::string("build/run/fs/")+name; FILE* f=fopen(p.c_str(),"wb"); fwrite(d.data(),1,d.size(),f); fclose(f); }
static int inkIn(int x0,int y0,int x1,int y1){ int n=0; for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++) if(gfxIsInk(x,y)) n++; return n; }
static KeyEvent K(Key k,EvType t){ return KeyEvent{k,t,0}; }
static char kind(){ return epdWasFast()?'f':'F'; }     // f = szybkie (stary obraz wysylany), F = pelne
int main(){
  for(int i=0;i<64;i++) g_pin[i]=1; g_ms=100000; system("rm -rf build/run/fs/*; mkdir -p build/run/png");
  fsOk=true; prefs.begin("reader",false); fontInit(); loadSettings(); statsLoadTotals(); clockNowHook=[]()->time_t{return 0;};
  std::string t; for(int i=0;i<500;i++) t+="Zażółć gęślą jaźń pchnąć w tę łódź jeża lub ośm skrzyń fig. Wędrowiec szedł długo przez ciemny las.\r\n"; writeFile("A.txt",t); writeFile("B.txt",t+"x"); writeFile("C.txt","krotka");
  cfg=Settings{FAM_CLASSIC,1,false,true,true,false,true,1,false,1,1,0,true,0,0,false,0,false,0,true}; applyLayout();

  SECTION("T37 listy: szybkie odswiezanie przy ruchu, pelne przy wejsciu");
  { prefs.s.erase("last"); g_trace.clear(); goMenu(); std::string pat; pat+=kind();
    for(int i=0;i<24;i++){ g_trace.clear(); onKeyMenu(K(KEY_DOWN,EV_PRESS)); pat+=kind(); }
    printf("  menu, 24 ruchy: %s\n",pat.c_str()); CHECK(pat=="FfffffffffFfffffffffFffff");       // F + 9 szybkich + F + 9 + F ...
    cfg.fastMenu=false; g_trace.clear(); goMenu(); std::string p2; p2+=kind(); for(int i=0;i<5;i++){ g_trace.clear(); onKeyMenu(K(KEY_DOWN,EV_PRESS)); p2+=kind(); } printf("  Menu flip = Full: %s\n",p2.c_str()); CHECK(p2=="FFFFFF"); cfg.fastMenu=true;
    // wejscie na inny ekran = pelne, ruch w nim = szybkie, zmiana wartosci = szybkie, wyjscie z grupy = pelne
    menuSel=0; for(int i=0;i<menuCount;i++) if(menuIds[i]==M_SETTINGS) menuSel=i; g_trace.clear(); onKeyMenu(K(KEY_OK,EV_SHORT)); std::string p3; p3+=kind(); CHECK(screen==S_SETTINGS);
    g_trace.clear(); onKeySettings(K(KEY_DOWN,EV_PRESS)); p3+=kind(); g_trace.clear(); onKeySettings(K(KEY_OK,EV_SHORT)); p3+=kind(); CHECK(setGroup==1);        // OK na liscie grup = wejscie = pelne
    g_trace.clear(); onKeySettings(K(KEY_OK,EV_SHORT)); p3+=kind(); g_trace.clear(); onKeySettings(K(KEY_DOWN,EV_PRESS)); p3+=kind(); g_trace.clear(); onKeySettings(K(KEY_BACK,EV_SHORT)); p3+=kind();
    printf("  Settings: wejscie F, ruch f, wejscie w grupe ?, zmiana wartosci ?, ruch ?, wyjscie z grupy ? -> %s\n",p3.c_str()); CHECK(p3[0]=='F' && p3[1]=='f' && p3[2]=='F' && p3[5]=='F' && p3[3]=='f' && p3[4]=='f');
    // biblioteka
    scanBooks(); goLibrary(); g_trace.clear(); onKeyLibrary(K(KEY_DOWN,EV_PRESS)); CHECK(kind()=='f'); printf("  biblioteka: ruch = szybkie\n");
    // ustawienie z menu zapisuje sie i wraca ze strony WWW
    g_args.clear(); g_args["mflip"]="0"; handleConfigPost(); CHECK(!cfg.fastMenu && prefs.u["cfg_mf"]==0); g_args["mflip"]="1"; handleConfigPost(); CHECK(cfg.fastMenu); g_lastBody.clear(); g_args.clear(); handleConfigGet(); CHECK(g_lastBody.find("name='mflip'")!=std::string::npos); printf("  strona WWW: Menu refresh Fast/Full\n"); }

  SECTION("T38 menu: BACK wraca do ksiazki");
  { prefs.s.erase("last"); goMenu(); CHECK(inkIn(SCREEN_W-TAG_COL_W,SCREEN_H-TAG_H,SCREEN_W-1,SCREEN_H-1)==0); onKeyMenu(K(KEY_BACK,EV_SHORT)); CHECK(screen==S_MENU); printf("  bez ostatniej ksiazki: brak etykiety READ, BACK nic nie robi\n");
    CHECK(openBook("A.txt")); screen=S_READER; page=5; showPage(false); saveBookmark(); closeBook(); { char lb[NAME_LEN]; CHECK(getLastBook(lb,NAME_LEN)); } goMenu();
    CHECK(inkIn(SCREEN_W-TAG_COL_W,SCREEN_H-TAG_H,SCREEN_W-1,SCREEN_H-1)>100); onKeyMenu(K(KEY_BACK,EV_SHORT)); CHECK(screen==S_READER && strcmp(curBook,"A.txt")==0 && page==5); printf("  BACK w menu: wraca na strone %d ksiazki %s\n",page+1,curBook); closeBook(); }

  SECTION("T39 ksiazka: dlugie BACK = ustawienia Reading i powrot");
  { cfg=Settings{FAM_CLASSIC,1,false,true,true,false,true,1,false,1,1,0,true,0,0,false,0,false,0,true}; applyLayout(); CHECK(openBook("B.txt")); screen=S_READER; page=40; showPage(false);
    uint32_t off=pageOff[page]; int pagesBefore=totalPages; onKeyReader(K(KEY_BACK,EV_LONG)); CHECK(screen==S_SETTINGS && setGroup==0 && settingsFromReader && setSel==0); CHECK(strcmp(settingsResumeBook,"B.txt")==0);
    CHECK(inkIn(SCREEN_W-TAG_COL_W,SCREEN_H-TAG_H,SCREEN_W-1,SCREEN_H-1)>100); printf("  ksiazka zamknieta, ekran Reading otwarty od razu (etykieta BOOK)\n");
    setSel=1; onKeySettings(K(KEY_OK,EV_SHORT)); CHECK(cfg.fontIdx==0);                                  // zmniejsz rozmiar czcionki
    setSel=3; onKeySettings(K(KEY_OK,EV_SHORT)); CHECK(cfg.lineGap==1);                                  // interlinia
    onKeySettings(K(KEY_BACK,EV_SHORT)); CHECK(screen==S_READER && !settingsFromReader && strcmp(curBook,"B.txt")==0);
    CHECK(pageOff[page]<=off && (page+1>=totalPages || off<pageOff[page+1])); printf("  BACK: wrocilismy do ksiazki z nowym ukladem (%d -> %d stron), ten sam fragment tekstu\n",pagesBefore,totalPages); CHECK(totalPages!=pagesBefore);
    CHECK(prefs.u["cfg_fs"]==0 && prefs.u["cfg_lg"]==1); closeBook();
    // zwykle wejscie do ustawien nie zostawia flagi
    goSettings(); CHECK(!settingsFromReader && setGroup==-1); }

  SECTION("T40 odstepy: wiersze nie nachodza na siebie, pogrubiony Unifont rysuje sie w calej szerokosci");
  { struct F{int fam,si; bool bold;} fs[]={{FAM_UNIFONT,0,false},{FAM_UNIFONT,0,true},{FAM_HELV,1,false},{FAM_HELV,3,true},{FAM_CENTURY,0,false},{FAM_CENTURY,3,false}};
    std::string line="Ćg Żj Śp Ąq Źy Ńę Ół"; std::string tx; for(int i=0;i<40;i++) tx+=line+"\n"; writeFile("D.txt",tx);
    for(auto&f:fs){ cfg=Settings{(uint8_t)f.fam,(uint8_t)f.si,f.bold,false,false,false,false,1,false,1,1,0,true,0,0,false,0,false,0,true}; applyLayout(); CHECK(openBook("D.txt")); screen=S_READER; page=0; showPage(false);
      int lh=fontLineH(), tp=fontTopPad(), bad=0, rows=0;
      for(int r=0;r+1<linesPerPage;r++){ int y0=tp+r*lh, y1=tp+(r+1)*lh; int lastInk=-1, firstNext=-1; for(int y=y0-4;y<y1+lh;y++){ bool ink=false; for(int x=0;x<SCREEN_W && !ink;x++) ink=gfxIsInk(x,y); if(!ink) continue; if(y<y1-1 && y>=y0-4 && (y>=y0)) lastInk=y; }
        int nextTop=-1; for(int y=y1-6;y<y1+lh;y++){ bool ink=false; for(int x=0;x<SCREEN_W&&!ink;x++) ink=gfxIsInk(x,y); if(ink){ nextTop=y; break; } }
        // najnizszy zapalony wiersz wiersza r (do polowy odstepu do nastepnego) i najwyzszy wiersza r+1 musza sie roznic
        int lowR=-1; for(int y=y0;y<y1+3;y++){ bool ink=false; for(int x=0;x<SCREEN_W&&!ink;x++) ink=gfxIsInk(x,y); if(ink && y<nextTop) lowR=y; }
        rows++; if(lowR>=0 && nextTop>=0 && lowR>=nextTop) bad++; }
      printf("  %-9s %2dpx bold=%d: wiersz %d px, %d wierszy na stronie, nakladajacych sie par: %d\n",fontFamilyName(f.fam),fontSizeValue(f.fam,f.si),f.bold,lh,linesPerPage,bad); CHECK(bad==0);
      // najnizszy ogonek (4 px pod linia bazowa) i najwyzszy akcent kapitalika (asc+3) mieszcza sie w wierszu
      CHECK(lh>=fAsc+4+1); closeBook(); }
    // Unifont pogrubiony: 26 znakow "W" ma zajac cala szerokosc zawijania (skok 9 px), a nie 8
    writeFile("W.txt",std::string(60,'W')+"\n"); cfg=Settings{FAM_UNIFONT,0,true,false,false,false,false,1,false,1,1,0,true,0,0,false,0,false,0,true}; applyLayout(); CHECK(openBook("W.txt")); screen=S_READER; page=0; showPage(false);
    int per=maxTextW/fontCharW('W'); int rightMost=-1; for(int x=SCREEN_W-1;x>=0;x--){ bool ink=false; for(int y=0;y<fontLineH()&&!ink;y++) ink=gfxIsInk(x,y+fontTopPad()); if(ink){rightMost=x;break;} }
    printf("  Unifont bold: %d znakow w wierszu (skok %d px), prawy skraj tuszu x=%d, limit x=%d\n",per,fontCharW('W'),rightMost,textX0+maxTextW); CHECK(fontCharW('W')==9 && rightMost>=textX0+(per-1)*9 && rightMost<textX0+maxTextW); closeBook(); }


  SECTION("T41 naglowki pogrubione, skrot do paska stanu z ksiazki");
  { cfg=Settings{FAM_CLASSIC,0,false,true,true,false,true,1,false,1,1,0,true,0,0,false,0,false,0,true}; applyLayout(); prefs.s.erase("last");
    // Classic: naglowek bez pogrubiania (pogrubianie gestej czcionki zlewalo "M" w klocek)
    gfxClear(); gfxText(UI_X,0,"Menu",FONT_SIZE); int plainMenu=inkIn(0,0,60,15); goMenu(); int classicTitle=inkIn(0,0,60,15);
    printf("  naglowek 'Menu' w Classic: zwykly %d px tuszu, w menu %d px (bez pogrubiania)\n",plainMenu,classicTitle); CHECK(classicTitle==plainMenu);
    // Helvetica: pogrubiony naglowek, kursor i pozycje w innej czcionce
    cfg.uiFont=1; menuSel=0; goMenu(); int helvTitle=inkIn(0,0,60,15); CHECK(helvTitle>0 && helvTitle!=plainMenu); int itemInk=inkIn(UI_X+16,16,200,31); CHECK(itemInk>20 && inkIn(UI_X,16,UI_X+7,31)>5);
    printf("  Helvetica: naglowek %d px, pierwsza pozycja %d px, kursor jest\n",helvTitle,itemInk);
    goSettings(); CHECK(inkIn(0,0,120,15)>0); goInfo(); CHECK(inkIn(0,0,40,15)>0); goStats(); CHECK(inkIn(0,0,50,15)>0); cfg.uiFont=0;
    // skrot z ksiazki: HOME przelacza Reading <-> Status line
    CHECK(openBook("B.txt")); screen=S_READER; page=30; showPage(false); int lpBefore=linesPerPage; uint32_t off=pageOff[page]; onKeyReader(K(KEY_BACK,EV_LONG)); CHECK(screen==S_SETTINGS && setGroup==0);
    int tagTop=inkIn(SCREEN_W-TAG_COL_W,0,SCREEN_W-1,TAG_H); CHECK(tagTop>50);                          // etykieta STATUS
    onKeySettings(K(KEY_HOME,EV_SHORT)); CHECK(setGroup==1 && setSel==0 && strncmp(settingsItem(0),"Status line",11)==0); CHECK(inkIn(SCREEN_W-TAG_COL_W,0,SCREEN_W-1,TAG_H)>50);   // etykieta TEXT
    onKeySettings(K(KEY_OK,EV_SHORT)); CHECK(!statusAnyOn() && !statusVisible());                          // wylacz pasek stanu (wszystkie podopcje)
    onKeySettings(K(KEY_HOME,EV_SHORT)); CHECK(setGroup==0 && strncmp(settingsItem(0),"Font",4)==0);   // z powrotem do tekstu
    onKeySettings(K(KEY_HOME,EV_SHORT)); onKeySettings(K(KEY_BACK,EV_SHORT)); CHECK(screen==S_READER && linesPerPage>lpBefore && pageOff[page]<=off && (page+1>=totalPages || off<pageOff[page+1]));
    printf("  z ksiazki: dlugie BACK -> Reading, HOME -> Status line, wylaczenie paska, BACK: %d -> %d wierszy, ten sam fragment\n",lpBefore,linesPerPage); closeBook(); statusSetAll(true); applyLayout();
    // bez skrotu HOME w ustawieniach nic nie zmienia
    setTopSel=0; goSettings(); onKeySettings(K(KEY_OK,EV_SHORT)); CHECK(setGroup==0); onKeySettings(K(KEY_HOME,EV_SHORT)); CHECK(setGroup==0 && screen==S_SETTINGS);
    // strona WWW: glowny przelacznik (JS zaznacza/odznacza podopcje), serwer czyta podopcje
    g_lastBody.clear(); g_args.clear(); handleConfigGet(); CHECK(g_lastBody.find("id='stm'")!=std::string::npos && g_lastBody.find("Show the status line")!=std::string::npos && g_lastBody.find("getElementsByName(n)")!=std::string::npos && g_lastBody.find("'clkbook'")!=std::string::npos);
    g_args.clear(); g_args["pct"]="on"; handleConfigPost(); CHECK(cfg.showPct && !cfg.showPage && statusAnyOn()); g_args.clear(); handleConfigPost(); CHECK(!statusAnyOn() && !statusVisible()); statusSetAll(true); printf("  strona WWW: pole 'Show the status line'\n"); }


  SECTION("T42 czcionka menu: zadna pozycja nie wchodzi pod legende");
  { writeFile("Bardzo_dluga_nazwa_ksiazki_ktora_nie_miesci_sie_w_wierszu_menu_aaaaaaaaaaaaaaaaaa.txt","x"); scanBooks(); prefs.s.erase("last");
    const char* names[4]={"Classic","(brak)","Helv 12","Unifont"}; const int modes[3]={0,2,3};
    for(int mi=0;mi<3;mi++){ int mode=modes[mi]; cfg=Settings{FAM_CLASSIC,1,false,true,true,false,true,1,false,1,1,0,true,0,0,false,0,false,0,true,(uint8_t)mode}; applyLayout(); int bad=0,n=0;
      for(int g=0; g<SET_GROUPS; g++){ for(int i=0;i<SET_GROUP_ITEMS[g];i++){ setGroup=g; setSel=i; screen=S_SETTINGS; drawSettings(); int ov=inkIn(SCREEN_W-TAG_COL_W-1,16,SCREEN_W-TAG_COL_W+3,106); n++; if(ov>0) bad++; } }
      goLibrary(); int libOv=inkIn(SCREEN_W-TAG_COL_W-1,16,SCREEN_W-TAG_COL_W+3,106); menuSel=0; goMenu(); int first=inkIn(UI_X+16,16,200,31);
      printf("  %-8s: %d pozycji ustawien pod legende wchodzi: %d, biblioteka z dluga nazwa: %d, menu - pierwsza pozycja %d px\n",names[mode],n,bad,libOv,first); CHECK(bad==0 && libOv==0 && first>20); setGroup=-1; }
    // rozne tryby daja rozne rysunki
    int ink[4]={0,0,0,0}; for(int mi=0;mi<3;mi++){ cfg.uiFont=modes[mi]; menuSel=0; goMenu(); ink[modes[mi]]=inkIn(UI_X+16,16,200,31); } CHECK(ink[0]!=ink[2] && ink[2]!=ink[3] && ink[0]!=ink[3]);
    // strona WWW i zapis
    cfg.uiFont=2; g_lastBody.clear(); g_args.clear(); handleConfigGet(); CHECK(g_lastBody.find("name='uifont'")!=std::string::npos && g_lastBody.find("Helvetica 12 (thin)")!=std::string::npos && g_lastBody.find("Unifont (pixel, wide)")!=std::string::npos);
    g_args.clear(); g_args["uifont"]="3"; handleConfigPost(); CHECK(cfg.uiFont==3 && prefs.u["cfg_uf"]==3); g_args["uifont"]="0"; handleConfigPost(); CHECK(cfg.uiFont==0); g_args["uifont"]="9"; handleConfigPost(); CHECK(cfg.uiFont==0); g_args["uifont"]="1"; handleConfigPost(); CHECK(cfg.uiFont==2); g_args["uifont"]="2"; handleConfigPost(); CHECK(cfg.uiFont==2);
    CHECK(g_lastBody.find("Helvetica 14")==std::string::npos); g_lastBody.clear(); g_args.clear(); handleConfigGet(); CHECK(g_lastBody.find("name='appower'")!=std::string::npos && g_lastBody.find("Low (best when")!=std::string::npos); g_args.clear(); g_args["appower"]="2"; handleConfigPost(); CHECK(cfg.apPower==2 && prefs.u["cfg_ap"]==2); g_args["appower"]="7"; handleConfigPost(); CHECK(cfg.apPower==2); g_args["appower"]="0"; handleConfigPost(); CHECK(cfg.apPower==0); cfg.apPower=1;
    // zapisane dawne "Helvetica 14" (1) jest przeksztalcane na Helvetica 12; nieprawidlowe wartosci naprawiane
    cfg.uiFont=1; applyLayout(); CHECK(cfg.uiFont==2); cfg.uiFont=9; cfg.apPower=9; applyLayout(); CHECK(cfg.uiFont==0 && cfg.apPower==1); cfg.uiFont=2;
    printf("  strona WWW: Menu font (4 opcje, bledne wartosci ignorowane)\n"); cfg.uiFont=2; }

  printf("\n==== TESTY MENU, SKROTOW I ODSTEPOW: %d OK, %d bledow ====\n",passN,failN); return failN?1:0; }
