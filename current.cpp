#include <Arduino.h>
#include <Preferences.h>
#include "current.h"
#include "state.h"
#include "motor.h"

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
        setEstado(OBSTACULO);   // 👈 activar el estado de retroceso
        Serial.printf("¡CORTE al cerrar! I=%.2f A (lim=%.2f)\n", Ia, limit);
      } else {
        setEstado(DETENIDO);    // 👈 solo parar si estaba abriendo
        Serial.printf("¡CORTE al abrir! I=%.2f A (lim=%.2f)\n", Ia, limit);
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
