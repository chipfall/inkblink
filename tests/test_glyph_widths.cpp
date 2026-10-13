#include "inkblink.ino"
static int passN=0, failN=0;
#define CHECK(c) do{ if(c){passN++;} else {failN++; printf("  FAIL line %d: %s\n",__LINE__,#c);} }while(0)
static void writeFile(const char* name,const std::string& d){ std::string p=std::string("build/run/fs/")+name; FILE* f=fopen(p.c_str(),"wb"); fwrite(d.data(),1,d.size(),f); fclose(f); }
// "prawdziwa" szerokosc narysowanego wiersza (przez getUTF8Width calego napisu), tak jak ja liczy biblioteka
static int trueWidth(const char* s,int len){ char nb[256]; int j=0,i=0; while(i<len && j<250){ uint32_t cp; i+=gfxU8Decode(s+i,len-i,cp); if(cp==0) continue; j+=utf8Encode(fontNorm(cp),nb+j);} nb[j]=0; return u8g2.getUTF8Width(nb)+(fSynthBold?(int)strlen(nb)>0?1:0:0); }
int main(){
  for(int i=0;i<64;i++) g_pin[i]=1; g_ms=100000; system("rm -rf build/run/fs/*; mkdir -p build/run/fs");
  fsOk=true; prefs.begin("reader",false); fontInit(); loadSettings();
  std::string t="\xEF\xBB\xBF"; const char* w[]={"swojską.","Stoły","poprze-suwano,","czarodzieje","i","czarodziejki","poznosili","do","sali","zdobyte","gdzieś","fotele,","krzesła","porozsiadali","się","na","nich","Filippa","Eilhart.","Zdenerwowany","Sabrina,","znajdź","prędko","Mark","Wędrowiec","szedł","długo","przez","ciemny","las","konstantynopolitańczykowianeczka","ją","tu","wziął?","— Skąd","ten","iii","lll","mmm","WWW","..."};
  srand(3); int nw=sizeof(w)/sizeof(w[0]); for(int p=0;p<400;p++){ int k=15+rand()%60; for(int i=0;i<k;i++){ t+=w[rand()%nw]; t+=(i+1<k)?" ":""; } t+="\r\n\r\n"; } writeFile("W.txt",t);
  printf("== T23 szerokosc wierszy czcionek U8g2 liczona jak biblioteka (nie moze przekraczac maxTextW) ==\n");
  struct C{int fam,si;bool bold;} cs[]={{1,0,false},{1,1,false},{1,2,false},{1,1,true},{1,2,true},{1,0,true},{2,0,false},{2,0,true}};
  for(auto&c:cs){
    cfg=Settings{(uint8_t)c.fam,(uint8_t)c.si,c.bold,true,true,false,true,1,false,1,1,0,true}; applyLayout(); CHECK(openBook("W.txt"));
    int worst=0,over=0,lines=0; char buf[PAGE_BUF+1];
    for(int p=0;p<totalPages;p++){ book.seek(pageOff[p]); int n=book.read((uint8_t*)buf,PAGE_BUF); if(n<0)n=0; buf[n]=0; sanitizeBuf(buf,n); int pos=0;
      for(int r=0;r<linesPerPage;r++){ int adv; int len=lineLength(buf,pos,adv); int tw=trueWidth(buf+pos,len); if(tw>worst)worst=tw; if(tw>maxTextW) over++; lines++; pos+=adv; if(buf[pos]==0)break; } }
    printf("  %-9s %2dpx bold=%d: najszersza linia %3d px przy limicie %d, za szerokich: %d z %d\n",fontFamilyName(c.fam),fontSizeValue(c.fam,c.si),c.bold,worst,maxTextW,over,lines);
    CHECK(over==0); closeBook(); }
  printf("\n==== TEST SZEROKOSCI: %d OK, %d bledow ====\n",passN,failN); return failN?1:0; }
