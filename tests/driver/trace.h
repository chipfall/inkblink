#pragma once
#include <vector>
#include <cstdint>
struct TraceCmd { int reg; std::vector<uint8_t> data; };
extern std::vector<TraceCmd> g_trace;

inline bool epdWasFast() { for (auto& c : g_trace) if (c.reg == 0x10) return true; return false; }
