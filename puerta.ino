#include "config.h"
#include "display.h"
#include "state.h"
#include "net.h"
#include "ota_ctl.h"
#include "logx.h"
#include "motor.h"
#include "current.h"
#include "hall.h"
#include "light.h"
#include "safety.h"

static unsigned long tUltimoCambio = 0;


void handleButton(unsigned long ahora) {
  static uint32_t tLastRead = 0;
  static int lastStable = (BUTTON_ACTIVE_LVL == LOW) ? HIGH : LOW;
  static int lastRead   = lastStable;

  // +1 = veníamos de ABRIENDO; -1 = veníamos de CERRANDO
  // Inicia en +1 para que desde DETENIDO el primer pulso vaya a CERRANDO (como pediste).
  static int lastDir = +1;

  int raw = digitalRead(BUTTON_PIN);
  if (raw != lastRead) { lastRead = raw; tLastRead = ahora; }

  if ((ahora - tLastRead) >= BUTTON_DEBOUNCE_MS) {
    if (lastStable != raw) {
      lastStable = raw;
      bool pressed = (raw == BUTTON_ACTIVE_LVL);
      if (pressed) {
        EstadoPuerta e = getEstado();
        switch (e) {
          case ABRIENDO:
            setEstado(DETENIDO);
            lastDir = +1;
            break;

          case CERRANDO:
            setEstado(DETENIDO);
            lastDir = -1;
            break;

          case DETENIDO:
            if (lastDir == +1) {         // veníamos de abrir → ahora cerramos
              setEstado(CERRANDO);
              lastDir = -1;
            } else {                      // veníamos de cerrar → ahora abrimos
              setEstado(ABRIENDO);
              lastDir = +1;
            }
            break;

          case ABIERTO:                   // si estás abierto y pulsas: cierra
            setEstado(CERRANDO);
            lastDir = -1;
            break;

          case CERRADO:                   // si estás cerrado y pulsas: abre
            setEstado(ABRIENDO);
            lastDir = +1;
            break;
        }
        logPrintf("[BTN] Press -> nuevo estado: %d (lastDir=%d)\n", (int)getEstado(), lastDir);
      }
    }
  }
}


void setup() {
  net_begin();
  motor_begin();     // <- importante
  display_begin();
  current_begin(); 
  light_begin(); 
  safety_begin();
  setEstado(DETENIDO);
  renderEstado(getEstado());
  pinMode(BUTTON_PIN, (BUTTON_ACTIVE_LVL == LOW) ? INPUT_PULLUP : INPUT);
  // Encoder Hall
  hall_begin();
  if (BOOT_OTA_MINUTES > 0) ota_enable_for(BOOT_OTA_MINUTES);
}

void loop() {
  unsigned long ahora = millis();

  // 1) Animación del display
  display_tick();

  // 2) Demo de rotación de estados (si INTERVALO_ESTADO_MS > 0)
  #if (INTERVALO_ESTADO_MS > 0)
  if (ahora - tUltimoCambio >= INTERVALO_ESTADO_MS) {
    tUltimoCambio = ahora;
    EstadoPuerta e = static_cast<EstadoPuerta>((getEstado() + 1) % 4);
    setEstado(e);
  }
  #endif

    // Botón local siempre
  handleButton(ahora);
  light_tick(ahora);
  safety_tick(ahora); 


  // Hall (aplica o no según flag)
  if (hall_is_enabled()) {
    hall_tick(ahora);
  }

  // 4) Red (Wi-Fi/MQTT)
  net_tick();

  // 5) OTA on-demand
  ota_tick();
}
