#include "config.h"
#include "display.h"
#include "state.h"
#include "net.h"
#include "ota_ctl.h"
#include "logx.h"

static unsigned long tUltimoCambio = 0;

void setup() {
  // La Serial se inicia dentro de net_begin() para centralizar logs
  net_begin();
  motor_setup();

  display_begin();
  renderEstado(getEstado());

  if (BOOT_OTA_MINUTES > 0) ota_enable_for(BOOT_OTA_MINUTES);
}

void loop() {
  // Animación del display
  display_tick();

  // Demo de rotación de estados
  #if (INTERVALO_ESTADO_MS > 0)
  unsigned long ahora = millis();
  if (ahora - tUltimoCambio >= INTERVALO_ESTADO_MS) {
    tUltimoCambio = ahora;
    EstadoPuerta e = static_cast<EstadoPuerta>((getEstado() + 1) % 4);
    setEstado(e);
  }
  #endif

  // Red (Wi-Fi/MQTT)
  net_tick();

  // OTA on-demand
  ota_tick();
}