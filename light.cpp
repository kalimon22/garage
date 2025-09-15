#include <Arduino.h>
#include <math.h>
#include <Preferences.h>
#include "config.h"
#include "light.h"
#include "state.h"
#include "logx.h"

// =============== Config interna de efectos ===============
static const uint32_t RESP_UPDATE_MS   = 30;    // refresco respiración
static const uint32_t RESP_PERIOD_MS   = 2000;  // periodo respiración (ms)
static const float    BREATH_MIN       = 0.20f; // no baja a negro (20%)
static const float    BREATH_MAX       = 1.00f;

static const uint16_t FADE_STEP        = 20;    // decremento por paso (ajusta velocidad)
static const uint32_t FADE_STEP_MS     = 30;    // periodo de pasos de fade

// =============== Estado interno ===============
static bool         s_on = false;                 // estado lógico
static uint16_t     s_user_max = LIGHT_DEFAULT_LEVEL; // brillo guardado (persistente) 0..(2^RES-1)
static uint32_t     s_autoOffStart = 0;           // se arma al terminar movimiento
static bool         s_fading = false;             // hay fade en curso (auto-off o OFF manual)
static uint32_t     s_lastFadeStep = 0;           // control de cadencia del fade
static uint16_t     s_fadeLevel = 0;              // nivel actual durante fade (ambas tiras igual)

static bool         s_breath_user = false;        // respiración solicitada por MQTT en reposo
static float        s_breath_phase = 0.0f;        // 0..1
static uint32_t     s_lastRespUpdate = 0;
static EstadoPuerta s_prevEstado = DETENIDO;

static Preferences  s_prefs;                      // NVS para el brillo guardado

// =============== Helpers PWM ===============
static inline uint16_t levelMax() { return (1U << LIGHT_PWM_RES_BITS) - 1U; }
static inline uint16_t clampLevel(uint32_t v) {
  uint16_t m = levelMax();
  return (v > m) ? m : (uint16_t)v;
}
static inline uint16_t applyActive(uint16_t raw) {
  return (LIGHT_ACTIVE_LVL == HIGH) ? raw : (uint16_t)(levelMax() - raw);
}
static void writePWMToPin(uint8_t pin, uint16_t raw) {
#if LIGHT_PWM_ENABLED
  ledcWrite(pin, applyActive(raw)); // core 3.x: write por pin
#else
  // sin PWM: binario
  digitalWrite(pin, (s_on ? LIGHT_ACTIVE_LVL : (LIGHT_ACTIVE_LVL == HIGH ? LOW : HIGH)));
#endif
}

// =============== Respiración ===============
static inline float breathFactor(float phase01) {
  float s = sinf(phase01 * 2.0f * (float)M_PI); // -1..+1
  float x = (s + 1.0f) * 0.5f;                  // 0..1
  return BREATH_MIN + (BREATH_MAX - BREATH_MIN) * x;
}

// =============== Salida ===============
static void applyOutput() {
#if LIGHT_PWM_ENABLED
  uint16_t level1 = 0, level2 = 0;

  if (!s_on) {
    level1 = level2 = 0;
  } else if (s_fading) {
    // Fade manda: mismas dos tiras
    level1 = level2 = clampLevel(s_fadeLevel);
  } else {
    // Construye intensidades según estado de la puerta
    EstadoPuerta e = getEstado();
    const uint16_t baseRest   = s_user_max;                             // 100% guardado
    const uint16_t baseMoving = (uint16_t)((uint32_t)s_user_max * 40U / 100U); // 40% guardado

    if (e == ABRIENDO || e == CERRANDO) {
      // Respiración alternada en movimiento (suave, no llega a negro)
      float ph = s_breath_phase;
      float f1 = breathFactor(ph);
      float f2 = breathFactor(fmodf(ph + 0.5f, 1.0f)); // 180° de desfase
      level1 = clampLevel((uint32_t)(baseMoving * f1));
      level2 = clampLevel((uint32_t)(baseMoving * f2));
    } else {
      // Reposo
      if (s_breath_user) {
        float ph = s_breath_phase;
        float f1 = breathFactor(ph);
        float f2 = breathFactor(fmodf(ph + 0.5f, 1.0f));
        level1 = clampLevel((uint32_t)(baseRest * f1));
        level2 = clampLevel((uint32_t)(baseRest * f2));
      } else {
        level1 = level2 = baseRest;
      }
    }
  }

  writePWMToPin(LIGHT_PIN, clampLevel(level1));
  #ifdef LIGHT_PIN2
  writePWMToPin(LIGHT_PIN2, clampLevel(level2));
  #endif

#else
  // Modo binario sin PWM
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

// =============== Persistencia ===============
static void persist_user_max(uint16_t lvl) {
  s_prefs.begin("light", false);       // false -> RW (guardar)
  s_prefs.putUShort("max", lvl);
  s_prefs.end();
}

// =============== API pública ===============
void light_begin() {
#if LIGHT_PWM_ENABLED
  ledcAttach(LIGHT_PIN,  LIGHT_PWM_FREQ_HZ, LIGHT_PWM_RES_BITS);
  #ifdef LIGHT_PIN2
  ledcAttach(LIGHT_PIN2, LIGHT_PWM_FREQ_HZ, LIGHT_PWM_RES_BITS);
  #endif
#else
  pinMode(LIGHT_PIN, OUTPUT);
  #ifdef LIGHT_PIN2
  pinMode(LIGHT_PIN2, OUTPUT);
  #endif
#endif

  // Cargar brillo guardado (si no hay, usa por defecto)
  s_prefs.begin("light", true);        // true -> RO (leer)
  uint16_t stored = s_prefs.getUShort("max", clampLevel(LIGHT_DEFAULT_LEVEL));
  s_prefs.end();
  s_user_max = clampLevel(stored);

  s_on = false;
  s_fading = false;
  s_fadeLevel = 0;
  s_autoOffStart = 0;
  s_breath_user = false;
  s_breath_phase = 0.0f;
  s_lastRespUpdate = 0;
  s_prevEstado = getEstado();

  applyOutput();
}

void light_on() {
  // Cancela fades y enciende a 100% del guardado (en reposo)
  s_fading = false;
  s_on = true;
  s_fadeLevel = s_user_max; // por consistencia
  s_autoOffStart = 0;       // encendido manual no arma auto-off
  applyOutput();
}

void light_off() {
  // Iniciar fade manual desde el nivel guardado
  s_on = true;          // para poder hacer fade
  s_fading = true;
  s_fadeLevel = s_user_max;
  s_lastFadeStep = millis();
  s_autoOffStart = 0;   // cortar auto-off en curso (esto es un OFF manual)
  applyOutput();
}

void light_toggle() {
  if (s_on && !s_fading) light_off();
  else                   light_on();
}

bool light_is_on() { return s_on; }

void light_set_level(uint16_t level) {
  // Este nivel es el "máximo guardado" (persistente)
  s_user_max = clampLevel(level);
  persist_user_max(s_user_max);

  // Si está encendida y en reposo (no fading), actualiza salida
  if (s_on && !s_fading && (getEstado() == DETENIDO)) {
    applyOutput();
  }
}

uint16_t light_get_level() {
  return s_user_max;
}

void light_set_percent(uint8_t pct) {
  if (pct > 100) pct = 100;
  uint32_t raw = (uint32_t)pct * levelMax() / 100U;
  light_set_level((uint16_t)raw); // persiste
}

void light_set_breath_mode(bool enable) {
  s_breath_user = enable;
  if (enable && !s_on) {
    // activar respiración en reposo implica encender a 100% del guardado
    s_on = true;
    s_fading = false;
  }
  applyOutput();
}

bool light_get_breath_mode() { return s_breath_user; }

// =============== TICK: animaciones, auto-off, transiciones ===============
void light_tick(uint32_t now) {
  EstadoPuerta e = getEstado();

  // Respiración (solo si ON y no hay fade)
  if (s_on && !s_fading && (now - s_lastRespUpdate >= RESP_UPDATE_MS)) {
    s_lastRespUpdate = now;
    float d = (float)RESP_UPDATE_MS / (float)RESP_PERIOD_MS; // 0..1 por ciclo
    s_breath_phase += d;
    if (s_breath_phase >= 1.0f) s_breath_phase -= 1.0f;
    applyOutput();
  }

  // Fade (manual OFF o auto-off)
  if (s_fading && (now - s_lastFadeStep >= FADE_STEP_MS)) {
    s_lastFadeStep = now;
    if (s_fadeLevel > 0) {
      s_fadeLevel = (s_fadeLevel > FADE_STEP) ? (uint16_t)(s_fadeLevel - FADE_STEP) : 0;
      applyOutput();
    }
    if (s_fadeLevel == 0) {
      s_on = false;
      s_fading = false;
      s_autoOffStart = 0; // por si vino de auto-off
      applyOutput();
    }
  }

  // Cambio de estado de puerta
  if (e != s_prevEstado) {
    if (e == ABRIENDO || e == CERRANDO) {
      // Entramos en movimiento: ON, respirar al 40%, sin auto-off ni fade activos
      s_on = true;
      s_fading = false;
      s_autoOffStart = 0;
      s_breath_user = false;  // en movimiento solo respiración automática
      applyOutput();
    } else {
      // Se detuvo: ON al 100% guardado y armar auto-off
      s_on = true;
      s_fading = false;
      s_fadeLevel = s_user_max;
      applyOutput();
      s_autoOffStart = now;   // aquí arranca el contador (p.ej. 15 s o 15 min)
    }
    s_prevEstado = e;
  }

  // Auto-off solo cuando estamos en reposo, ON, sin fade manual
  if (s_autoOffStart && !s_fading && s_on && (e == DETENIDO)) {
    if (now - s_autoOffStart >= LIGHT_AUTO_OFF_MS) {
      // Disparar fade de auto-off
      s_fading = true;
      s_fadeLevel = s_user_max;  // arranca desde el 100% guardado
      s_lastFadeStep = now;
      s_autoOffStart = 0;        // desarma el contador; fade se encarga de apagar
      applyOutput();
    }
  }
}
