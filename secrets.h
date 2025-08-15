#pragma once
#include <cstdint> // Para uint16_t

// ===== Wi‑Fi & OTA =====
extern const char*    ssid;
extern const char*    password;
extern const char*    ota_password;

// ===== MQTT =====
extern const char*    mqtt_host;
extern const uint16_t mqtt_port;
extern const char*    mqtt_user;
extern const char*    mqtt_pass;