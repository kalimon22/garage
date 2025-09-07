#include <Arduino.h>
#include <Preferences.h>
#include "hall.h"
#include "config.h"
#include "state.h"
#include "motor.h"

static Preferences prefsHall;

static bool hall_enabled = (HALL_ENABLED_DEFAULT != 0);

void hall_set_enabled(bool on) { hall_enabled = on; }
bool hall_is_enabled()         { return hall_enabled; }

static volatile long encCount = 0;
static volatile int  encDir   = 0;    // +1 abrir, -1 cerrar

// valor configurable en NVS
long hall_open_pulses = HALL_OPEN_PULSES_DEFAULT;

// Persistencia del último final de carrera alcanzado
static const char* NVS_NS_HALL   = "hall";
static const char* KEY_OPEN_PLS  = "open_pulses";
static const char* KEY_LAST_END  = "last_end";

enum LastEndstop : uint8_t { END_UNKNOWN=0, END_CLOSED=1, END_OPEN=2 };

// ISR para el pin de PULSOS
IRAM_ATTR static void isr_pulse() {
  int dirLevel = digitalRead(HALL_DIR_PIN);
#if HALL_DIR_ACTIVE_HIGH_CLOSE
  encDir = (dirLevel ? -1 : +1);
#else
  encDir = (dirLevel ? +1 : -1);
#endif
  encCount += encDir;
}

void hall_begin() {
  prefsHall.begin(NVS_NS_HALL, false);
  hall_open_pulses = prefsHall.getLong(KEY_OPEN_PLS, HALL_OPEN_PULSES_DEFAULT);

  // Restaurar último extremo si lo había
  uint8_t lastEnd = prefsHall.getUChar(KEY_LAST_END, END_UNKNOWN);
  if (lastEnd == END_CLOSED)      encCount = 0;
  else if (lastEnd == END_OPEN)   encCount = hall_open_pulses;

  pinMode(HALL_PULSE_PIN, INPUT);
  pinMode(HALL_DIR_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(HALL_PULSE_PIN), isr_pulse, RISING);
}

long hall_get_count() { return encCount; }
int  hall_get_dir()   { return encDir; }

void hall_mark_closed() {
  encCount = 0;
  prefsHall.putUChar(KEY_LAST_END, END_CLOSED);
}

void hall_mark_open() {
  hall_open_pulses = encCount;
  prefsHall.putLong(KEY_OPEN_PLS, hall_open_pulses);
  prefsHall.putUChar(KEY_LAST_END, END_OPEN);
}

void hall_tick(unsigned long now) {
  static unsigned long tLastStop = 0;

  auto tryStop = [&](const char* /*why*/){
    if (now - tLastStop >= HALL_STOP_DEBOUNCE_MS) {
      setEstado(DETENIDO);
      tLastStop = now;

      if (encCount <= 0) {
        prefsHall.putUChar(KEY_LAST_END, END_CLOSED);
        encCount = 0;
      } else if (encCount >= hall_open_pulses) {
        prefsHall.putUChar(KEY_LAST_END, END_OPEN);
        encCount = hall_open_pulses;
      }
    }
  };

  long c = hall_get_count();
  const long total = hall_open_pulses;
  const long slowThreshPulses = (total * HALL_SLOWDOWN_THRESHOLD_PERCENT) / 100;

  auto nearEitherEnd = [&](long pos){
    long distToClosed = pos;
    long distToOpen   = total - pos;
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
        hall_mark_closed();
      }
    } break;

    default:
      motor_set_slow(false);
      break;
  }
}
