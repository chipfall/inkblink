#pragma once
#include "Arduino.h"
class File { public: FILE* fp=nullptr; DIR* dir=nullptr; std::string path; bool isdir=false; std::string nm;
  operator bool() const { return fp||dir; }
  size_t size(){ long c=ftell(fp); fseek(fp,0,SEEK_END); long e=ftell(fp); fseek(fp,c,SEEK_SET); return e; }
  int read(uint8_t* b,size_t n){ return (int)fread(b,1,n,fp);} size_t write(const uint8_t* b,size_t n){return fwrite(b,1,n,fp);}
  bool seek(uint32_t p){ return fseek(fp,p,SEEK_SET)==0;} void close(){ if(fp)fclose(fp); if(dir)closedir(dir); fp=nullptr; dir=nullptr; }
  bool isDirectory(){return isdir;} const char* name(){return nm.c_str();}
  File openNextFile(){ File f; if(!dir) return f; dirent* e; while((e=readdir(dir))){ if(e->d_name[0]=='.') continue; std::string p=path+"/"+e->d_name; f.path=p; f.nm=e->d_name; struct stat st; stat(p.c_str(),&st); f.isdir=S_ISDIR(st.st_mode); f.fp=f.isdir?nullptr:fopen(p.c_str(),"rb"); if(f.isdir) f.dir=opendir(p.c_str()); return f;} return f; } };
struct FSC { std::string base="build/run/fs";
  bool begin(bool){return true;} void end(){}
  File open(const char* p,const char* m="r"){ File f; std::string full=base+p; if(strcmp(p,"/")==0){ f.dir=opendir(base.c_str()); f.path=base; f.isdir=true; return f;} f.path=full; f.nm=(strrchr(p,'/')?strrchr(p,'/')+1:p); f.fp=fopen(full.c_str(), strcmp(m,"w")==0?"wb":"rb"); return f; }
  bool exists(const char* p){ struct stat st; return stat((base+p).c_str(),&st)==0; } bool remove(const char* p){ return ::remove((base+p).c_str())==0; }
  bool rename(const char* a,const char* b){ return ::rename((base+a).c_str(),(base+b).c_str())==0; }
  size_t totalBytes(){return 6*1024*1024;} size_t usedBytes(){return 1024*1024;} };
extern FSC LittleFS;
