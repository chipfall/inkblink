#include "trace.h"
#include "inkblink.ino"
#include <set>
static int passN=0, failN=0;
#define CHECK(c) do{ if(c){passN++;} else {failN++; printf("  FAIL line %d: %s\n",__LINE__,#c);} }while(0)
static void writeFile(const char* name,const std::string& d){ std::string p=std::string("build/run/fs/")+name; FILE* f=fopen(p.c_str(),"wb"); fwrite(d.data(),1,d.size(),f); fclose(f); }
static std::vector<int> regs(){ std::vector<int> r; for(auto&c:g_trace) r.push_back(c.reg); return r; }
static const TraceCmd* find(int reg){ for(auto&c:g_trace) if(c.reg==reg) return &c; return nullptr; }
static std::string seq(){ std::string s; for(auto&c:g_trace){ char b[16]; snprintf(b,sizeof(b),"%02X(%zu) ",c.reg,c.data.size()); s+=b; } return s; }
int main(){
  for(int i=0;i<64;i++) g_pin[i]=1; g_ms=100000; system("rm -rf build/run/fs/*; mkdir -p build/run/fs");
  fsOk=true; prefs.begin("reader",false); fontInit(); loadSettings(); statsLoadTotals();
  printf("== T20 szybkie odswiezanie: polecenia wysylane do ekranu ==\n");
  CHECK(!gfxFastReady());
  gfxClear(); gfxText(10,10,"Pierwszy obraz",16); g_trace.clear(); gfxPresent();
  printf("  pelne:  %s\n",seq().c_str());
  CHECK(!find(0x10)); CHECK(find(0x13) && find(0x13)->data.size()==ALLSCREEN_BYTES); CHECK(find(0x17)); CHECK(gfxFastReady());
  CHECK(memcmp(find(0x13)->data.data(),ImageBW,ALLSCREEN_BYTES)==0);
  // pelne odswiezenie laduje przebieg GC
  { const TraceCmd* l=find(0x20); CHECK(l && l->data.size()==56 && memcmp(l->data.data(),lut_R20_GC,56)==0); }
  std::vector<uint8_t> first(ImageBW,ImageBW+ALLSCREEN_BYTES);
  gfxClear(); gfxText(10,40,"Drugi obraz",16); g_trace.clear(); gfxPresentFast();
  printf("  szybkie: %s\n",seq().c_str());
  CHECK(find(0x10) && find(0x10)->data.size()==ALLSCREEN_BYTES && find(0x13) && find(0x13)->data.size()==ALLSCREEN_BYTES);
  CHECK(memcmp(find(0x10)->data.data(),first.data(),ALLSCREEN_BYTES)==0);     // STARE dane = poprzedni obraz
  CHECK(memcmp(find(0x13)->data.data(),ImageBW,ALLSCREEN_BYTES)==0);          // NOWE dane = biezacy bufor
  { const TraceCmd* l=find(0x20); CHECK(l && memcmp(l->data.data(),lut_R20_DU,56)==0 && memcmp(l->data.data(),lut_R20_GC,56)!=0); }   // przebieg DU
  { auto r=regs(); size_t i10=0,i13=0,i17=0,i20=0; for(size_t i=0;i<r.size();i++){ if(r[i]==0x10)i10=i; if(r[i]==0x13)i13=i; if(r[i]==0x17)i17=i; if(r[i]==0x20)i20=i; } CHECK(i10<i13 && i13<i20 && i20<i17); printf("  kolejnosc: stare dane -> nowe dane -> przebieg DU -> odswiezenie\n"); }
  // po szybkim odswiezeniu 'stary obraz' to ten nowy
  std::vector<uint8_t> second(ImageBW,ImageBW+ALLSCREEN_BYTES); gfxClear(); gfxText(10,70,"Trzeci",16); g_trace.clear(); gfxPresentFast(); CHECK(memcmp(find(0x10)->data.data(),second.data(),ALLSCREEN_BYTES)==0); printf("  kolejne szybkie odswiezenie dostaje poprawny poprzedni obraz\n");
  // wszystkie polecenia, ktorych nie znamy z oryginalnego sterownika, nie wystepuja
  { std::set<int> known={0x50,0x10,0x13,0x20,0x21,0x22,0x23,0x24,0x17}; bool ok=true; for(auto&c:g_trace) if(!known.count(c.reg)) ok=false; CHECK(ok); printf("  uzyte tylko polecenia z oryginalnego sterownika\n"); }

  printf("\n== T20b logika w czytniku: co ile stron pelne odswiezenie ==\n");
  writeFile("T.txt",std::string(30000,'x')+" ");
  { std::string t; for(int i=0;i<600;i++) t+="Zazolc gesla jazn pchnac w te lodz jeza lub osm skrzyn fig. "; writeFile("T.txt",t); }
  cfg=Settings{FAM_CLASSIC,1,false,true,true,false,true,1,true,0,1,0,true}; applyLayout();   // fastFlip, co 5 stron
  CHECK(openBook("T.txt")); screen=S_READER; g_trace.clear(); showPage(false); statsTouch();
  std::string pattern; pattern+= find(0x10)?'f':'F';
  for(int i=0;i<12;i++){ g_trace.clear(); g_ms+=10000; turnPages(1); pattern+= find(0x10)?'f':'F'; }
  printf("  wzorzec (F=pelne, f=szybkie), co 5: %s\n",pattern.c_str()); CHECK(pattern=="FffffFffffFff");
  // skok do strony i wejscie do czytnika zawsze pelne
  cfg.fullEvery=1; applyLayout(); fastLeft=0; closeBook(); CHECK(openBook("T.txt")); g_trace.clear(); screen=S_READER; showPage(false); CHECK(!find(0x10));
  g_trace.clear(); g_ms+=10000; turnPages(1); CHECK(find(0x10)); g_trace.clear(); gotoVal=30; KeyEvent hm{KEY_HOME,EV_SHORT,0}; screen=S_GOTO; onKeyGoto(hm); CHECK(!find(0x10)); printf("  wejscie do ksiazki i skok do strony: zawsze pelne odswiezenie\n");
  // wylaczone w ustawieniach -> zawsze pelne
  cfg.fastFlip=false; for(int i=0;i<6;i++){ g_trace.clear(); g_ms+=10000; turnPages(1); CHECK(!find(0x10)); } printf("  Page flip: Full -> brak szybkich odswiezen\n");
  // menu i ustawienia zawsze pelne
  cfg.fastFlip=true; g_trace.clear(); drawMenu(); CHECK(!find(0x10)); g_trace.clear(); drawSettings(); CHECK(!find(0x10)); printf("  menu i ustawienia zawsze pelne (bezpieczne wyjscie, gdyby tryb szybki pokazywal smieci)\n");
  closeBook();
  printf("\n==== TEST SZYBKIEGO ODSWIEZANIA: %d OK, %d bledow ====\n",passN,failN); return failN?1:0; }
