#pragma once
typedef int gpio_num_t;
#define GPIO_NUM_0 0
inline void esp_sleep_enable_ext0_wakeup(gpio_num_t,int){}
inline void esp_deep_sleep_start(){ extern bool g_slept, g_timerWake, g_deepTimerWake; g_slept=true; g_deepTimerWake=g_timerWake; }

// --- lekkie uspienie i taktowanie procesora (testy: licznik wywolan) ---
#define GPIO_INTR_LOW_LEVEL 4
extern int g_lightSleeps;
extern int g_cpuMhz;
extern int g_cpuHistory[32];
extern int g_cpuHistoryN;
inline void gpio_wakeup_enable(gpio_num_t, int) {}
inline void esp_sleep_enable_gpio_wakeup() {}
inline void esp_sleep_enable_timer_wakeup(unsigned long long) { extern bool g_timerWake; g_timerWake = true; }
extern bool g_timerWake;     // budzik czasowy ustawiony (nie moze zostac przed glebokim snem)
inline void esp_light_sleep_start() { g_lightSleeps++; }
#define ESP_SLEEP_WAKEUP_ALL 0
inline void esp_sleep_disable_wakeup_source(int) { g_timerWake = false; }
inline void gpio_wakeup_disable(gpio_num_t) {}
inline bool setCpuFrequencyMhz(unsigned mhz) { g_cpuMhz = (int)mhz; if (g_cpuHistoryN < 32) g_cpuHistory[g_cpuHistoryN++] = (int)mhz; return true; }
extern int g_wakeCause;
#define ESP_SLEEP_WAKEUP_UNDEFINED 0
inline int esp_sleep_get_wakeup_cause() { return g_wakeCause; }
