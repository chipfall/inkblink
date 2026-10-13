#include "spi.h"
#include "trace.h"
std::vector<TraceCmd> g_trace;
void EPD_GPIOInit(void) {}
void EPD_WR_Bus(uint8_t) {}
void EPD_WR_REG(uint8_t reg) { g_trace.push_back({reg, {}}); }
void EPD_WR_DATA8(uint8_t d) { if (!g_trace.empty()) g_trace.back().data.push_back(d); }
