#include "display.h"
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include "config.h"

// Hardware MAX7219 (ajusta si usas otro)
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 1
#define DATA_PIN   23
#define CLK_PIN    18
#define CS_PIN     5

static MD_Parola display = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

void display_begin() {
  display.begin();
  display.setIntensity(PAROLA_BRIGHTNESS);
  display.displayClear();
}

void renderEstado(EstadoPuerta e) {
  switch (e) {
    case ABRIENDO:
      display.displayScroll("ABRIENDO", PA_LEFT, PA_SCROLL_LEFT, 100);
      break;
    case ABIERTO:
      display.displayText("ABIERTO", PA_CENTER, 100, 2000, PA_PRINT, PA_NO_EFFECT);
      break;
    case CERRANDO:
      display.displayScroll("CERRANDO", PA_LEFT, PA_SCROLL_LEFT, 100);
      break;
    case CERRADO:
      display.displayText("CERRADO", PA_CENTER, 100, 2000, PA_PRINT, PA_NO_EFFECT);
      break;
  }
  display.displayReset();
}

void display_tick() {
  display.displayAnimate();
}