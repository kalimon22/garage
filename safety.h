#pragma once
#include <Arduino.h>

// Inicialización (si necesitas resetear estados)
void safety_begin();

// Llamar periódicamente desde loop()
void safety_tick(uint32_t now);
