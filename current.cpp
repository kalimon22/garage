#include <Arduino.h>
#include <Preferences.h>
#include "current.h"
#include "state.h"

const int PIN_ACS = 34;

const float VREF = 3.3;
const int   ADC_MAX = 4095;
const float DIVIDER_GAIN = 1.5;

// Ajusta según módulo ACS712 (5A=0.185, 20A=0.100, 30A=0.066)
const float SENS_V_PER_A = 0.100; 

// Límite por defecto si no hay nada guardado
static float LIMIT_A = 8.0;  

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
  float Ia = current_readA();
  if (Ia > LIMIT_A) {
    setEstado(DETENIDO);  // setEstado se encarga de parar motor y publicar
    Serial.printf("¡CORTE por sobrecorriente! I=%.2f A (límite=%.2f)\n", Ia, LIMIT_A);
    return true;
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
