#pragma once
// ===== Parámetros de la app =====
#define PAROLA_BRIGHTNESS     5     // 0..15
#define INTERVALO_ESTADO_MS   0  // demo de rotación de estados (ms). Pon 0 para desactivarla
#define BOOT_OTA_MINUTES      5     // Ventana OTA al arrancar (min). 0 = desactivada

// ===== GPIOs =====
#define RELAY_PIN        27
#define RELAY_ACTIVE_LVL LOW
#define BUTTON_PIN       14

// ===== Tópicos MQTT =====
// --- Tópicos de envío de comandos al ESP32 ---
// TOPIC_CMD:     Comandos generales ("ota on [min]", "ota off", "status", "reboot")
#define TOPIC_CMD        "garage/door/cmd"
// TOPIC_OPEN_CMD: Comando para abrir mientras se mantenga ON. Valores esperados: "ON" / "OFF"
#define TOPIC_OPEN_CMD   "garage/door/open/cmd"
// TOPIC_CLOSE_CMD: Comando para cerrar mientras se mantenga ON. Valores esperados: "ON" / "OFF"
#define TOPIC_CLOSE_CMD  "garage/door/close/cmd"

// --- Tópicos de información publicados por el ESP32 ---
// TOPIC_STATE:   Estado actual de la puerta ("ABRIENDO", "CERRANDO", "DETENIDO"). Retained
#define TOPIC_STATE      "garage/door/state"     // retained
// TOPIC_LOG:     Mensajes de log en texto plano
#define TOPIC_LOG        "garage/door/log"
// TOPIC_INFO:    Información de red, IP y estado de conexión en JSON
#define TOPIC_INFO       "garage/door/info"
// TOPIC_OTA:     Estado OTA actual ("ON"/"OFF"). Retained
#define TOPIC_OTA        "garage/door/ota"      // retained (ON/OFF)