#pragma once
typedef int gpio_num_t;
#define GPIO_NUM_0 0
inline void esp_sleep_enable_ext0_wakeup(gpio_num_t,int){}
inline void esp_deep_sleep_start(){ extern bool g_slept; g_slept=true; }

// --- lekkie uspienie i taktowanie procesora (testy: licznik wywolan) ---
#define GPIO_INTR_LOW_LEVEL 4
extern int g_lightSleeps;
extern int g_cpuMhz;
extern int g_cpuHistory[32];
extern int g_cpuHistoryN;
inline void gpio_wakeup_enable(gpio_num_t, int) {}
inline void esp_sleep_enable_gpio_wakeup() {}
inline void esp_sleep_enable_timer_wakeup(unsigned long long) {}
inline void esp_light_sleep_start() { g_lightSleeps++; }
inline bool setCpuFrequencyMhz(unsigned mhz) { g_cpuMhz = (int)mhz; if (g_cpuHistoryN < 32) g_cpuHistory[g_cpuHistoryN++] = (int)mhz; return true; }
extern int g_wakeCause;
#define ESP_SLEEP_WAKEUP_UNDEFINED 0
inline int esp_sleep_get_wakeup_cause() { return g_wakeCause; }
