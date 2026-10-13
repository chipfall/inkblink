#pragma once
#include "Arduino.h"
extern bool g_dnsUp;
struct DNSServer { bool start(int,const char*,const IPAddress&){ g_dnsUp=true; return true; } void stop(){ g_dnsUp=false; } void processNextRequest(){} };
