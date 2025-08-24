#pragma once
#include <Arduino.h>

void  hall_begin();
long  hall_get_count();        // pulsos acumulados (relativo)
int   hall_get_dir();          // +1 = abre, -1 = cierra, 0 = parado/indef.
void  hall_reset();            // pone el contador a 0
void  hall_tick(unsigned long now);
