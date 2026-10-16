#pragma once
// Minimal stand-in for the ESP-IDF WiFi config API used for diagnostics.
typedef int esp_err_t;
#define ESP_OK 0
enum wifi_interface_t { WIFI_IF_STA = 0, WIFI_IF_AP = 1 };
struct wifi_ap_config_t { unsigned char ssid_len; unsigned char ssid_hidden; int authmode; unsigned char channel; };
union wifi_config_t { wifi_ap_config_t ap; };
inline esp_err_t esp_wifi_get_config(wifi_interface_t, wifi_config_t* c) { c->ap.ssid_len = 13; c->ap.ssid_hidden = 0; c->ap.authmode = 3; c->ap.channel = 1; return ESP_OK; }
