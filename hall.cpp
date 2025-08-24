#include "hall.h"
#include "config.h"
#include "state.h"   // getEstado, setEstado

static volatile long encCount = 0;
static volatile int  encDir   = 0;    // +1 abrir, -1 cerrar
static uint8_t lastAB = 0;

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
  pinMode(HALL_A_PIN, INPUT); // si el sensor es 5V, usa divisores a 3.3V
  pinMode(HALL_B_PIN, INPUT);
  lastAB = ((uint8_t)digitalRead(HALL_A_PIN) << 1) | (uint8_t)digitalRead(HALL_B_PIN);
  attachInterrupt(digitalPinToInterrupt(HALL_A_PIN), isr_update, CHANGE);
  attachInterrupt(digitalPinToInterrupt(HALL_B_PIN), isr_update, CHANGE);
}

long hall_get_count() { return encCount; }
int  hall_get_dir()   { return encDir; }
void hall_mark_closed() { encCount = 0; }

// Parada local por finales "virtuales" de Hall, SIN depender de WiFi/MQTT.
void hall_tick(unsigned long now) {
  static unsigned long tLastStop = 0;

  // Protege contra paradas repetidas seguidas (rebote mecánico)
  auto tryStop = [&](const char* why){
    if (now - tLastStop >= HALL_STOP_DEBOUNCE_MS) {
      setEstado(DETENIDO);  // state.cpp parará el motor y publicará estado
      tLastStop = now;
    }
  };

  long c = hall_get_count();
  switch (getEstado()) {
    case ABRIENDO:
      if (c >= HALL_OPEN_PULSES) {
        tryStop("[HALL] tope de ABIERTO");
      }
      break;
    case CERRANDO:
      // Referencia: 0 es CERRADO. Si te pasas a negativo por juego/ruido, también paras.
      if (c <= 0) {
        tryStop("[HALL] tope de CERRADO");
        hall_mark_closed(); // asegúrate de quedarte en 0
      }
      break;
    default:
      // En detenido/abierto/cerrado no forzamos nada.
      break;
  }
}
