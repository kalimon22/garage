#pragma once
#include <stdint.h>

enum EstadoPuerta : uint8_t { ABRIENDO, ABIERTO, CERRANDO, CERRADO, DETENIDO };

void display_begin();
void renderEstado(EstadoPuerta e);
void display_tick();  // debe llamarse en loop()