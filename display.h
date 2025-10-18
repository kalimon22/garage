#pragma once
#include <stdint.h>

enum EstadoPuerta : uint8_t { ABRIENDO, CERRANDO,  DETENIDO, OBSTACULO };

void display_begin();
void renderEstado(EstadoPuerta e);
void display_tick();  // debe llamarse en loop()