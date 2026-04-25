#include <Arduino.h>
#include <Preferences.h>
#include "current.h"
#include "state.h"
#include "motor.h"
#include "hall.h"

const int PIN_ACS = 34;

const float VREF = 3.3;
const int   ADC_MAX = 4095;
const float DIVIDER_GAIN = 1.5;

// Ajusta según módulo ACS712 (5A=0.185, 20A=0.100, 30A=0.066)
const float SENS_V_PER_A = 0.100; 

// Límite por defecto si no hay nada guardado
static float LIMIT_A = 8.0;
static uint8_t overCount = 0;     // cuántas veces seguidas superó el límite
static const uint8_t REQUIRED_OVER = 20;  // cuántas lecturas consecutivas  

static float vZero = 0.0;
static Preferences prefs;   // instancia de NVS

static unsigned long lastRecalMs = 0;
static const unsigned long RECAL_INTERVAL_MS = 6UL * 60UL * 60UL * 1000UL;  // 6 horas
static const unsigned long MIN_IDLE_FOR_RECAL_MS = 60UL * 1000UL;           // 1 minuto detenido

static void calibrate_offset() {
  const int N = 200;
  uint32_t acc = 0;
  for (int i = 0; i < N; ++i) {
    acc += analogRead(PIN_ACS);
    delay(2);
  }
  float adcMean = acc / float(N);
  float vAdc = (adcMean / ADC_MAX) * VREF;
  vZero = vAdc * DIVIDER_GAIN;
  Serial.printf("[CURRENT] Offset vZero=%.3f V\n", vZero);
}

void current_begin() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // --- Abrir NVS y leer el límite guardado ---
  prefs.begin("garage", false);  // namespace = "garage"
  float saved = prefs.getFloat("limitA", LIMIT_A);
  LIMIT_A = saved;
  Serial.printf("[CURRENT] Límite cargado: %.2f A\n", LIMIT_A);

  delay(50);

  // Calibrar offset con motor parado
  calibrate_offset();
  lastRecalMs = millis();
}

float current_readA() {
  const int N = 32;
  uint32_t acc = 0;
  for (int i = 0; i < N; ++i) acc += analogRead(PIN_ACS);
  float adcMean = acc / float(N);
  float vAdc = (adcMean / ADC_MAX) * VREF;
  float vOut = vAdc * DIVIDER_GAIN;
  float delta = vOut - vZero;
  float amps  = delta / SENS_V_PER_A;
  return fabs(amps);
}

bool current_guard_stop_if_over() {
  static uint8_t overCount = 0;  // 👈 asegurate de que sea estática
  float Ia = current_readA();
  float limit = LIMIT_A;

  if (motor_isSlowMode()) {
    limit *= 0.8;  // reduce el límite en modo lento (ajustable)
  }

  if (Ia > limit) {
    overCount++;
    if (overCount >= REQUIRED_OVER) {
      EstadoPuerta eNow = getEstado();  // 👈 saber si estaba cerrando o abriendo

      if (eNow == CERRANDO) {
        if (hall_is_near_closed()) {
          setEstado(DETENIDO);
          hall_mark_closed();
          Serial.printf("¡Tope físico detectado al cerrar! Resincronizando a 0. I=%.2f A\n", Ia);
        } else {
          setEstado(OBSTACULO);   // retroceder si no estamos cerca del final
          Serial.printf("¡OBSTÁCULO al cerrar! I=%.2f A (lim=%.2f)\n", Ia, limit);
        }
      } else {
        if (hall_is_near_open()) {
          setEstado(DETENIDO);
          hall_mark_open();
          Serial.printf("¡Tope físico detectado al abrir! Resincronizando final. I=%.2f A\n", Ia);
        } else {
          setEstado(DETENIDO);    // simplemente parar si estaba abriendo lejos del final
          Serial.printf("¡CORTE al abrir! I=%.2f A (lim=%.2f)\n", Ia, limit);
        }
      }

      overCount = 0;
      return true;
    }
  } else {
    overCount = 0;
  }

  return false;
}


// --- Nuevo: guardar límite en NVS cuando cambia ---
void current_set_limit(float amps) {
  LIMIT_A = amps;
  prefs.putFloat("limitA", LIMIT_A);  // guardar en flash
  Serial.printf("[CURRENT] Nuevo límite guardado: %.2f A\n", LIMIT_A);
}

float current_get_limit() { return LIMIT_A; }

void current_tick(unsigned long ahoraMs) {
  static bool estabaDetenido = false;
  static unsigned long detenidoDesde = 0;

  bool estaDetenido = (getEstado() == DETENIDO);

  if (!estaDetenido) {
    estabaDetenido = false;
    return;
  }

  if (!estabaDetenido) {
    estabaDetenido = true;
    detenidoDesde = ahoraMs;
  }

  if ((ahoraMs - lastRecalMs) < RECAL_INTERVAL_MS)
    return;
  if ((ahoraMs - detenidoDesde) < MIN_IDLE_FOR_RECAL_MS)
    return;

  calibrate_offset();
  lastRecalMs = ahoraMs;
}
