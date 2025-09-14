#include <Arduino.h>
#include <math.h>
#include "config.h"
#include "light.h"
#include "state.h"
#include "logx.h"

// Este archivo usa la API nueva (core ESP32 3.x):
//   - ledcAttach(pin, freq, res);
//   - ledcWrite(pin, duty);
// Si compilas con core 2.x, usa la versión anterior con ledcSetup/ledcAttachPin/ledcWrite(canal).

// =========================
// Estado interno de la luz
// =========================
static bool s_on = false;
static uint16_t s_level_full = LIGHT_DEFAULT_LEVEL;   // nivel "usuario" (reposo)
static uint32_t s_tAutoOffStart = 0;
static EstadoPuerta s_prevEstado = DETENIDO;

// Control de usuario por MQTT (en reposo): cualquier comando marca override
static bool s_user_override = false;

// Respiración solicitada por el usuario en reposo (respira con s_level_full)
static bool s_breath_user_mode = false;

// Helpers PWM
static inline uint16_t levelMax() { return (1U << LIGHT_PWM_RES_BITS) - 1U; }
static inline uint16_t clampLevel(uint32_t v) {
  uint16_t m = levelMax();
  return (v > m) ? m : (uint16_t)v;
}

// Ajusta el duty si el hardware es activo-bajo
static inline uint16_t applyActive(uint16_t raw) {
  return (LIGHT_ACTIVE_LVL == HIGH) ? raw : (uint16_t)(levelMax() - raw);
}

static void writePWMToPin(uint8_t pin, uint16_t raw) {
#if LIGHT_PWM_ENABLED
  uint16_t duty = applyActive(raw);
  ledcWrite(pin, duty); // core 3.x: write por pin
#else
  digitalWrite(pin, (s_on ? LIGHT_ACTIVE_LVL : (LIGHT_ACTIVE_LVL == HIGH ? LOW : HIGH)));
#endif
}

// ===============================
// Parámetros y estado respiración
// ===============================
static constexpr uint16_t LIGHT_LEVEL_MOVIMIENTO = (uint16_t)((uint32_t)LIGHT_DEFAULT_LEVEL * 40U / 100U); // 40% en movimiento
static bool s_resp_creasing = true;
static uint8_t s_resp_val = 0; // 0..100
static const uint8_t s_resp_step = 5;
static uint32_t s_last_resp_update = 0;

// ====================
// Aplicación de salida
// ====================
static void applyOutput() {
#if LIGHT_PWM_ENABLED
  uint16_t level1 = 0, level2 = 0;

  if (!s_on) {
    level1 = level2 = 0;
  } else {
    EstadoPuerta e = getEstado();
    if (e == ABRIENDO || e == CERRANDO) {
      // En movimiento: respiración al 40% (alternada entre tiras)
      uint8_t v = s_resp_val;
      level1 = (uint16_t)((uint32_t)LIGHT_LEVEL_MOVIMIENTO * v / 100U);
      level2 = (uint16_t)((uint32_t)LIGHT_LEVEL_MOVIMIENTO * (100U - v) / 100U);
    } else {
      // Reposo
      if (s_breath_user_mode) {
        // Respiración con el brillo actual del usuario
        uint8_t v = s_resp_val;
        level1 = (uint16_t)((uint32_t)s_level_full * v / 100U);
        level2 = (uint16_t)((uint32_t)s_level_full * (100U - v) / 100U);
      } else {
        // Fijo
        level1 = level2 = s_level_full;
      }
    }
  }

  writePWMToPin(LIGHT_PIN, clampLevel(level1));
  #ifdef LIGHT_PIN2
  writePWMToPin(LIGHT_PIN2, clampLevel(level2));
  #endif

#else
  // Sin PWM, binario: simple on/off
  if (s_on) {
    digitalWrite(LIGHT_PIN, LIGHT_ACTIVE_LVL);
    #ifdef LIGHT_PIN2
    digitalWrite(LIGHT_PIN2, LIGHT_ACTIVE_LVL);
    #endif
  } else {
    digitalWrite(LIGHT_PIN, (LIGHT_ACTIVE_LVL == HIGH) ? LOW : HIGH);
    #ifdef LIGHT_PIN2
    digitalWrite(LIGHT_PIN2, (LIGHT_ACTIVE_LVL == HIGH) ? LOW : HIGH);
    #endif
  }
#endif
}

// =====================
// API pública de "light"
// =====================
void light_begin() {
#if LIGHT_PWM_ENABLED
  // API nueva: unifica setup+attach por pin
  ledcAttach(LIGHT_PIN, LIGHT_PWM_FREQ_HZ, LIGHT_PWM_RES_BITS);
  #ifdef LIGHT_PIN2
  ledcAttach(LIGHT_PIN2, LIGHT_PWM_FREQ_HZ, LIGHT_PWM_RES_BITS);
  #endif
#else
  pinMode(LIGHT_PIN, OUTPUT);
  #ifdef LIGHT_PIN2
  pinMode(LIGHT_PIN2, OUTPUT);
  #endif
#endif
  s_on = false;
  s_user_override = false;
  s_breath_user_mode = false;
  s_level_full = clampLevel(LIGHT_DEFAULT_LEVEL);
  s_prevEstado = getEstado(); // por si el arranque no es DETENIDO
  applyOutput();
}

// Apagado suave disminuyendo nivel
static void fadeOutStep() {
  if (s_level_full > 0) {
    s_level_full = (s_level_full > 20) ? (uint16_t)(s_level_full - 20) : 0;
    applyOutput();
  }
  if (s_level_full == 0) {
    s_on = false;
  }
}

void light_on()  {
  s_on = true;
  applyOutput();
}

void light_off() {
  // Apagado suave manual: se inicia fade-out desde el nivel actual
  s_tAutoOffStart = 0;   // cancelar auto-off programado
  s_user_override = true;
  s_on = true;           // asegurar que está encendido para hacer fade
  // El fade real lo ejecuta light_tick cuando toque el timer,
  // pero si quieres cortar ya, puedes llamar a fadeOutStep() en bucle desde fuera.
}

void light_toggle() {
  if (s_on) {
    light_off();
  } else {
    s_user_override = true;
    s_on = true;
    applyOutput();
  }
}

bool light_is_on() {
  return s_on;
}

void light_set_level(uint16_t level) {
  s_level_full = clampLevel(level);
  s_user_override = true;
  if (s_on) applyOutput();
}

uint16_t light_get_level() {
  return s_level_full;
}

void light_set_percent(uint8_t pct) {
  if (pct > 100) pct = 100;
  uint32_t raw = (uint32_t)pct * levelMax() / 100U;
  light_set_level((uint16_t)raw);
}

void light_set_breath_mode(bool enable) {
  s_breath_user_mode = enable; // sólo aplica en reposo
  s_user_override = true;
  s_on = (s_on || enable);  // si activan respiración, enciende si estaba apagado
  applyOutput();
}

bool light_get_breath_mode() {
  return s_breath_user_mode;
}

// ===========================================
// Lógica automática: movimiento / reposo / fade
// ===========================================
void light_tick(uint32_t now) {
  EstadoPuerta e = getEstado();

  // Cambios de estado de la puerta
  if (e != s_prevEstado) {
    if (e == ABRIENDO || e == CERRANDO) {
      // Al empezar a moverse: control automático
      s_on = true;
      s_user_override = false;     // desactiva override de usuario durante movimiento
      s_breath_user_mode = false;  // respiración "usuario" no aplica en movimiento
      s_tAutoOffStart = 0;         // sin auto-off durante el movimiento
      s_resp_val = 0;              // reinicia respiración
      s_resp_creasing = true;
      applyOutput();
    } else {
      // Al detenerse: 100% fijo y arrancar auto-off
      s_breath_user_mode = false;
      s_on = true;
      s_level_full = clampLevel(levelMax()); // 100% fijo
      applyOutput();
      s_tAutoOffStart = now;                 // cuenta atrás (p.ej. 2 min)
      // s_user_override sigue en false hasta que llegue MQTT
    }
    s_prevEstado = e;
  }

  // Actualiza efecto respiración
  if (s_on) {
    bool doResp = false;
    if (e == ABRIENDO || e == CERRANDO) {
      doResp = true; // respiración al 40%
    } else if (s_breath_user_mode) {
      doResp = true; // respiración con s_level_full en reposo
    }

    if (doResp && (now - s_last_resp_update > 80U)) {
      s_last_resp_update = now;
      if (s_resp_creasing) {
        uint8_t nv = (uint8_t)(s_resp_val + s_resp_step);
        s_resp_val = (nv >= 100) ? 100 : nv;
        if (s_resp_val >= 100) s_resp_creasing = false;
      } else {
        if (s_resp_val > s_resp_step) s_resp_val -= s_resp_step;
        else s_resp_val = 0;
        if (s_resp_val == 0) s_resp_creasing = true;
      }
      applyOutput();
    }
  }

  // Auto-apagado (sólo en reposo, sin override de usuario)
  if (s_tAutoOffStart && (e != ABRIENDO && e != CERRANDO) && !s_user_override) {
    if (now - s_tAutoOffStart >= LIGHT_AUTO_OFF_MS) {
      // Fade-out gradual: una muesca por "tick"
      fadeOutStep();
      s_tAutoOffStart = now;
      if (!s_on) {
        // apagado completo: detener timer
        s_tAutoOffStart = 0;
      }
    }
  }
}
