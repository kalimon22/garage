#pragma once
#include <Arduino.h>

// Inicialización (resetea estados internos de seguridad)
void safety_begin();

// Tick periódico (llamar en loop principal con millis())
void safety_tick(uint32_t now);
