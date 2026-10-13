#include "inkblink.ino"
#include <fstream>
#include <sstream>
static int passN=0, failN=0;
#define CHECK(c) do{ if(c){passN++;} else {failN++; printf("  FAIL line %d: %s\n",__LINE__,#c);} }while(0)
#define SECTION(n) printf("\n== %s ==\n",n)
static void dumpRaw(const char* path){ FILE* f=fopen(path,"wb"); for(int y=0;y<SCREEN_H;y++) for(int x=0;x<SCREEN_W;x++) fputc(gfxIsInk(x,y)?1:0,f); fclose(f); }
static int inkIn(int x0,int y0,int x1,int y1){ int n=0; for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++) if(gfxIsInk(x,y)) n++; return n; }
static std::string slurp(const char* p){ std::ifstream f(p,std::ios::binary); std::stringstream ss; ss<<f.rdbuf(); return ss.str(); }
static std::string fsRead(const char* name){ return slurp((std::string("build/run/fs/")+name).c_str()); }
static bool fsHas(const char* name){ std::ifstream f(std::string("build/run/fs/")+name); return f.good(); }
static void writeFile(const char* name,const std::string& d){ std::string p=std::string("build/run/fs/")+name; FILE* f=fopen(p.c_str(),"wb"); fwrite(d.data(),1,d.size(),f); fclose(f); }
static void upload(const char* fn,const std::string& data){
  g_up.status=UPLOAD_FILE_START; g_up.filename=String(fn); g_up.buf=nullptr; g_up.currentSize=0; handleUploadData();
  for(size_t o=0;o<data.size();o+=4096){ size_t n=std::min<size_t>(4096,data.size()-o); g_up.status=UPLOAD_FILE_WRITE; g_up.buf=(uint8_t*)data.data()+o; g_up.currentSize=n; handleUploadData(); }
  g_up.status=UPLOAD_FILE_END; g_up.buf=nullptr; g_up.currentSize=0; handleUploadData(); }
static bool validUtf8(const char* s){ for(int i=0;s[i];){ uint8_t c=s[i]; int n=c<0x80?1:(c&0xE0)==0xC0?2:(c&0xF0)==0xE0?3:(c&0xF8)==0xF0?4:0; if(!n) return false; for(int k=1;k<n;k++) if(((uint8_t)s[i+k]&0xC0)!=0x80) return false; i+=n; } return true; }
int main(){
  for(int i=0;i<64;i++) g_pin[i]=1; g_ms=100000; system("rm -rf build/run/fs/*; mkdir -p build/run/png");
  fsOk=true; prefs.begin("reader",false); fontInit(); loadSettings(); statsLoadTotals(); clockNowHook=[]()->time_t{return 0;};
  std::string txt=slurp("build/fixtures/Dluga_podroz.txt"), toc=slurp("build/fixtures/Dluga_podroz.toc");
  CHECK(txt.size()>200000 && toc.size()>1000);

  SECTION("T33 spis tresci: nazwy, upload, kasowanie");
  { char o[NAME_LEN]; CHECK(makeTocName("Test Autor - Długa podróż.toc",o) && strcmp(o,"Test_Autor_-_Dluga_podroz.toc")==0); CHECK(!makeTocName("a.txt",o) && !makeTocName("a.epub",o) && !makeTocName(".toc",o)); CHECK(makeBookName("Test Autor - Długa podróż.txt",o) && strcmp(o,"Test_Autor_-_Dluga_podroz.txt")==0 && !makeBookName("x.toc",o));
    char tp[NAME_LEN+3]; tocPathFor("Test_Autor_-_Dluga_podroz.txt",tp,sizeof tp); CHECK(strcmp(tp,"/Test_Autor_-_Dluga_podroz.toc")==0); printf("  nazwy: ksiazka i spis maja ten sam trzon po transliteracji (%s)\n",tp);
    lastUploaded[0]=0; uploadDone=false; upload("Test Autor - Długa podróż.txt",txt); CHECK(uploadOk && uploadErr.length()==0 && fsHas("Test_Autor_-_Dluga_podroz.txt")); CHECK(strcmp(lastUploaded,"Test_Autor_-_Dluga_podroz.txt")==0 && uploadDone);
    uploadDone=false; upload("Test Autor - Długa podróż.toc",toc); CHECK(uploadOk && uploadErr.length()==0 && fsHas("Test_Autor_-_Dluga_podroz.toc") && fsRead("Test_Autor_-_Dluga_podroz.toc")==toc);
    CHECK(!uploadDone && strcmp(lastUploaded,"Test_Autor_-_Dluga_podroz.txt")==0); printf("  upload .txt (%zu B) i .toc (%zu B) zapisane; .toc nie zmienia 'Got:' i nie odswieza ekranu\n",txt.size(),toc.size());
    scanBooks(); CHECK(bookCount==1 && strcmp(bookNames[0],"Test_Autor_-_Dluga_podroz.txt")==0); printf("  biblioteka widzi 1 ksiazke (plik .toc nie jest ksiazka)\n");
    // za duzy spis i zly typ
    upload("duzy.toc",std::string(30000,'x')); CHECK(!uploadOk && uploadErr=="Chapter list too large"); CHECK(!fsHas("duzy.toc") && !fsHas("upload.tmp")); upload("a.pdf","x"); CHECK(uploadErr=="Only .txt files are accepted"); printf("  za duzy spis i obcy typ pliku odrzucone, brak smieci\n"); }

  SECTION("T34 spis tresci: wczytanie i rozdzialy w ksiazce");
  { cfg=Settings{FAM_CLASSIC,1,false,true,true,false,true,1,false,1,1,0,true,0,0,false,0,false,0}; applyLayout(); CHECK(openBook("Test_Autor_-_Dluga_podroz.txt")); CHECK(chCount==40);
    printf("  %d stron, %d rozdzialow; pierwszy: '%s' @%u, ostatni: '%s' @%u\n",totalPages,chCount,chTitle[0],(unsigned)chOff[0],chTitle[chCount-1],(unsigned)chOff[chCount-1]);
    bool asc=true,utf=true,points=true; std::string T=txt; for(int i=0;i<chCount;i++){ if(i&&chOff[i]<=chOff[i-1]) asc=false; if(!validUtf8(chTitle[i])||strlen(chTitle[i])>31) utf=false; if(T.compare(chOff[i],10,"Rozdział ")!=0) points=false; }
    CHECK(asc && utf && points); printf("  offsety rosnace, tytuly poprawnym UTF-8 (<=31 B), kazdy offset wskazuje na 'Rozdzial N'\n");
    CHECK(chapterOfOffset(0)==0 && chapterOfOffset(chOff[5])==5 && chapterOfOffset(chOff[5]+10)==5 && chapterOfOffset(chOff[5]-1)==4 && chapterOfOffset(txt.size())==39);
    // zepsute wiersze w spisie
    closeBook(); writeFile("Maly.txt",std::string(2000,'a')+"\n"+std::string(3000,'b')); std::string bad="abc\n100 no tab\n50\tPierwszy\n40\tmalejacy\n\n1500\tDrugi rozdzial z bardzo dlugim tytulem ktory sie nie miesci w 31 bajtach\r\n99999999\tza daleko\n2500\tTrzeci bez konca linii"; writeFile("Maly.toc",bad);
    CHECK(openBook("Maly.txt")); printf("  zepsuty spis: wczytano %d rozdzialy: ",chCount); for(int i=0;i<chCount;i++) printf("[%u '%s'] ",(unsigned)chOff[i],chTitle[i]); printf("\n");
    CHECK(chCount==3 && chOff[0]==50 && chOff[1]==1500 && chOff[2]==2500 && strcmp(chTitle[2],"Trzeci bez konca linii")==0 && strlen(chTitle[1])==31); closeBook(); CHECK(chCount==0);
    writeFile("Bez.txt","Ksiazka bez spisu."); CHECK(openBook("Bez.txt") && chCount==0); closeBook(); }

  SECTION("T35 ekran 'Go to page': krok 'chapter'");
  { cfg=Settings{FAM_CLASSIC,1,false,true,true,false,true,1,false,1,1,0,true,0,0,false,0,false,0}; applyLayout(); CHECK(openBook("Test_Autor_-_Dluga_podroz.txt")); screen=S_READER; page=0; showPage(false);
    KeyEvent ok{KEY_OK,EV_SHORT,0}, dn{KEY_DOWN,EV_PRESS,0}, up{KEY_UP,EV_PRESS,0}, go{KEY_HOME,EV_SHORT,0}, bk{KEY_BACK,EV_SHORT,0};
    gotoStepIdx=0; goGoto(); CHECK(screen==S_GOTO && gotoVal==1); dumpRaw("build/run/png/goto_numeric.raw"); int chapInk=inkIn(4,92,200,106); CHECK(chapInk>20);
    int seq[6]; for(int i=0;i<6;i++){ seq[i]=gotoStepIdx; onKeyGoto(ok); } printf("  cykl krokow po OK: %d %d %d %d %d %d\n",seq[0],seq[1],seq[2],seq[3],seq[4],seq[5]); CHECK(seq[0]==0&&seq[1]==1&&seq[2]==2&&seq[3]==3&&seq[4]==4&&seq[5]==0);
    gotoStepIdx=GOTO_CHAPTER; gotoVal=1; drawGoto(); dumpRaw("build/run/png/goto_chapter.raw");
    onKeyGoto(dn); int p2=gotoVal; CHECK(pageOff[p2-1]<=chOff[1] && (p2>=totalPages || pageOff[p2]>chOff[1])); printf("  DOWN z str.1: strona %d zawiera poczatek rozdzialu 2 (offset %u)\n",p2,(unsigned)chOff[1]);
    onKeyGoto(dn); CHECK(chapterOfPage(gotoVal-1)==2); onKeyGoto(dn); int p4=gotoVal; CHECK(chapterOfPage(p4-1)==3 && p4==chapterStartPage(3)); printf("  kolejne DOWN: rozdzial 3, 4 (strona %d) - bez zawieszania sie na stronie z polowicznym poczatkiem\n",p4);
    // UP: jestesmy na poczatku rozdzialu -> poprzedni; w srodku rozdzialu -> poczatek biezacego
    int mid=p4+3; gotoVal=mid; onKeyGoto(up); CHECK(gotoVal==chapterStartPage(chapterOfPage(mid-1))); int startCur=gotoVal; onKeyGoto(up); CHECK(gotoVal<startCur); printf("  UP: z polowy rozdzialu na jego poczatek (strona %d), potem do poprzedniego (strona %d)\n",startCur,gotoVal);
    gotoVal=1; onKeyGoto(up); CHECK(gotoVal==chapterStartPage(chCount-1)); printf("  UP z poczatku ksiazki: na ostatni rozdzial (strona %d)\n",gotoVal);
    onKeyGoto(dn); CHECK(gotoVal==1); printf("  DOWN z ostatniego rozdzialu: na pierwszy\n");
    // zatwierdzenie: czytnik pokazuje strone z rozdzialem 7
    gotoVal=1; for(int i=0;i<6;i++) onKeyGoto(dn); onKeyGoto(go); CHECK(screen==S_READER); CHECK(pageOff[page]<=chOff[6] && (page+1>=totalPages || pageOff[page+1]>chOff[6])); printf("  HOME: czytnik na stronie %d (zawiera 'Rozdzial 7')\n",page+1);
    // nazwa rozdzialu na ekranie zmienia sie z celem
    gotoStepIdx=GOTO_CHAPTER; gotoVal=1; drawGoto(); int a=inkIn(4,92,200,106); gotoVal=findPage(chOff[30])+1; drawGoto(); int b=inkIn(4,92,200,106); printf("  linia z nazwa rozdzialu: tusz %d / %d px\n",a,b); CHECK(a>20&&b>20);
    // ksiazka bez spisu: brak kroku i linii z rozdzialem
    closeBook(); CHECK(openBook("Bez.txt")); screen=S_READER; showPage(false); gotoStepIdx=GOTO_CHAPTER; goGoto(); CHECK(gotoStepIdx==0); int none=inkIn(4,92,200,106); CHECK(none==0);
    int s2[5]; for(int i=0;i<5;i++){ s2[i]=gotoStepIdx; onKeyGoto(ok);} printf("  bez spisu cykl krokow: %d %d %d %d %d (bez 'chapter')\n",s2[0],s2[1],s2[2],s2[3],s2[4]); for(int i=0;i<5;i++) CHECK(s2[i]!=GOTO_CHAPTER); closeBook(); }

  SECTION("T36 kasowanie ksiazki usuwa spis i indeks");
  { CHECK(fsHas("Test_Autor_-_Dluga_podroz.toc")); g_args.clear(); g_args["f"]="Test_Autor_-_Dluga_podroz.txt"; handleDelete(); CHECK(g_lastCode==200 && !fsHas("Test_Autor_-_Dluga_podroz.txt") && !fsHas("Test_Autor_-_Dluga_podroz.toc")); printf("  ksiazka i jej spis tresci skasowane razem\n");
    // strona glowna oznacza ksiazki ze spisem
    upload("Ze spisem.txt",std::string(2000,'a')); upload("Ze spisem.toc","0\tStart\n1000\tMiddle\n"); upload("Bez spisu.txt","bbb"); scanBooks(); g_args.clear(); g_lastBody.clear(); handleRoot();
    printf("  [debug] code=%d body=%zu B, zawiera 'Add books': %d\n",g_lastCode,g_lastBody.size(),(int)(g_lastBody.find("Add books")!=std::string::npos)); size_t i1=g_lastBody.find("Ze spisem"),i2=g_lastBody.find("Bez spisu"); CHECK(i1!=std::string::npos&&i2!=std::string::npos); std::string r1=g_lastBody.substr(i1,g_lastBody.find("</li>",i1)-i1), r2=g_lastBody.substr(i2,g_lastBody.find("</li>",i2)-i2);
    CHECK(r1.find("chapters")!=std::string::npos && r2.find("chapters")==std::string::npos); for(const char* w:{".epub","EPUB","window.epubToBook","DecompressionStream","chapter list"}){ bool f=g_lastBody.find(w)!=std::string::npos; CHECK(f); if(!f) printf("    brak w stronie: %s\n",w);} printf("  strona glowna: znacznik 'chapters' tylko przy ksiazce ze spisem; skrypt EPUB i opis w stronie\n"); }

  printf("\n==== TESTY SPISU TRESCI: %d OK, %d bledow ====\n",passN,failN); return failN?1:0; }
