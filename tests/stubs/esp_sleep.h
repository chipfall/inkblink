#pragma once
typedef int gpio_num_t;
#define GPIO_NUM_0 0
inline void esp_sleep_enable_ext0_wakeup(gpio_num_t,int){}
inline void esp_deep_sleep_start(){ extern bool g_slept; g_slept=true; }
