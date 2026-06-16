// Copy this file to "config.h" and fill in your values.
// config.h is git-ignored so your secrets stay private.
#ifndef EDGEAI_ESP_CONFIG_H
#define EDGEAI_ESP_CONFIG_H

// --- WiFi network the ESP8266 should join ---
#define WIFI_SSID   "YourWiFiName"
#define WIFI_PASS   "YourWiFiPassword"

// --- Groq cloud API (https://console.groq.com/keys) ---
#define GROQ_API_KEY "gsk_xxxxxxxxxxxxxxxxxxxxxxxx"
#define GROQ_MODEL   "llama-3.3-70b-versatile"

#endif  // EDGEAI_ESP_CONFIG_H
