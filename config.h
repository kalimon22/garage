#pragma once
// ===== Parámetros de la app =====
#define PAROLA_BRIGHTNESS     5     // 0..15
#define INTERVALO_ESTADO_MS   0     // demo de rotación de estados (ms). Pon 0 para desactivarla
#define BOOT_OTA_MINUTES      5     // Ventana OTA al arrancar (min). 0 = desactivada

// ===== Botón / entrada de relé =====
#define BUTTON_PIN       14
#define BUTTON_ACTIVE_LVL LOW       // Nivel activo (LOW si contacto a masa)
#define BUTTON_DEBOUNCE_MS 50       // Debounce botón (ms)

// ===== Motor (IBT-2 BTS7960) =====
#define MOTOR_RPWM_PIN     25
#define MOTOR_LPWM_PIN     26
#define MOTOR_REN_PIN      32
#define MOTOR_LEN_PIN      33

#define MOTOR_PWM_FREQ     1000     // Hz
#define MOTOR_PWM_RES      8        // bits (0..255)
#define MOTOR_DUTY_DEFAULT 255      // duty (≈78% del máximo)

// ===== Motor (rampa) =====
#define MOTOR_RAMP_STEP_PERCENT  5  // cambia 5% por tick
#define MOTOR_TICK_MS            20 // llama motor_tick() cada 20 ms

// Tiempo muerto (dead-time) al invertir sentido para proteger el puente H
#define MOTOR_REVERSE_DEADTIME_MS 80   // ms (60–120 ms va bien)

// Comprobación de sobrecorriente
#define CURRENT_CHECK_PERIOD_MS  20     // cada 20 ms
#define CURRENT_BLANKING_MS      300    // ignora durante 300 ms desde que empieza a moverse



// =========================
// Configuración Sensor Hall
// =========================

// Pin que recibe los PULSOS del Hall (cada cambio = 1 paso)
#define HALL_PULSE_PIN   35

// Pin que indica DIRECCIÓN de giro
// - Si está a nivel alto = CERRANDO (si HALL_DIR_ACTIVE_HIGH_CLOSE=1)
// - Si está a nivel bajo  = ABRIENDO
#define HALL_DIR_PIN     36

// Número de pulsos que se esperan en un ciclo completo CERRADO → ABIERTO
// Se ajusta tras un ciclo completo y se guarda en NVS, este es solo el valor inicial
#define HALL_OPEN_PULSES_DEFAULT 5000  

// % del recorrido donde el motor debe ir en modo "lento"
// Ej: 10% = los primeros y últimos 10% del trayecto van despacio
#define HALL_SLOWDOWN_THRESHOLD_PERCENT 20
// ----- Frenado suave por porcentaje de recorrido -----
#define MOTOR_SLOWDOWN_FACTOR_PERCENT     50   // velocidad = 40% de la base cuando está en tramo lento

// Define cómo interpretar el pin de dirección
// - 1 = nivel alto significa que la puerta está CERRANDO
// - 0 = nivel alto significa que la puerta está ABRIENDO
#define HALL_DIR_ACTIVE_HIGH_CLOSE 1

// Tiempo mínimo entre paradas consecutivas por Hall (antirebote, en ms)
#define HALL_STOP_DEBOUNCE_MS 80
// ===== Tópicos MQTT =====

// Hall ON/OFF por defecto (puertas.ini puede sobreescribirlo en tu arranque)
#define HALL_ENABLED_DEFAULT  1  // 0=OFF (más fácil comisionar la 1ª vez), 1=ON

// Tópicos MQTT para habilitar Hall
#define TOPIC_HALL_EN_CMD   "garage/hall/enabled/cmd"   // publica "ON"/"OFF" desde tu dashboard
#define TOPIC_HALL_EN_STATE "garage/hall/enabled"       // estado actual (retained)

// --- Comandos al ESP32 ---
#define TOPIC_CMD           "garage/door/cmd"          // comandos generales ("ota on", "ota off", "status", "reboot")
#define TOPIC_OPEN_CMD      "garage/door/open/cmd"     // abrir mientras ON
#define TOPIC_CLOSE_CMD     "garage/door/close/cmd"    // cerrar mientras ON

// Programación de finales (nuevo)
#define TOPIC_MARK_CLOSED_CMD  "garage/door/mark_closed/cmd"   // marcar posición actual como CERRADO (0)
#define TOPIC_MARK_OPEN_CMD    "garage/door/mark_open/cmd"     // marcar posición actual como ABIERTO (pulsos actuales)

// --- Información desde el ESP32 ---
#define TOPIC_STATE       "garage/door/state"        // estado ("ABRIENDO", "CERRANDO", "DETENIDO"), retained
#define TOPIC_LOG         "garage/door/log"          // logs
#define TOPIC_INFO        "garage/door/info"         // info de red/estado, JSON
#define TOPIC_OTA         "garage/door/ota"          // estado OTA, retained ("ON"/"OFF")

// --- Corriente (ACS712) ---
#define TOPIC_ILIMIT       "garage/current/limit"       // Estado (retained): valor actual del límite (A)
#define TOPIC_ILIMIT_CMD   "garage/current/limit/cmd"   // Comando: dashboard publica nuevo valor
#define TOPIC_IMEAS        "garage/current/value"       // Medida en tiempo real (A)

// --- Velocidad motor ---
#define TOPIC_SPEED        "garage/motor/speed"       // estado actual (retained)
#define TOPIC_SPEED_CMD    "garage/motor/speed/cmd"   // comandos desde dashboard

// Publicación de posición del encoder (pulsos relativos desde reset/arranque)
#define TOPIC_ENC_POS   "garage/encoder/pos"    // estado (no retained)
#define TOPIC_ENC_DIR   "garage/encoder/dir"    // "1" abrir, "-1" cerrar


