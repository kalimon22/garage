#include "safety.h"
#include "config.h"
#include "state.h"
#include "motor.h"
#include "current.h"
#include "hall.h"
#include "logx.h"
#include "net.h"
#include "light.h"

// --- Salvaguardas de plausibilidad (valores por defecto) ---
// Puedes sobreescribir en config.h si quieres
#ifndef SAFETY_MIN_CURRENT_A
#define SAFETY_MIN_CURRENT_A              0.15f
#endif
#ifndef SAFETY_ZERO_CURRENT_TIMEOUT_MS
#define SAFETY_ZERO_CURRENT_TIMEOUT_MS    800
#endif
#ifndef SAFETY_NO_ENCODER_TIMEOUT_MS
#define SAFETY_NO_ENCODER_TIMEOUT_MS      700
#endif
#ifndef SAFETY_MIN_PULSES_DELTA
#define SAFETY_MIN_PULSES_DELTA           2
#endif
#ifndef SAFETY_CHECK_PERIOD_MS
#define SAFETY_CHECK_PERIOD_MS            50
#endif

static uint32_t tLastCheck = 0;
static long lastEnc = 0;
static uint32_t tZeroISince = 0;
static uint32_t tNoEncSince = 0;

static void safety_emergency_stop(const char* reason) {
  motor_emergency_stop();   // si no existe, usa motor_stop()
  setEstado(DETENIDO);

  logPrintf("[SAFETY] Parada de emergencia: %s\n", reason);
  net_mqtt_publish(TOPIC_LOG, String("[SAFETY] ") + reason, false);

#if defined(LIGHT_PWM_ENABLED)
  light_set_level((1U << LIGHT_PWM_RES_BITS) - 1);
  light_on();
#endif
}

void safety_begin() {
  tLastCheck = 0;
  lastEnc = hall_get_count();
  tZeroISince = 0;
  tNoEncSince = 0;
}

void safety_tick(uint32_t now) {
  if (now - tLastCheck < SAFETY_CHECK_PERIOD_MS) return;
  tLastCheck = now;

  EstadoPuerta e = getEstado();
  bool moving = (e == ABRIENDO || e == CERRANDO);

  if (!moving) {
    tZeroISince = 0;
    tNoEncSince = 0;
    lastEnc = hall_get_count();
    return;
  }

  float I = current_readA();
  long enc = hall_get_count();

  // (1) Motor en movimiento declarado pero corriente ~0
  if (I <= SAFETY_MIN_CURRENT_A) {
    if (tZeroISince == 0) tZeroISince = now;
    if (now - tZeroISince >= SAFETY_ZERO_CURRENT_TIMEOUT_MS) {
      safety_emergency_stop("Motor moviendo pero corriente ~0 (posible cable suelto).");
      tZeroISince = 0;
      tNoEncSince = 0;
      lastEnc = enc;
      return;
    }
  } else {
    tZeroISince = 0;
  }

  // (2) Corriente presente pero sin pulsos Hall
  if (hall_is_enabled()) {
    long dp = enc - lastEnc;
    if (llabs(dp) < SAFETY_MIN_PULSES_DELTA) {
      if (tNoEncSince == 0) tNoEncSince = now;
      if (now - tNoEncSince >= SAFETY_NO_ENCODER_TIMEOUT_MS) {
        safety_emergency_stop("Corriente presente pero sin pulsos Hall (atasco o sensor KO).");
        tNoEncSince = 0;
        tZeroISince = 0;
        lastEnc = enc;
        return;
      }
    } else {
      tNoEncSince = 0;
    }
  }

  lastEnc = enc;
}
