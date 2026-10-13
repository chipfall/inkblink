#pragma once
// With the stub driver there is no register trace; the stub only remembers how the last
// screen update was sent (full or fast). tests/driver/trace.h replaces this for the real driver.
#include <vector>
#include <cstdint>
struct TraceCmd { int reg; std::vector<uint8_t> data; };
extern std::vector<TraceCmd> g_trace;
extern bool g_epdLastFast;
inline bool epdWasFast() { return g_epdLastFast; }
