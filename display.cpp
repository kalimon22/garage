#include "display.h"
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include "config.h"

// ---- Ajusta a tu hardware MAX7219 ----
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES   1
#define DATA_PIN      23
#define CLK_PIN       18
#define CS_PIN        5

static MD_Parola display = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// ---- Estado interno del módulo display ----
static EstadoPuerta s_estado = DETENIDO;
static bool   s_repeat     = false;        // repetir scroll (solo ABRIENDO/CERRANDO)
static bool   s_inPause    = false;        // estamos en pausa entre pasadas
static uint32_t s_pauseUntil = 0;          // millis cuando termina la pausa
static const uint16_t SCROLL_SPEED     = 100;   // velocidad del scroll (Parola)
static const uint32_t REPEAT_PAUSE_MS  = 2000;  // 2 s entre pasadas
static const uint32_t DETENIDO_MS      = 3000;  // 3 s del icono detenido
static char   s_msg[16];                        // texto a scrollear

// ---- Icono 8x8: círculo con “X” (DETENIDO) ----
// Cada byte es una FILA, bit más significativo = columna 0 (izquierda)
static const uint8_t ICONO_DETENIDO[8] = {
  0b00111100,  //  ####
  0b01000010,  // #    #
  0b01011010,  // # ## #
  0b01100110,  // ##  ##
  0b01100110,  // ##  ##
  0b01011010,  // # ## #
  0b01000010,  // #    #
  0b00111100   //  ####
};

static const char* estadoToText(EstadoPuerta e)
{
  switch (e) {
    case ABRIENDO: return "ABRIENDO";
    case CERRANDO: return "CERRANDO";
    case DETENIDO: return "DETENIDO";
    case OBSTACULO: return "!";
  }
  return "?";
}

// Dibuja un bitmap 8x8 usando setPoint() (compatible con MD_MAX72XX)
static void drawBitmap8x8(const uint8_t bmp[8])
{
  display.getGraphicObject()->clear();  // limpia el módulo
  MD_MAX72XX* mx = display.getGraphicObject();
  for (uint8_t y = 0; y < 8; y++)
  {
    uint8_t row = bmp[y];
    for (uint8_t x = 0; x < 8; x++)
    {
      bool on = (row & (0x80 >> x)) != 0; // MSB = columna 0
      mx->setPoint(y, x, on);             // (fila, columna)
    }
  }
}

static void startScroll()
{
  display.displayScroll(s_msg, PA_LEFT, PA_SCROLL_LEFT, SCROLL_SPEED);
  display.displayReset();
}

// ---------- API pública ----------
void display_begin()
{
  display.begin();
  display.setIntensity(PAROLA_BRIGHTNESS);
  display.displayClear();

  // Estado inicial (por si nadie llama aún a renderEstado)
  s_estado = DETENIDO;
  s_repeat = false;
  s_inPause = false;
  // No mostramos nada aquí; lo mostrará renderEstado() cuando tu lógica fije el estado
}

void renderEstado(EstadoPuerta e)
{
  s_estado = e;
  // Preparamos el mensaje de scroll para estados con texto
  strncpy(s_msg, estadoToText(e), sizeof(s_msg) - 1);
  s_msg[sizeof(s_msg) - 1] = '\0';

  display.displayClear();
  s_inPause = false;

  if (e == DETENIDO)
  {
    // Icono de “detenido” durante 3 s, sin scroll
    drawBitmap8x8(ICONO_DETENIDO);
    s_inPause = true;
    s_pauseUntil = millis() + DETENIDO_MS;
    s_repeat = false;
    return;
  }

  if (e == ABRIENDO || e == CERRANDO)
  {
    // Scroll repetido con pausa entre pasadas
    s_repeat = true;
    startScroll();
  }
  else
  {
    // ABIERTO / CERRADO: un scroll y listo
    s_repeat = false;
    startScroll();
  }
}

void display_tick()
{
  // Caso especial: icono DETENIDO mostrado 3 s
  if (s_estado == DETENIDO)
  {
    if (s_inPause && (int32_t)(millis() - s_pauseUntil) >= 0)
    {
      display.displayClear(); // tras 3 s, limpia (si quieres que quede fijo, comenta esta línea)
      s_inPause = false;
    }
    return; // nada más que hacer en este estado
  }

  // Para estados con scroll:
  if (display.displayAnimate())  // true cuando termina el efecto actual
  {
    if (s_repeat)
    {
      // ABRIENDO/CERRANDO: tras acabar una pasada, esperamos 2 s y relanzamos
      if (!s_inPause)
      {
        s_inPause = true;
        s_pauseUntil = millis() + REPEAT_PAUSE_MS;
      }
    }
    else
    {
      // ABIERTO/CERRADO: no relanzamos (scroll único)
    }
  }

  // Si estamos en la pausa entre pasadas, relanzar cuando toque
  if (s_inPause && (int32_t)(millis() - s_pauseUntil) >= 0)
  {
    s_inPause = false;
    startScroll();
  }
}
