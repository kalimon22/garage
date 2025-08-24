#include <Arduino.h>
#include <Preferences.h>
#include "hall.h"
#include "config.h"
#include "state.h"
#include "motor.h"

static Preferences prefsHall;

static volatile long encCount = 0;
static volatile int  encDir   = 0;    // +1 abrir, -1 cerrar
static uint8_t lastAB = 0;

// valor configurable en NVS
long hall_open_pulses = HALL_OPEN_PULSES_DEFAULT;

// Persistencia del último final de carrera alcanzado
static const char* NVS_NS_HALL   = "hall";
static const char* KEY_OPEN_PLS  = "open_pulses";
static const char* KEY_LAST_END  = "last_end";

enum LastEndstop : uint8_t { END_UNKNOWN=0, END_CLOSED=1, END_OPEN=2 };

IRAM_ATTR static void isr_update() {
  uint8_t a = (uint8_t)digitalRead(HALL_A_PIN);
  uint8_t b = (uint8_t)digitalRead(HALL_B_PIN);
  uint8_t ab = (a << 1) | b;

  int delta = 0;
  uint8_t prev = lastAB;
  if (prev == 0b00) { if (ab == 0b01) delta = +1; else if (ab == 0b10) delta = -1; }
  else if (prev == 0b01) { if (ab == 0b11) delta = +1; else if (ab == 0b00) delta = -1; }
  else if (prev == 0b11) { if (ab == 0b10) delta = +1; else if (ab == 0b01) delta = -1; }
  else if (prev == 0b10) { if (ab == 0b00) delta = +1; else if (ab == 0b11) delta = -1; }

  if (delta != 0) {
    encCount += delta;
    encDir = (delta > 0) ? +1 : -1;
    lastAB = ab;
  } else {
    lastAB = ab;
  }
}

void hall_begin() {
  prefsHall.begin(NVS_NS_HALL, false);
  hall_open_pulses = prefsHall.getLong(KEY_OPEN_PLS, HALL_OPEN_PULSES_DEFAULT);

  // Si el último estado fue un final fiable, restaura la posición al extremo
  uint8_t lastEnd = prefsHall.getUChar(KEY_LAST_END, END_UNKNOWN);
  if (lastEnd == END_CLOSED)      encCount = 0;
  else if (lastEnd == END_OPEN)   encCount = hall_open_pulses;
  // else UNKNOWN -> dejamos encCount en 0 por seguridad, pero no lo consideramos fiable

  pinMode(HALL_A_PIN, INPUT); // si el sensor es 5V, usa divisores a 3.3V
  pinMode(HALL_B_PIN, INPUT);
  lastAB = ((uint8_t)digitalRead(HALL_A_PIN) << 1) | (uint8_t)digitalRead(HALL_B_PIN);
  attachInterrupt(digitalPinToInterrupt(HALL_A_PIN), isr_update, CHANGE);
  attachInterrupt(digitalPinToInterrupt(HALL_B_PIN), isr_update, CHANGE);
}

long hall_get_count() { return encCount; }
int  hall_get_dir()   { return encDir; }

void hall_mark_closed() {
  encCount = 0;
  // Guardar que el último final alcanzado es CERRADO
  prefsHall.putUChar(KEY_LAST_END, END_CLOSED);
}

void hall_mark_open() {
  // Tomar la posición actual como límite superior y persistir
  hall_open_pulses = encCount;
  prefsHall.putLong(KEY_OPEN_PLS, hall_open_pulses);
  // Guardar que el último final alcanzado es ABIERTO
  prefsHall.putUChar(KEY_LAST_END, END_OPEN);
}

// Parada local por finales "virtuales" de Hall, SIN depender de WiFi/MQTT.
// Además activa modo lento al inicio y al final del recorrido.
void hall_tick(unsigned long now) {
  static unsigned long tLastStop = 0;

  auto tryStop = [&](const char* /*why*/){
    if (now - tLastStop >= HALL_STOP_DEBOUNCE_MS) {
      setEstado(DETENIDO);
      tLastStop = now;

      // Si paramos en extremos, persistimos "last_endstop"
      if (encCount <= 0) {
        prefsHall.putUChar(KEY_LAST_END, END_CLOSED);
        encCount = 0; // clamp
      } else if (encCount >= hall_open_pulses) {
        prefsHall.putUChar(KEY_LAST_END, END_OPEN);
        encCount = hall_open_pulses; // clamp
      }
    }
  };

  long c = hall_get_count();
  const long total = hall_open_pulses;
  const long slowThreshPulses = (total * HALL_SLOWDOWN_THRESHOLD_PERCENT) / 100;

  auto nearEitherEnd = [&](long pos){
    long distToClosed = pos;          // distancia a 0
    long distToOpen   = total - pos;  // distancia a total
    long dmin = (distToClosed < distToOpen) ? distToClosed : distToOpen;
    return dmin <= slowThreshPulses;
  };

  switch (getEstado()) {
    case ABRIENDO: {
      bool inSlow = nearEitherEnd(c);
      motor_set_slow(inSlow);

      if (c >= total) {
        tryStop("[HALL] tope de ABIERTO");
      }
    } break;

    case CERRANDO: {
      bool inSlow = nearEitherEnd(c);
      motor_set_slow(inSlow);

      if (c <= 0) {
        tryStop("[HALL] tope de CERRADO");
        hall_mark_closed(); // asegúrate de quedarte en 0
      }
    } break;

    default:
      motor_set_slow(false);
      break;
  }
}
