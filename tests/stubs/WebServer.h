#pragma once
#include "Arduino.h"
#define CONTENT_LENGTH_UNKNOWN ((size_t)-1)

enum HTTPMethod { HTTP_GET, HTTP_POST };
enum HTTPAuthMethod { BASIC_AUTH, DIGEST_AUTH };
enum HTTPUploadStatus { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };
struct HTTPUpload { HTTPUploadStatus status; String filename; uint8_t* buf; size_t currentSize; };
extern std::string g_uri; extern bool g_authOk; extern int g_lastCode; extern std::string g_lastBody, g_lastLoc; extern int g_authRequested;
extern std::map<std::string,std::string> g_args; extern HTTPUpload g_up;
class WebServer { public: WebServer(int){}
  typedef std::function<void()> H; void on(const char*,HTTPMethod,H){} void on(const char*,HTTPMethod,H,H){}
  void onNotFound(H){} String uri(){ return String(g_uri.c_str()); } void begin(){} void stop(){} void handleClient(){}
  bool authenticate(const char*,const char*){ return g_authOk; }
  void requestAuthentication(HTTPAuthMethod=BASIC_AUTH,const char* =NULL,const String& =String("")){ g_authRequested++; g_lastCode=401; }
  void send(int code,const char* =NULL,const String& body=String("")){ g_lastCode=code; g_lastBody=body.s; }
  void setContentLength(size_t){}
  void sendContent(const String& s){ g_lastBody+=s.s; }
  void sendContent_P(const char* s){ g_lastBody+=s; }
  void sendHeader(const char* n,const char* v){ if(!strcmp(n,"Location")) g_lastLoc=v; }
  String arg(const char* n){ return String(g_args.count(n)?g_args[n].c_str():""); } bool hasArg(const char* n){ return g_args.count(n)>0; }
  HTTPUpload& upload(){ return g_up; } };
