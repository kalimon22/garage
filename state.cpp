#include "state.h"
#include "net.h"
#include "config.h"

static EstadoPuerta estadoActual = ABRIENDO;

static const char* estadoToText(EstadoPuerta e) {
  switch (e) {
    case ABRIENDO: return "ABRIENDO";
    case ABIERTO:  return "ABIERTO";
    case CERRANDO: return "CERRANDO";
    case CERRADO:  return "CERRADO";
  }
  return "?";
}

EstadoPuerta getEstado() { return estadoActual; }

void setEstado(EstadoPuerta e) {
  if (estadoActual == e) return;
  estadoActual = e;
  renderEstado(estadoActual);
  net_mqtt_publish(TOPIC_STATE, String(estadoToText(estadoActual)), true); // retained
}