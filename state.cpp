#include "state.h"
#include "net.h"
#include "config.h"
#include "motor.h"

static EstadoPuerta estadoActual = DETENIDO;

static const char* estadoToText(EstadoPuerta e) {
  switch (e) {
    case ABRIENDO: return "ABRIENDO";
    case ABIERTO:  return "ABIERTO";
    case CERRANDO: return "CERRANDO";
    case CERRADO:  return "CERRADO";
    case DETENIDO: return "DETENIDO";
  }
  return "?";
}

EstadoPuerta getEstado() { return estadoActual; }

void setEstado(EstadoPuerta e) {
  if (estadoActual == e) return;
  estadoActual = e;

  // Acciones físicas sobre el motor según estado
  switch (estadoActual) {
    case ABRIENDO: 
      motorOpen(); 
      break;
    case CERRANDO: 
      motorClose(); 
      break;
    case DETENIDO:
      motorStop(); 
      break;
    case ABIERTO:
      motorStop(); 
      break;
    case CERRADO: 
      motorStop(); 
      break;
  }

  // Renderizar y publicar MQTT
  renderEstado(estadoActual);
  net_mqtt_publish(TOPIC_STATE, String(estadoToText(estadoActual)), true);
}
