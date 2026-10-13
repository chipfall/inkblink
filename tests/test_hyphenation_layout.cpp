#include "inkblink.ino"
static int passN=0, failN=0;
#define CHECK(c) do{ if(c){passN++;} else {failN++; printf("  FAIL line %d: %s\n",__LINE__,#c);} }while(0)
static void writeFile(const char* name,const std::string& d){ std::string p=std::string("build/run/fs/")+name; FILE* f=fopen(p.c_str(),"wb"); fwrite(d.data(),1,d.size(),f); fclose(f); }
static void dumpRaw(const char* path){ FILE* f=fopen(path,"wb"); for(int y=0;y<SCREEN_H;y++) for(int x=0;x<SCREEN_W;x++) fputc(gfxIsInk(x,y)?1:0,f); fclose(f); }
static bool isLetterCp(const char* s,int len){ uint32_t cp; gfxU8Decode(s,len,cp); return hyIsLetter(hyLower(cp)); }
static int maxRun[8];
static int trueW(const char* s,int len,bool hy){ if(fFam==FAM_CLASSIC) return textWidthPx(s,len)+(hy?fontCharW('-'):0); char nb[300]; int j=0,i=0; while(i<len&&j<280){ uint32_t cp; i+=gfxU8Decode(s+i,len-i,cp); if(cp==0)continue; j+=utf8Encode(fontNorm(cp),nb+j);} if(hy) nb[j++]='-'; nb[j]=0; return u8g2.getUTF8Width(nb)+(fSynthBold?1:0); }
int main(){
  for(int i=0;i<64;i++) g_pin[i]=1; g_ms=100000; system("rm -rf build/run/fs/*; mkdir -p build/run/png");
  fsOk=true; prefs.begin("reader",false); fontInit(); loadSettings();
  const char* w[]={"swojską.","Stoły","poprze-suwano,","czarodzieje","i","czarodziejki","poznosili","do","sali","zdobyte","gdzieś","fotele,","krzesła","porozsiadali","się","na","nich","Filippa","Eilhart.","Zdenerwowany","Sabrina,","znajdź","prędko","Wędrowiec","szedł","długo","przez","ciemny","las","niebezpieczny","przyjaciel","powietrze","wykształcenie","— Skąd","ten","wziął?","Nilfgaardczycy","(przekupieni)","„zamiary”","ABC","USA-owski","2024-rok","kamień-węgielny","Redańczyków"};
  std::string t; srand(11); int nw=sizeof(w)/sizeof(w[0]); for(int p=0;p<500;p++){ int k=12+rand()%50; for(int i=0;i<k;i++){ t+=w[rand()%nw]; if(i+1<k) t+=" "; } t+="\r\n"; } writeFile("H.txt",t);
  printf("== T29 przenoszenie wyrazow: wszystkie rodziny ==\n");
  struct C{int fam,si,gap;} cs[]={{0,1,0},{0,0,0},{1,1,0},{1,3,0},{3,1,0},{3,2,1},{2,0,0}};
  for(auto&c:cs){
    int pagesOff=0,pagesOn=0,hyLines=0,lines=0,over=0,badFrag=0,litHy=0,wrongHy=0;
    for(int hy=0;hy<2;hy++){
      cfg=Settings{(uint8_t)c.fam,(uint8_t)c.si,false,true,true,false,true,1,false,1,1,0,true,0,(uint8_t)c.gap,(bool)hy,0,false,0}; applyLayout(); CHECK(openBook("H.txt"));
      if(hy) pagesOn=totalPages; else pagesOff=totalPages;
      char buf[PAGE_BUF+1]; int run=0;  
      for(int p=0;p<totalPages;p++){ book.seek(pageOff[p]); int n=book.read((uint8_t*)buf,PAGE_BUF); if(n<0)n=0; buf[n]=0; sanitizeBuf(buf,n); int pos=0; hyRun=0; run=0;
        for(int r=0;r<linesPerPage;r++){ int adv; bool h; int len=layoutLine(buf,pos,adv,h); lines++; if(hy){ if(h){ run++; if(run>maxRun[0]) maxRun[0]=run; if(run>=3) maxRun[1]++; } else run=0; } if(trueW(buf+pos,len,h)>maxTextW) over++;
          if(!hy && h) wrongHy++;
          if(h){ hyLines++;
            // fragmenty po obu stronach: co najmniej 3 litery
            int a=pos+len; int lf=0; while(a>pos){ int s=a-1; while(s>pos && ((uint8_t)buf[s]&0xC0)==0x80) s--; if(!isLetterCp(buf+s,a-s)) break; lf++; a=s; }
            int rf=0; int q=pos+adv; while(buf[q]){ int cb=charBytes((uint8_t)buf[q]); if(!isLetterCp(buf+q,cb)) break; rf++; q+=cb; }
            if(lf<2||rf<2){ badFrag++; static int shown=0; if(shown++<8){ char l1[64],l2[64]; int a1=len>30?len-30:0; snprintf(l1,sizeof l1,"%.*s",len-a1,buf+pos+a1); snprintf(l2,sizeof l2,"%.12s",buf+pos+adv); printf("    [lf=%d rf=%d] ...%s-|%s\n",lf,rf,l1,l2);} }
            if(buf[pos+len]==' '||buf[pos+len]=='\n'||buf[pos+len]=='\r') wrongHy++;           // przerwa musi byc w srodku wyrazu
          } else if(len>0 && buf[pos+len-1]=='-' && isLetterCp(buf+pos+adv,charBytes((uint8_t)buf[pos+adv]))) litHy++;
          pos+=adv; if(buf[pos]==0)break; } }
      closeBook(); }
    printf("  %-9s %2dpx gap=%d: stron bez/z dzieleniem %d/%d, wierszy z myslnikiem %d z %d (%.1f%%), za szerokich %d, krotkie fragmenty %d, bledne %d, lamanie po '-' z tekstu %d\n",fontFamilyName(c.fam),fontSizeValue(c.fam,c.si),c.gap,pagesOff,pagesOn,hyLines,lines/2,100.0*hyLines/(lines/2.0),over,badFrag,wrongHy,litHy);
    CHECK(over==0 && badFrag==0 && wrongHy==0 && hyLines>0 && pagesOn<=pagesOff); }
  // wizualna kontrola: Classic 16 z dzieleniem
  cfg=Settings{0,1,false,false,false,false,false,1,false,1,1,0,true,0,0,true,0,false,0}; applyLayout(); openBook("H.txt"); screen=S_READER; page=7; showPage(false); dumpRaw("build/run/png/hy_classic.raw"); closeBook();
  cfg=Settings{0,1,false,false,false,false,false,1,false,1,1,0,true,0,0,false,0,false,0}; applyLayout(); openBook("H.txt"); screen=S_READER; page=7; showPage(false); dumpRaw("build/run/png/hy_classic_off.raw"); closeBook();
    printf("  najdluzszy ciag wierszy z myslnikiem na jednej stronie: %d (limit %d)\n",maxRun[0],HY_MAX_RUN); CHECK(maxRun[0]<=HY_MAX_RUN);
  printf("\n==== TEST DZIELENIA: %d OK, %d bledow ====\n",passN,failN); return failN?1:0; }
