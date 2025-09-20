#pragma once

// =====================================================
//                 PARÁMETROS DE LA APLICACIÓN
// =====================================================
#define PAROLA_BRIGHTNESS        5        // 0..15
#define INTERVALO_ESTADO_MS      0        // demo de rotación de estados (0 = desactivado)
#define BOOT_OTA_MINUTES         5        // ventana OTA al arrancar (min). 0 = desactivada

// =====================================================
//                 BOTÓN / ENTRADA RELÉ
// =====================================================
#define BUTTON_PIN               14
#define BUTTON_ACTIVE_LVL        LOW      // nivel activo (LOW si contacto a masa)
#define BUTTON_DEBOUNCE_MS       50       // debounce botón (ms)

// =====================================================
//                 MOTOR (IBT-2 / BTS7960)
// =====================================================
#define MOTOR_RPWM_PIN           25
#define MOTOR_LPWM_PIN           26
#define MOTOR_REN_PIN            32
#define MOTOR_LEN_PIN            33

#define MOTOR_PWM_FREQ           10000     // Hz
#define MOTOR_PWM_RES            8        // bits (0..255)
#define MOTOR_DUTY_DEFAULT       255      // duty por defecto

// ---------------- Rampa del motor ----------------
#define MOTOR_RAMP_STEP_PERCENT  5        // cambia 5% por tick
#define MOTOR_TICK_MS            20       // llama motor_tick() cada 20 ms

// Tiempo muerto (dead-time) al invertir sentido para proteger el puente H
#define MOTOR_REVERSE_DEADTIME_MS 80      // ms (60–120 ms recomendado)

// ---------------- Comprobación de sobrecorriente ----------------
#define CURRENT_CHECK_PERIOD_MS  20       // periodo de chequeo (ms)
#define CURRENT_BLANKING_MS      300      // ignora durante 300 ms desde que empieza a moverse

// =====================================================
//                 SENSOR HALL (ENCODER)
// =====================================================

// Pin que recibe los PULSOS del Hall (cada cambio = 1 paso)
#define HALL_PULSE_PIN           35

// Pin que indica DIRECCIÓN de giro
// - Si está a nivel alto = CERRANDO (si HALL_DIR_ACTIVE_HIGH_CLOSE=1)
// - Si está a nivel bajo  = ABRIENDO
#define HALL_DIR_PIN             36

// Número de pulsos que se esperan en un ciclo completo CERRADO → ABIERTO
// Se ajusta tras un ciclo completo y se guarda en NVS; este es el valor inicial
#define HALL_OPEN_PULSES_DEFAULT 5000

// % del recorrido donde el motor debe ir en modo "lento"
#define HALL_SLOWDOWN_THRESHOLD_PERCENT 20

// Frenado/velocidad suave por porcentaje de recorrido
#define MOTOR_SLOWDOWN_FACTOR_PERCENT  50 // velocidad = 50% de la base en tramo lento

// Define cómo interpretar el pin de dirección:
// 1 = nivel alto significa CERRANDO; 0 = nivel alto significa ABRIENDO
#define HALL_DIR_ACTIVE_HIGH_CLOSE 1

// Tiempo mínimo entre paradas consecutivas por Hall (antirebote, en ms)
#define HALL_STOP_DEBOUNCE_MS     80

// Hall ON/OFF por defecto (puede sobreescribirse en arranque)
#define HALL_ENABLED_DEFAULT      1      // 0 = OFF (útil para comisionado), 1 = ON

// =====================================================
//                 MQTT - TÓPICOS GENERALES
// =====================================================
#define TOPIC_CMD                 "garage/door/cmd"          // "ota on", "ota off", "status", "reboot"
#define TOPIC_OPEN_CMD            "garage/door/open/cmd"     // abrir mientras ON
#define TOPIC_CLOSE_CMD           "garage/door/close/cmd"    // cerrar mientras ON

// Programación de finales
#define TOPIC_MARK_CLOSED_CMD     "garage/door/mark_closed/cmd" // marcar posición actual como CERRADO (0)
#define TOPIC_MARK_OPEN_CMD       "garage/door/mark_open/cmd"   // marcar posición actual como ABIERTO (pulsos actuales)

// Información desde el ESP32
#define TOPIC_STATE               "garage/door/state"        // "ABRIENDO"/"CERRANDO"/"DETENIDO" (retained)
#define TOPIC_LOG                 "garage/door/log"          // logs
#define TOPIC_INFO                "garage/door/info"         // info de red/estado (JSON)
#define TOPIC_OTA                 "garage/door/ota"          // estado OTA (retained "ON"/"OFF")

// =====================================================
//                 MQTT - CORRIENTE (ACS712)
// =====================================================
#define TOPIC_ILIMIT              "garage/current/limit"       // estado (retained): valor actual del límite (A)
#define TOPIC_ILIMIT_CMD          "garage/current/limit/cmd"   // comando: dashboard publica nuevo valor
#define TOPIC_IMEAS               "garage/current/value"       // medida en tiempo real (A)

// =====================================================
//                 MQTT - VELOCIDAD MOTOR
// =====================================================
#define TOPIC_SPEED               "garage/motor/speed"       // estado actual (retained)
#define TOPIC_SPEED_CMD           "garage/motor/speed/cmd"   // comandos desde dashboard

// =====================================================
//                 MQTT - ENCODER
// =====================================================
#define TOPIC_ENC_POS             "garage/encoder/pos"       // posición (no retained)
#define TOPIC_ENC_DIR             "garage/encoder/dir"       // "1" abrir, "-1" cerrar

// =====================================================
//                 MQTT - HALL SENSOR
// =====================================================
// ON/OFF por defecto (puede sobreescribirse en arranque)
#define HALL_ENABLED_DEFAULT      1      // 0 = OFF (útil para comisionado), 1 = ON

// Comandos y estados MQTT
#define TOPIC_HALL_EN_CMD         "garage/hall/enabled/cmd"   // dashboard publica "ON"/"OFF"
#define TOPIC_HALL_EN_STATE       "garage/hall/enabled"       // estado actual (retained)


// =====================================================
//                 LUZ DE GARAJE (PWM)
// =====================================================
#define LIGHT_PIN                 27
#define LIGHT_PIN2                13      // segunda tira (opcional)

#define LIGHT_ACTIVE_LVL          HIGH    // o LOW si tu driver es activo-bajo

// ¡OJO! Usar 1/0 para que funcione con #if
#define LIGHT_PWM_ENABLED         1
#define LIGHT_PWM_FREQ_HZ         1000
#define LIGHT_PWM_RES_BITS        10
#define LIGHT_DEFAULT_LEVEL       1023    // para 10 bits, 1023 = max
#define LIGHT_AUTO_OFF_MS         (2UL * 60UL * 1000UL)  // 2 minutos

// ---------------- MQTT - LUCES ----------------
#define TOPIC_LIGHT_CMD           "garage/light/cmd"           // "ON" / "OFF" / "TOGGLE"
#define TOPIC_LIGHT_STATE         "garage/light/state"         // "ON"/"OFF" (retained)
#define TOPIC_LIGHT_DIM_CMD       "garage/light/dim/cmd"       // 0..100 (%) o 0..1023 (raw)
#define TOPIC_LIGHT_DIM           "garage/light/dim"           // nivel actual (retained)

// Modo respiración (en reposo, con nivel actual)
#define TOPIC_LIGHT_BREATH_CMD    "garage/light/breath/cmd"    // "ON" / "OFF"
#define TOPIC_LIGHT_BREATH_STATE  "garage/light/breath/state"  // "ON" / "OFF"

// =====================================================
//                 SALVAGUARDAS (PLAUSIBILIDAD)
// =====================================================

// Por debajo de este valor consideramos "corriente ~0"
#define SAFETY_MIN_CURRENT_A              0.15f   

// Tiempo tolerado con corriente ~0 estando en movimiento (ms)
#define SAFETY_ZERO_CURRENT_TIMEOUT_MS    800     

// Tiempo máximo permitido sin pulsos Hall suficientes estando con corriente (ms)
#define SAFETY_NO_ENCODER_TIMEOUT_MS      2000    

// Pulsos mínimos acumulados que deben aparecer dentro de la ventana anterior
#define SAFETY_MIN_PULSES_TOTAL           2       

// Periodo mínimo entre comprobaciones (ms)
#define SAFETY_CHECK_PERIOD_MS            50      
